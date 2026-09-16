//
// libs/xterrain::MultiTileTerrain -- a funcionalidade especifica pedida:
// varios tiles residentes ao mesmo tempo (players em regioes DIFERENTES),
// despejo de LRU quando o numero de tiles DISTINTOS excede o teto,
// travessia de fronteira entre dois tiles sem logica especial, e
// concorrencia real -- multiplas threads chamando getElevation() no MESMO
// objeto, o cenario que numBgThreads>1 torna real em producao (ver o
// cabecalho de MultiTileTerrain.hpp).
//
// Os tiles usados aqui sao SINTETICOS (SRTM3, 1201x1201 = 2884802 bytes,
// o tamanho exato que SrtmHgtFile::determineSrtmInfo() aceita), gerados em
// ::testing::TempDir() -- NAO dependem dos tiles reais de
// shared/data/terrain/srtm/. Cada tile e' preenchido com um valor UNIFORME
// e DISTINTO (todo post do arquivo com o MESMO int16), o que faz o proprio
// valor devolvido por getElevation() provar sozinho qual tile respondeu,
// sem precisar inspecionar estado interno nem se preocupar com a ordem de
// varredura (linha/coluna) que SrtmHgtFile usa por dentro.
//
#include "xterrain/MultiTileTerrain.hpp"
#include "xterrain/factory.hpp"

#include "mixr/base/Object.hpp"
#include "mixr/base/String.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

namespace fs = std::filesystem;
using namespace mixr;

constexpr std::size_t kSrtm3Side{1201};   // 1201*1201*2 = 2884802 bytes (SRTM3)

void writeBigEndianInt16(std::ofstream& out, const std::int16_t value)
{
   out.put(static_cast<char>((value >> 8) & 0xFF));
   out.put(static_cast<char>(value & 0xFF));
}

void writeUniformSrtm3Tile(const fs::path& path, const std::int16_t elevationM)
{
   std::ofstream out(path, std::ios::binary | std::ios::trunc);
   ASSERT_TRUE(out.is_open()) << "falha ao criar tile de teste: " << path;
   for (std::size_t i = 0; i < kSrtm3Side * kSrtm3Side; ++i) {
      writeBigEndianInt16(out, elevationM);
   }
}

// Mesmo tile acima, mas so' o '.hgt.gz' sobrevive (gzip -f remove o '.hgt')
// -- exercita o caminho de descompressao sob demanda de residentTileLocked(),
// a mesma situacao dos tiles reais versionados (so' o .gz fica no git).
void writeUniformSrtm3TileGz(const fs::path& hgtPath, const std::int16_t elevationM)
{
   writeUniformSrtm3Tile(hgtPath, elevationM);
   const std::string cmd{"gzip -f \"" + hgtPath.string() + "\""};
   ASSERT_EQ(std::system(cmd.c_str()), 0) << "gzip falhou para " << hgtPath;
}

base::String* str(const std::string& s) { return new base::String(s.c_str()); }

class MultiTileTerrainTest : public ::testing::Test {
protected:
   void SetUp() override
   {
      dir = fs::path(::testing::TempDir()) / "xterrain-multi-tile-test";
      std::error_code ec;
      fs::remove_all(dir, ec);
      fs::create_directories(dir, ec);
      ASSERT_TRUE(fs::exists(dir));

      // Dois tiles ADJACENTES, fronteira em lon = -1: A a oeste (S01W002,
      // elev 100, so' .hgt.gz -- testa a descompressao sob demanda), B a
      // leste (S01W001, elev 200, .hgt puro).
      writeUniformSrtm3TileGz(dir / "S01W002.hgt", 100);
      writeUniformSrtm3Tile(dir / "S01W001.hgt", 200);

      // kMaxResidentTiles+3 tiles DISTANTES entre si e do par acima, para o
      // teste de despejo de LRU -- cada um com elevacao = 1000+indice.
      for (std::size_t i = 0; i < xterrain::MultiTileTerrain::kMaxResidentTiles + 3; ++i) {
         const int latDeg{static_cast<int>(i) + 10};   // 10..24 -- sempre 2 digitos
         // 'N' (norte, latitude POSITIVA) -- distanteLat()/distanteLon()
         // abaixo consultam pontos com lat > 0.
         const std::string name{"N" + std::to_string(latDeg) + "W100.hgt"};
         writeUniformSrtm3Tile(dir / name, static_cast<std::int16_t>(1000 + i));
      }
   }

   void TearDown() override
   {
      std::error_code ec;
      fs::remove_all(dir, ec);
   }

   // Instancia pronta pra consultar -- dir setado e reset() (=> loadData(),
   // que so' monta o INDICE do diretorio; nenhum tile e' carregado ainda).
   xterrain::MultiTileTerrain* makeReady() const
   {
      auto* const t{new xterrain::MultiTileTerrain()};
      auto* const d{str(dir.string() + "/")};
      EXPECT_TRUE(t->setSlotByName("dir", d));
      d->unref();
      t->reset();
      return t;
   }

   // lat/lon do i-esimo tile "distante" (SetUp acima).
   static double distanteLat(std::size_t i) { return 10.0 + static_cast<double>(i) + 0.5; }
   static double distanteLon() { return -99.5; }
   static double distanteElev(std::size_t i) { return 1000.0 + static_cast<double>(i); }

   fs::path dir;
};

TEST_F(MultiTileTerrainTest, ConstroiEDestroiSemDirSemCrash)
{
   const auto t = new xterrain::MultiTileTerrain();
   double elev{};
   EXPECT_FALSE(t->getElevation(&elev, -22.0, -43.0));
   EXPECT_FALSE(t->isDataLoaded());
   t->unref();
}

TEST_F(MultiTileTerrainTest, DiretorioInexistenteNaoTravaEDevolveFalse)
{
   const auto t = new xterrain::MultiTileTerrain();
   auto* const d{str("/caminho/que/nao/existe/de-verdade/")};
   EXPECT_TRUE(t->setSlotByName("dir", d));
   d->unref();
   t->reset();

   double elev{};
   EXPECT_FALSE(t->getElevation(&elev, -22.0, -43.0));
   EXPECT_FALSE(t->isDataLoaded());
   t->unref();
}

// A pergunta que motivou esta lib: com players em regioes diferentes
// (aqui, dois tiles distintos), cada consulta resolve para o SEU tile,
// independente da outra -- nao ha atribuicao de player a tile.
TEST_F(MultiTileTerrainTest, ResolvePlayersEmTilesDiferentesParaOTileCorreto)
{
   const auto t = makeReady();

   double elevA{};
   double elevB{};
   ASSERT_TRUE(t->getElevation(&elevA, -0.5, -1.5));   // dentro de S01W002 (elev 100)
   ASSERT_TRUE(t->getElevation(&elevB, -0.5, -0.5));   // dentro de S01W001 (elev 200)
   EXPECT_DOUBLE_EQ(elevA, 100.0);
   EXPECT_DOUBLE_EQ(elevB, 200.0);
   EXPECT_EQ(t->residentTileCount(), 2u);

   t->unref();
}

TEST_F(MultiTileTerrainTest, CruzaFronteiraEntreTilesSemLogicaEspecial)
{
   const auto t = makeReady();

   double elev{};
   ASSERT_TRUE(t->getElevation(&elev, -0.5, -1.001));   // ainda em S01W002
   EXPECT_DOUBLE_EQ(elev, 100.0);

   ASSERT_TRUE(t->getElevation(&elev, -0.5, -0.999));   // ja em S01W001
   EXPECT_DOUBLE_EQ(elev, 200.0);

   t->unref();
}

TEST_F(MultiTileTerrainTest, ForaDeTodosOsTilesDevolveFalseSemInventarElevacao)
{
   const auto t = makeReady();

   double elev{-1.0};
   EXPECT_FALSE(t->getElevation(&elev, 45.0, 45.0));   // longe de qualquer tile indexado
   EXPECT_DOUBLE_EQ(elev, -1.0);   // contrato de Terrain::getElevation(): sem sucesso, nao mexe

   t->unref();
}

TEST_F(MultiTileTerrainTest, CacheNuncaExcedeOTetoDeTilesResidentes)
{
   const auto t = makeReady();

   for (std::size_t i = 0; i < xterrain::MultiTileTerrain::kMaxResidentTiles + 3; ++i) {
      double elev{};
      ASSERT_TRUE(t->getElevation(&elev, distanteLat(i), distanteLon())) << "tile " << i;
      EXPECT_DOUBLE_EQ(elev, distanteElev(i));
      EXPECT_LE(t->residentTileCount(), xterrain::MultiTileTerrain::kMaxResidentTiles);
   }

   // Depois de tocar em mais tiles do que o teto, o cache fica CHEIO (nao
   // vazio, nao menor que o teto por acidente de despejo excessivo).
   EXPECT_EQ(t->residentTileCount(), xterrain::MultiTileTerrain::kMaxResidentTiles);

   t->unref();
}

TEST_F(MultiTileTerrainTest, TileDespejadoRecarregaComOValorCorreto)
{
   const auto t = makeReady();

   double primeiro{};
   ASSERT_TRUE(t->getElevation(&primeiro, distanteLat(0), distanteLon()));   // primeiro tile tocado
   EXPECT_DOUBLE_EQ(primeiro, distanteElev(0));

   // Toca em kMaxResidentTiles tiles A MAIS, todos distintos do primeiro --
   // suficiente para garantir (LRU) que o primeiro foi despejado do cache.
   for (std::size_t i = 1; i <= xterrain::MultiTileTerrain::kMaxResidentTiles; ++i) {
      double elev{};
      ASSERT_TRUE(t->getElevation(&elev, distanteLat(i), distanteLon()));
   }

   // Consultar o primeiro de novo: precisa ser recarregado (descompressao/
   // parse de novo), e o valor devolvido tem que continuar correto --
   // thrashing custa I/O, nunca corretude.
   double primeiroDeNovo{};
   ASSERT_TRUE(t->getElevation(&primeiroDeNovo, distanteLat(0), distanteLon()));
   EXPECT_DOUBLE_EQ(primeiroDeNovo, distanteElev(0));

   t->unref();
}

// O caso que motivou a pergunta original: players em regioes DIFERENTES
// decidindo em threads DIFERENTES ao mesmo tempo (numBgThreads>1 em
// producao) -- aqui, N threads reais batendo no MESMO objeto, misturando
// consulta ao MESMO tile entre threads (disputa pelo mesmo TileEntry) e a
// tiles DIFERENTES ao mesmo tempo.
TEST_F(MultiTileTerrainTest, ConcorrenciaVariasThreadsMesmoObjetoSemCorrida)
{
   const auto t = makeReady();

   constexpr int kThreads{8};
   constexpr int kIteracoesPorThread{300};
   std::atomic<int> falhas{0};

   std::vector<std::thread> threads;
   threads.reserve(kThreads);
   for (int th = 0; th < kThreads; ++th) {
      threads.emplace_back([&, th]() {
         for (int i = 0; i < kIteracoesPorThread; ++i) {
            const int escolha{(th + i) % 3};
            double elev{};
            bool ok{};
            double esperado{};
            if (escolha == 0) {
               ok = t->getElevation(&elev, -0.5, -1.5);
               esperado = 100.0;
            } else if (escolha == 1) {
               ok = t->getElevation(&elev, -0.5, -0.5);
               esperado = 200.0;
            } else {
               const std::size_t idx{
                  static_cast<std::size_t>(i) % (xterrain::MultiTileTerrain::kMaxResidentTiles + 3)};
               ok = t->getElevation(&elev, distanteLat(idx), distanteLon());
               esperado = distanteElev(idx);
            }
            if (!ok || elev != esperado) falhas.fetch_add(1);
         }
      });
   }
   for (auto& th : threads) th.join();

   EXPECT_EQ(falhas.load(), 0);
   EXPECT_LE(t->residentTileCount(), xterrain::MultiTileTerrain::kMaxResidentTiles);

   t->unref();
}

// clone()/copyData() de verdade (IMPLEMENT_SUBCLASS, nao ABSTRACT como o
// QuadMap nativo) -- o clone nao herda o cache quente, mas resolve sozinho.
TEST_F(MultiTileTerrainTest, CloneNasceComCacheVazioMasFuncionaSozinho)
{
   const auto original = makeReady();

   double elev{};
   ASSERT_TRUE(original->getElevation(&elev, -0.5, -1.5));
   EXPECT_GT(original->residentTileCount(), 0u);

   const auto clone = original->clone();
   ASSERT_NE(clone, nullptr);
   EXPECT_EQ(clone->residentTileCount(), 0u);   // cache NAO e' clonado

   double elevClone{};
   EXPECT_TRUE(clone->getElevation(&elevClone, -0.5, -1.5));
   EXPECT_DOUBLE_EQ(elevClone, 100.0);   // mas resolve sozinho, pelo MESMO 'dir'

   clone->unref();
   original->unref();
}

TEST_F(MultiTileTerrainTest, GetElevationsPluralDelegaPontoAPontoDentroDoTile)
{
   const auto t = makeReady();

   constexpr unsigned int kN{5};
   double elevs[kN]{};
   bool valid[kN]{};
   const unsigned int achados{
      t->getElevations(elevs, valid, kN, -0.9, -1.9, 90.0, 500.0, false)};

   EXPECT_GT(achados, 0u);
   for (unsigned int i = 0; i < kN; ++i) {
      if (valid[i]) { EXPECT_DOUBLE_EQ(elevs[i], 100.0); }
   }

   t->unref();
}

TEST(MultiTileTerrainFactory, ConstroiPeloNomeDeFabricaCorreto)
{
   base::Object* const obj{xterrain::factory("MultiTileTerrain")};
   ASSERT_NE(obj, nullptr);
   EXPECT_NE(dynamic_cast<xterrain::MultiTileTerrain*>(obj), nullptr);
   obj->unref();

   EXPECT_EQ(xterrain::factory("NomeQueNaoExiste"), nullptr);
}

} // namespace
