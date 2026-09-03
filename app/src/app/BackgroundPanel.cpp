#include "app/BackgroundPanel.hpp"

#include <iomanip>
#include <sstream>

namespace app {

namespace {
using namespace ftxui;

std::string fmt2(const double v)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(2) << v;
   return oss.str();
}

std::string fmt1(const double v)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(1) << v;
   return oss.str();
}

// Bytes em unidade legivel -- um contador de socket que passa de alguns MB
// vira ruido em digitos crus.
std::string humanBytes(const unsigned long bytes)
{
   std::ostringstream oss;
   oss << std::fixed << std::setprecision(1);
   if (bytes >= 1024UL * 1024UL) { oss << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB"; }
   else if (bytes >= 1024UL)     { oss << (static_cast<double>(bytes) / 1024.0) << " KiB"; }
   else                          { oss << bytes << " B"; }
   return oss.str();
}

// Larguras FIXAS de coluna -- mesmo raciocinio das listas das outras abas
// (app/FleetPanel.hpp): o rotulo e o valor tem de ficar alinhados linha a
// linha, e nao com a largura de cada conteudo.
const int kColKey{26};

// Largura de cada uma das duas colunas do painel -- ver o comentario em
// renderBackgroundPanel() sobre por que nao pode ser 'flex'. Cabe o rotulo
// (kColKey) mais o valor mais comprido que aparece de fato (o caminho do
// arquivo .acmi).
const int kColumnWidth{62};

Element kv(const std::string& k, const std::string& v, const Color valueColor = Color::Default)
{
   return hbox({
      text(k) | dim | size(WIDTH, EQUAL, kColKey),
      text(v) | color(valueColor),
   });
}

// Um indicador binario com vocabulario consistente em todo o painel: verde
// = esta como deveria, cinza = ausente por configuracao (nao e defeito),
// vermelho = quebrado.
Element flag(const std::string& k, const bool on, const std::string& yes, const std::string& no,
             const bool noIsBad = false)
{
   return kv(k, on ? yes : no, on ? Color::Green : (noIsBad ? Color::Red : Color::GrayDark));
}

Element section(const std::string& title, Elements rows)
{
   Elements body{text(title) | bold | color(Color::CyanLight)};
   for (auto& r : rows) body.push_back(std::move(r));
   body.push_back(text(""));
   return vbox(std::move(body));
}

// ---- as secoes, uma funcao cada -------------------------------------------

Element sectionLoop(const BackgroundInfo& bg)
{
   const bool slow{bg.measuredHz < static_cast<double>(bg.targetHz) * 0.8};
   return section("Laco de atualizacao (esta thread)", {
      kv("taxa alvo", std::to_string(bg.targetHz) + " Hz"),
      kv("taxa medida", fmt1(bg.measuredHz) + " Hz", slow ? Color::Yellow : Color::Green),
      kv("ultima iteracao", fmt1(bg.lastIterationMs) + " ms"),
      kv("iteracoes", std::to_string(bg.iterationCount)),
   });
}

// O outro lado da moeda: o que este laco NAO faz. Mostrar os contadores do
// executivo ao lado dos do laco e o que torna a distincao concreta -- o
// frame de tempo critico corre numa thread propria, criada por
// Station::createTimeCriticalProcess().
Element sectionExecutive(const BackgroundInfo& bg)
{
   std::ostringstream cfp;
   cfp << "ciclo " << bg.execCycle << "  frame " << bg.execFrame << "  fase " << bg.execPhase;

   Elements rows{
      kv("taxa T/C declarada", fmt1(bg.stationTcRateHz) + " Hz"),
      kv("taxa bg declarada", fmt1(bg.stationBgRateHz) + " Hz"),
      flag("thread T/C nativa", bg.tcThreadRunning, "rodando", "nao criada"),
      flag("thread bg nativa", bg.bgThreadRunning, "rodando",
           "nao criada -- este laco faz o papel"),
      kv("contadores", cfp.str()),
      kv("passos do executivo", std::to_string(bg.execCounter)),
   };
   if (bg.tcTimingAvailable) {
      // O unico numero desta aba medido DENTRO da outra thread -- ver a nota
      // sobre base::Statistic em BackgroundInfo.
      rows.push_back(kv("duracao do frame T/C",
                        fmt2(bg.tcFrameLastMs) + " ms  (media " + fmt2(bg.tcFrameMeanMs)
                        + ", pico " + fmt2(bg.tcFrameMaxMs) + ")"));
      rows.push_back(kv("amostras", std::to_string(bg.tcFrameSamples)));
   }
   if (bg.fastForwardRate > 1) {
      rows.push_back(kv("fast-forward", std::to_string(bg.fastForwardRate) + "x por frame",
                        Color::Magenta));
   }
   return section("Executivo / tempo critico", std::move(rows));
}

// A secao que o pedido nomeou explicitamente ("socket do tacview").
Element sectionTacview(const BackgroundInfo& bg)
{
   if (!bg.tacviewEnabled) {
      return section("Tacview (exportacao)", {
         kv("estado", "nenhum ( TacviewOutput ) no cenario", Color::GrayDark),
      });
    }

   std::ostringstream endpoint;
   endpoint << bg.tacviewHost << ":" << bg.tacviewPort;

   // Tres estados bem distintos, nao um booleano: a porta pode estar de pe
   // sem ninguem conectado (normal), pode ter falhado no bind (porta
   // ocupada por outra poc -- acontece de verdade neste repositorio, que
   // roda varias ao mesmo tempo), ou pode ter tido cliente que caiu.
   Element link;
   if (bg.tacviewInitFailed) {
      link = kv("socket", "FALHOU -- nem socket nem arquivo", Color::Red);
   } else if (bg.tacviewInitialized && !bg.tacviewListening) {
      // Inicializado sem socket = o bind falhou (porta ocupada por outra
      // poc/instancia) mas a gravacao em arquivo subiu. Ver
      // TacviewOutput::initIfNeeded().
      link = kv("socket", "porta OCUPADA -- so gravando em arquivo", Color::Red);
   } else if (!bg.tacviewInitialized) {
      link = kv("socket", "ainda nao inicializado", Color::GrayDark);
   } else if (bg.tacviewConnected) {
      link = kv("socket", "cliente conectado", Color::Green);
   } else if (bg.tacviewConnections > 0) {
      link = kv("socket", "escutando (cliente ja conectou e caiu)", Color::Yellow);
   } else {
      link = kv("socket", "escutando, ninguem conectou ainda", Color::Yellow);
   }

   Elements rows{
      kv("endereco de escuta", endpoint.str()),
      kv("callsign", bg.tacviewCallsign),
      std::move(link),
      kv("conexoes desde o inicio", std::to_string(bg.tacviewConnections)),
      kv("bytes enviados", humanBytes(bg.tacviewBytesSent)),
      kv("linhas ACMI", std::to_string(bg.tacviewLines)),
      kv("quadros (#t)", std::to_string(bg.tacviewFrames)),
      kv("tempo do stream", fmt1(bg.tacviewStreamTime) + " s"),
      kv("objetos declarados", std::to_string(bg.tacviewDeclared)),
      // A identidade publicada e o que da tipo/cor/modelo certos ao que
      // nasce em runtime (o missil) -- ver TacviewOutput::publishIdentities().
      kv("identidades publicadas", std::to_string(bg.tacviewIdentified)),
      kv("varreduras de radar", std::to_string(bg.radarScanPushCount)),
   };
   if (bg.tacviewRecording) {
      // So o nome do arquivo: o caminho inteiro nao cabe na coluna, e o
      // diretorio e sempre o mesmo (./app/data/recordings/).
      const auto slash{bg.tacviewFile.find_last_of('/')};
      rows.push_back(kv("gravando em", slash == std::string::npos
                                        ? bg.tacviewFile
                                        : bg.tacviewFile.substr(slash + 1), Color::Green));
   } else {
      rows.push_back(kv("gravacao", "nenhuma", Color::GrayDark));
   }
   return section("Tacview (exportacao)", std::move(rows));
}

Element sectionWorld(const BackgroundInfo& bg)
{
   std::ostringstream ref;
   ref << std::fixed << std::setprecision(4) << bg.refLatDeg << ", " << bg.refLonDeg;

   const int tod{static_cast<int>(bg.simTimeOfDaySec)};
   std::ostringstream clock;
   clock << std::setfill('0') << std::setw(2) << (tod / 3600) << ":"
         << std::setw(2) << ((tod / 60) % 60) << ":" << std::setw(2) << (tod % 60) << " UTC";

   return section("Mundo", {
      kv("players vivos", std::to_string(bg.playerCount)),
      kv("referencia (lat, lon)", ref.str()),
      kv("hora simulada", clock.str()),
      flag("banco de elevacao", bg.terrainLoaded, "carregado", "nao carregado"),
   });
}

Element sectionNetwork(const BackgroundInfo& bg)
{
   if (bg.networkHandlerCount == 0) {
      return section("Rede (DIS)", {
         kv("handlers", "nenhum (cenario hermetico)", Color::GrayDark),
      });
   }
   return section("Rede (DIS)", {
      kv("handlers", std::to_string(bg.networkHandlerCount), Color::Yellow),
      kv("taxa declarada", fmt1(bg.networkRateHz) + " Hz"),
      flag("thread de rede", bg.networkThreadRunning, "propria",
           "nenhuma -- processada neste laco"),
   });
}

Element sectionProcess(const BackgroundInfo& bg)
{
   if (bg.residentKb <= 0) return vbox(Elements{});
   std::ostringstream mem;
   mem << (bg.residentKb / 1024) << " MiB residentes";
   return section("Processo", {kv("memoria", mem.str())});
}

}

Element renderBackgroundPanel(const BackgroundInfo& bg)
{
   Elements parts;

   parts.push_back(text(" Thread de Tempo Nao-Critico ") | bold | bgcolor(Color::Blue)
                   | color(Color::White));
   parts.push_back(separator());

   parts.push_back(paragraphAlignLeft(
      "Este app nunca cria a thread de background NATIVA do MIXR "
      "(Station::createBackgroundProcess()) -- quem faz esse papel e o "
      "proprio laco que atualiza esta interface, chamando "
      "station->updateData(dt) fora do frame de tempo critico. E aqui que "
      "o gravador e drenado para o Tacview, a elevacao de terreno de cada "
      "player e atualizada, a rede DIS seria processada se o cenario "
      "declarasse 'networks:', e onde as outras abas sao amostradas."
      ) | dim);
   parts.push_back(text(""));

   // Duas colunas: o que e DESTE laco a esquerda, com quem ele CONVERSA a
   // direita. Cabe num terminal comum sem virar uma coluna unica
   // quilometrica.
   //
   // Largura FIXA por coluna, nao 'flex': este painel e desenhado dentro de
   // um frame() (ver 'backgroundTab' em app/DashboardLoop.cpp), e dentro de
   // um frame o filho recebe a caixa que PEDE -- um 'flex' ali cresce sem
   // limite util e empurra a segunda coluna para fora da janela visivel
   // (medido: a coluna da direita simplesmente sumia). Com largura fixa as
   // duas aparecem sempre, e o frame rola se o terminal for estreito demais.
   parts.push_back(hbox({
      vbox({sectionLoop(bg), sectionExecutive(bg), sectionWorld(bg)})
         | size(WIDTH, EQUAL, kColumnWidth),
      separator(),
      vbox({sectionTacview(bg), sectionNetwork(bg), sectionProcess(bg)})
         | size(WIDTH, EQUAL, kColumnWidth),
   }));

   return vbox(std::move(parts)) | border;
}

} // namespace app
