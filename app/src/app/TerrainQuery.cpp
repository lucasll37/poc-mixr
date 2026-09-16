#include "app/TerrainQuery.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/terrain/Terrain.hpp"
#include "mixr/terrain/srtm/SrtmHgtFile.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/osg/Vec3d"
#include "mixr/base/util/nav_utils.hpp"

#include <cctype>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace app {

namespace {

// Mesmo diretorio que main.cpp usa pra 'ensureAllTerrainTiles()' (ver
// app/TerrainData.hpp) -- literal repetido de proposito (mesmo raciocinio
// de 'terrainDir'/'terrainTile' ja duplicados em cada main.cpp das quatro
// pocs deste repositorio: copia pequena, nao vale uma abstracao pra
// atravessar so essa fronteira).
const char* const kTerrainDir{"./shared/data/terrain/srtm/"};

// SrtmHgtFile marca "sem dado" (void, buraco no tile) com um valor de
// SENTINELA em torno de -32767/-32768 -- e DataFile::getElevation() (lido
// em contexts/src/mixr/src/terrain/DataFile.cpp antes de escrever isto)
// devolve esse valor como se fosse uma elevacao REAL (nao filtra void
// nenhum, so retorna 'false' se o ponto cair FORA da caixa do tile). Sem
// este filtro, um void vira "elevacao -32768 m" na vista Lateral -- a
// linha de contorno despencava pro fundo do canvas (o pedido que motivou
// isto: "hoje esta algo em torno de -32k, nao faz sentido"). Qualquer
// ponto da Terra de verdade fica acima disto por larga margem (o mais
// fundo, o Mar Morto, e so uns -430 m) -- tratado como void e descartado
// (a mesma semantica de "false" que ja usamos pra "fora de todo tile").
const double kMinPlausibleElevM{-1000.0};

// TETO DE TILES RESIDENTES EM MEMORIA -- a razao de este arquivo ter
// deixado de carregar tudo de uma vez.
//
// O desenho anterior carregava TODO '.hgt' da pasta na primeira consulta e
// nunca liberava nenhum. Isso funcionava porque a pasta tinha 4 tiles; com
// a cobertura do Brasil inteira em disco (~1600 tiles) a MESMA linha pediria
// 1600 x 25,9 MB = ~41,5 GB de RAM na primeira vez que alguem abrisse a aba
// Mapa -- ou seja, o programa morreria de OOM, sem aviso, so por causa de
// arquivos parados numa pasta que ele nem precisava ler.
//
// 12 tiles SRTM1 residentes sao ~311 MB. O padrao de acesso do Mapa e
// fortemente local (uma janela de poucas dezenas de km, milhares de
// amostras por redesenho, todas nos mesmos 1-4 tiles), entao o LRU acerta
// quase sempre e a troca so acontece ao arrastar o mapa para longe.
const std::size_t kMaxResidentTiles{12};

// Um tile CONHECIDO (indexado pelo nome do arquivo) -- carregado ou nao.
//
// 'terrain' e um ponteiro CRU, nao base::safe_ptr: safe_ptr<T> nao e
// copiavel/movivel do jeito que os contentores da STL precisam (o
// construtor de copia pede 'safe_ptr<T>&' nao-const e nao ha construtor de
// mover). A posse e explicita e local a este arquivo: quem carrega chama
// 'new', quem despeja chama 'unref()'.
struct TileEntry
{
   double swLat{};
   double swLon{};
   std::string hgtName;                          // "S23W043.hgt"
   bool hasGz{};                                 // existe "<nome>.hgt.gz"?
   mixr::terrain::Terrain* terrain{nullptr};     // nulo = ainda nao carregado
   bool loadFailed{false};                       // ja tentou e falhou; nao repetir
   bool pendingLoad{false};                      // ja enfileirado pra thread de carga -- nao duplicar
};

// Chave do indice: o canto SW em graus INTEIROS. Um std::map (e nao a
// varredura linear de antes) porque a aba Mapa faz ~3200 consultas por
// redesenho: com 1600 tiles, a varredura custaria ~5,1 milhoes de testes
// de caixa por quadro; a busca ordenada custa ~11 comparacoes por consulta.
using Cell = std::pair<int, int>;

// Extrai o canto SW do NOME do arquivo -- mesma convencao que
// SrtmHgtFile::determineSrtmInfo() usa por dentro (ultimos 11 caracteres,
// "[NS]DD[EW]DDD.hgt"), repetida aqui so pra ESCOLHER qual tile cobre um
// ponto, nao pra ler o arquivo (isso quem faz e o proprio SrtmHgtFile).
bool parseSwCorner(const std::string& filename, double& swLat, double& swLon)
{
   if (filename.size() < 11) return false;
   const std::string tag{filename.substr(filename.size() - 11, 7)};
   const char ns{static_cast<char>(std::toupper(static_cast<unsigned char>(tag[0])))};
   const char ew{static_cast<char>(std::toupper(static_cast<unsigned char>(tag[3])))};
   if ((ns != 'N' && ns != 'S') || (ew != 'E' && ew != 'W')) return false;
   if (!std::isdigit(static_cast<unsigned char>(tag[1])) ||
       !std::isdigit(static_cast<unsigned char>(tag[2])) ||
       !std::isdigit(static_cast<unsigned char>(tag[4])) ||
       !std::isdigit(static_cast<unsigned char>(tag[5])) ||
       !std::isdigit(static_cast<unsigned char>(tag[6]))) {
      return false;
   }
   const int latDeg{std::stoi(tag.substr(1, 2))};
   const int lonDeg{std::stoi(tag.substr(4, 3))};
   swLat = (ns == 'S') ? -latDeg : latDeg;
   swLon = (ew == 'W') ? -lonDeg : lonDeg;
   return true;
}

// Serializa TODO o estado deste arquivo (indice, cache residente, LRU).
//
// A versao anterior documentava "so a thread de UI do FTXUI chama isto" e
// nao travava nada. Continua sendo verdade hoje, mas agora ha DESPEJO: uma
// segunda thread entrando no meio de uma troca acharia um ponteiro recem
// liberado, e o modo de falha seria um crash raro e dificil de reproduzir.
// O mutex custa nada na taxa em que isto roda (~10 Hz de redesenho) e tira
// a corretude de depender de um comentario.
std::mutex& terrainMutex()
{
   static std::mutex m;
   return m;
}

// INDICE: varre a pasta UMA vez e registra so NOME e canto SW de cada tile
// -- sem abrir nenhum arquivo de dado. Custa um readdir, independente de a
// pasta ter 5 tiles ou 1600.
std::map<Cell, TileEntry>& tileIndex()
{
   static std::map<Cell, TileEntry> index = [] {
      std::map<Cell, TileEntry> idx;
      std::error_code ec;
      if (!std::filesystem::exists(kTerrainDir, ec)) return idx;

      for (const auto& entry : std::filesystem::directory_iterator(kTerrainDir, ec)) {
         if (ec) break;
         if (!entry.is_regular_file(ec)) continue;

         const std::string name{entry.path().filename().string()};

         // Aceita tanto "<tile>.hgt" quanto "<tile>.hgt.gz": um tile que so
         // existe comprimido e valido, e sera descomprimido na primeira vez
         // que alguem consultar um ponto dentro dele (ver residentTerrain()).
         // Antes, um '.hgt.gz' sem par descomprimido era simplesmente
         // invisivel aqui, e quem descomprimia era ensureAllTerrainTiles(),
         // na partida, para a pasta INTEIRA.
         std::string base;
         bool gz{false};
         if (name.size() > 7 && name.compare(name.size() - 7, 7, ".hgt.gz") == 0) {
            base = name.substr(0, name.size() - 3);   // tira ".gz"
            gz = true;
         } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".hgt") == 0) {
            base = name;
         } else {
            continue;
         }

         double swLat{};
         double swLon{};
         if (!parseSwCorner(base, swLat, swLon)) continue;

         const Cell cell{static_cast<int>(swLat), static_cast<int>(swLon)};
         TileEntry& e{idx[cell]};
         e.swLat = swLat;
         e.swLon = swLon;
         e.hgtName = base;
         e.hasGz = e.hasGz || gz;
      }
      return idx;
   }();
   return index;
}

// Ordem de uso dos tiles residentes: frente = mais recente.
std::list<Cell>& lruOrder()
{
   static std::list<Cell> lru;
   return lru;
}

void touchLru(const Cell& cell)
{
   auto& lru{lruOrder()};
   for (auto it = lru.begin(); it != lru.end(); ++it) {
      if (*it == cell) { lru.erase(it); break; }
   }
   lru.push_front(cell);
}

void evictIfNeeded()
{
   auto& lru{lruOrder()};
   auto& idx{tileIndex()};
   while (lru.size() > kMaxResidentTiles) {
      const Cell victim{lru.back()};
      lru.pop_back();
      const auto it{idx.find(victim)};
      if (it == idx.end() || it->second.terrain == nullptr) continue;
      it->second.terrain->unref();   // unica referencia nossa -- destroi o tile
      it->second.terrain = nullptr;
   }
}

// ---------------------------------------------------------------------------
// CARGA EM BACKGROUND -- a razao de este bloco existir.
//
// Ate aqui, um tile ainda nao residente era carregado NA CHAMADA: gunzip
// (fork+exec de um processo externo) + 'new SrtmHgtFile' + reset() (que le
// e valida ~26 MB), tudo dentro de residentTerrain(), chamada pela THREAD
// DE DESENHO (o Renderer do FTXUI) enquanto ela ja segura terrainMutex().
// Com poucos tiles isso era barato (o comentario antigo dizia "getElevation()
// e indexacao de array, nao I/O por chamada" -- verdade so depois do
// primeiro acesso). Dar bastante zoom out faz o grid de amostragem (passo
// FIXO em pixel de canvas, ver kTerrainGridStepPx em MapPanel.cpp) cobrir
// uma area geografica muito maior -- com a cobertura completa do Brasil
// baixada (scripts/fetch_srtm.sh --brasil, ~1600 tiles, a maioria so em
// '.gz'), uma unica varredura passa a cruzar dezenas de tiles NOVOS, cada
// um disparando um gunzip sincrono na thread de desenho: a UI trava por
// segundos, redesenho apos redesenho, enquanto o zoom ficar nesse nivel.
//
// A simulacao (station->updateTC()/updateData(), a MESMA thread que precisa
// ser deterministica em '-deterministic') nunca chamou nada deste arquivo
// -- makeTerrainSampler() so e usado pela aba Mapa do './app' interativo
// (ver main.cpp). Ainda assim, o trabalho pesado NAO pode continuar na
// thread de desenho: uma thread NOVA, dedicada, e' quem carrega. A thread
// de desenho so PEDE (requestLoad(), rapido, so enfileira) e le o que ja
// estiver pronto -- nunca espera.
// ---------------------------------------------------------------------------

// Fila de pedidos de carga, e o sinal pra a thread worker acordar --
// mutex PROPRIO, separado de terrainMutex(): a thread de desenho pede com
// terrainMutex() ja travado (dentro de residentTerrain()), e travar dois
// mutexes na mesma ordem dos dois lados (nunca o inverso) evita deadlock.
std::mutex& loaderMutex()
{
   static std::mutex m;
   return m;
}

std::condition_variable& loaderCv()
{
   static std::condition_variable cv;
   return cv;
}

std::deque<Cell>& loaderQueue()
{
   static std::deque<Cell> q;
   return q;
}

// Guardado pelo MESMO 'loaderMutex()' da fila -- pedido de parada pra
// loaderThreadBody() sair do laco (ver shutdownTerrainLoader(), a razao
// de existir isto -- achado rodando, nao hipotetico).
bool& loaderStopRequested()
{
   static bool stop{false};
   return stop;
}

// O 'std::thread' em si -- default-construido (nao-joinable) ate
// ensureLoaderThreadStarted() de fato criar a thread. Guardado aqui (nao
// so' 'detach()'ado) porque shutdownTerrainLoader() precisa de um handle
// pra join().
std::thread& loaderThreadHandle()
{
   static std::thread t;
   return t;
}

// PRE-CONDICAO: terrainMutex() ja travado pelo chamador (so pra marcar
// 'pendingLoad' sem corrida com a propria thread worker). NAO faz I/O --
// so anexa a fila e devolve.
void requestLoad(const Cell& cell)
{
   {
      const std::lock_guard<std::mutex> lock{loaderMutex()};
      loaderQueue().push_back(cell);
   }
   loaderCv().notify_one();
}

// O trabalho de verdade -- chamado SO' pela thread worker, NUNCA pela
// thread de desenho. As duas fases que tocam 'terrainMutex()' sao curtas
// (leitura/escrita de campos, sem I/O); o meio -- gunzip + leitura/parse do
// '.hgt' -- roda FORA de qualquer mutex, pra nao bloquear consultas de
// OUTRAS celulas (ja residentes) enquanto isto roda.
void loadTileIntoCache(const Cell& cell)
{
   std::string hgtName;
   bool hasGz{};
   {
      const std::lock_guard<std::mutex> lock{terrainMutex()};
      const auto it{tileIndex().find(cell)};
      if (it == tileIndex().end()) return;
      TileEntry& e{it->second};
      // Ja resolvido por outro pedido enfileirado antes deste (a mesma
      // celula pode ter sido pedida por dezenas de amostras do MESMO
      // quadro, antes de 'pendingLoad' silenciar as seguintes -- ver
      // residentTerrain()) ou ja carregado enquanto este pedido esperava
      // na fila.
      if (e.terrain != nullptr || e.loadFailed) { e.pendingLoad = false; return; }
      hgtName = e.hgtName;
      hasGz = e.hasGz;
   }

   const std::string hgtPath{std::string{kTerrainDir} + hgtName};

   // SrtmHgtFile nao le '.gz' -- descomprime sob demanda, so este tile.
   // 'gunzip'/leitura/parse: o custo real, de proposito FORA do mutex.
   std::error_code ec;
   bool failed{false};
   if (!std::filesystem::exists(hgtPath, ec)) {
      if (!hasGz) {
         failed = true;
      } else {
         const std::string cmd{"gunzip -kf \"" + hgtPath + ".gz\""};
         if (std::system(cmd.c_str()) != 0 || !std::filesystem::exists(hgtPath, ec)) {
            std::cerr << "[terreno] falha ao descomprimir " << hgtPath << ".gz -- tile ignorado"
                      << std::endl;
            failed = true;
         }
      }
   }

   mixr::terrain::SrtmHgtFile* tile{nullptr};
   if (!failed) {
      tile = new mixr::terrain::SrtmHgtFile();
      auto* const pathStr{new mixr::base::String(kTerrainDir)};
      auto* const fileStr{new mixr::base::String(hgtName.c_str())};
      tile->setPathname(pathStr);
      tile->setFilename(fileStr);
      pathStr->unref();
      fileStr->unref();
      tile->reset();   // Terrain::reset() chama loadData() se ainda nao carregado

      if (!tile->isDataLoaded()) {
         tile->unref();
         tile = nullptr;
         failed = true;   // tamanho invalido/arquivo truncado: nao insistir
      }
   }

   // Instalar o resultado -- rapido de novo: so um ponteiro + contabilidade
   // de LRU, sob o MESMO mutex que a thread de desenho usa pra ler.
   const std::lock_guard<std::mutex> lock{terrainMutex()};
   const auto it{tileIndex().find(cell)};
   if (it == tileIndex().end()) { if (tile != nullptr) tile->unref(); return; }
   TileEntry& e{it->second};
   e.pendingLoad = false;
   if (failed) { e.loadFailed = true; return; }
   e.terrain = tile;
   touchLru(cell);
   evictIfNeeded();
}

void loaderThreadBody()
{
   for (;;) {
      Cell cell{};
      {
         std::unique_lock<std::mutex> lock{loaderMutex()};
         loaderCv().wait(lock, [] { return !loaderQueue().empty() || loaderStopRequested(); });
         // Parada ABANDONA o resto da fila, nunca a drena -- achado rodando:
         // dar bastante zoom out com uma cobertura regional de tiles no
         // disco (nao so os poucos de demonstracao) enfileira potencialmente
         // centenas de celulas distintas numa unica sessao de zoom, e
         // esperar essa fila inteira esvaziar no encerramento levava o
         // shutdownTerrainLoader() a demorar bem mais que o esperado. No
         // encerramento ninguem precisa mais do resultado de um tile que
         // ainda nem comecou a carregar.
         if (loaderStopRequested()) return;
         cell = loaderQueue().front();
         loaderQueue().pop_front();
      }
      loadTileIntoCache(cell);
   }
}

// Uma thread so, criada na PRIMEIRA vez que algum cenario com terreno abre
// a aba Mapa. NAO e' detach()ada -- ver shutdownTerrainLoader() logo
// abaixo e o "porque", achado rodando: uma thread detached parada num
// condition_variable::wait() para sempre TRAVA o exit() normal do
// processo, medido travando o encerramento por completo do './app'
// interativo depois do 'q' -- main() retorna 0 mas o processo nunca
// termina. std::thread joinavel + join() explicito no fim (app::
// shutdownTerrainLoader(), chamado por main.cpp) elimina isso.
void ensureLoaderThreadStarted()
{
   static const bool started = [] {
      loaderThreadHandle() = std::thread{loaderThreadBody};
      return true;
   }();
   (void)started;
}

// Devolve o Terrain do tile que cobre 'cell' -- SO se ja estiver
// residente. Nulo tanto para "nao ha tile ali" quanto para "ha tile, mas
// ainda esta' carregando em background" (o pedido ja foi feito, ou esta
// sendo feito agora): NUNCA bloqueia, nunca faz I/O -- e' por isso que
// pode continuar rodando na thread de desenho, chamada por milhares de
// amostras a cada redesenho.
// PRE-CONDICAO: terrainMutex() ja travado pelo chamador.
mixr::terrain::Terrain* residentTerrain(const Cell& cell)
{
   auto& idx{tileIndex()};
   const auto it{idx.find(cell)};
   if (it == idx.end()) return nullptr;

   TileEntry& e{it->second};
   if (e.terrain != nullptr) { touchLru(cell); return e.terrain; }
   if (e.loadFailed) return nullptr;

   if (!e.pendingLoad) {
      e.pendingLoad = true;
      requestLoad(cell);
   }
   return nullptr;
}

}   // namespace

TerrainSampler makeTerrainSampler(mixr::models::WorldModel* const worldModel)
{
   if (worldModel == nullptr) return {};

   {
      const std::lock_guard<std::mutex> lock{terrainMutex()};
      if (tileIndex().empty()) return {};
   }

   // So' sobe a thread worker se HOUVER algum tile no diretorio (o check
   // acima) -- sem terreno nenhum, nao ha o que carregar em background.
   ensureLoaderThreadStarted();

   const double refLat{worldModel->getRefLatitude()};
   const double refLon{worldModel->getRefLongitude()};

   return [refLat, refLon](const double northM, const double eastM,
                           double& elevM) -> bool {
      double lat{};
      double lon{};
      double alt{};
      const mixr::base::Vec3d pos{northM, eastM, 0.0};
      if (!mixr::base::nav::convertPosVec2llE(refLat, refLon, pos, &lat, &lon, &alt)) {
         return false;
      }

      // floor(), nao truncamento: para lat/lon NEGATIVOS (o caso deste
      // repositorio inteiro) static_cast<int>(-22.3) da -22, e o tile que
      // cobre -22,3 e o S23 (canto SW -23). Truncar escolheria o vizinho
      // errado em todo o hemisferio sul/oeste.
      const Cell cell{static_cast<int>(std::floor(lat)), static_cast<int>(std::floor(lon))};

      const std::lock_guard<std::mutex> lock{terrainMutex()};
      mixr::terrain::Terrain* const terrain{residentTerrain(cell)};
      if (terrain == nullptr) return false;

      double elev{};
      if (terrain->getElevation(&elev, lat, lon, true) && elev >= kMinPlausibleElevM) {
         elevM = elev;
         return true;
      }
      return false;
   };
}

void shutdownTerrainLoader()
{
   // No-op se a thread nunca foi criada -- '-deterministic' nunca chama
   // makeTerrainSampler(), e um cenario sem nenhum tile no diretorio
   // tambem nunca chega a criar (ver o early-return de makeTerrainSampler()
   // acima). 'joinable()' e' falso nos dois casos (thread default-
   // construida, nunca movida por cima).
   if (!loaderThreadHandle().joinable()) return;

   {
      const std::lock_guard<std::mutex> lock{loaderMutex()};
      loaderStopRequested() = true;
   }
   loaderCv().notify_all();
   loaderThreadHandle().join();
}

} // namespace app
