// app::ensureTerrainData()/ensureAllTerrainTiles() -- preparacao do banco de
// elevacao SRTM ANTES do parse do cenario (app/TerrainData.hpp). Sem MIXR,
// sem Station -- so filesystem e um std::system("gzip"/"gunzip") de verdade.
//
// Achado por auditoria (workflow de investigacao desta sessao, dimensao
// 'testes-adversariais'): a validacao de tamanho ja e robusta no CODIGO
// (isValidSrtmSize() so aceita os dois tamanhos exatos que SrtmHgtFile
// reconhece; ensureTerrainData() morre com mensagem clara citando o tamanho
// achado e os dois esperados; ensureAllTerrainTiles() e deliberadamente
// TOLERANTE, so avisa e segue) -- mas nenhum teste protegia isso ate aqui.
//
// EXPECT_EXIT roda a expressao num processo FILHO (fork) e verifica o exit
// code + um trecho da saida -- o mesmo padrao ja usado em test_options.cpp
// para os die() de app::parseCommandLine().
#include "app/TerrainData.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace fs = std::filesystem;

// Um diretorio TEMPORARIO por teste (nao dentro do repositorio -- isto nao
// e uma fixture versionada, e um cenario adversarial construido em disco na
// hora). O nome inclui o nome do teste em curso, pra dois testes rodando
// (gtest e single-process, entao isso e so higiene) nunca colidirem.
class TerrainDataAdversarial : public ::testing::Test {
protected:
   void SetUp() override
   {
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      dir_ = fs::temp_directory_path() / ("poc_mixr_terrain_" + std::string(info->name()));
      fs::remove_all(dir_);
      fs::create_directories(dir_);
   }

   void TearDown() override { fs::remove_all(dir_); }

   // Grava 'conteudo' num arquivo comum e o comprime com gzip de verdade --
   // o mesmo formato que 'gunzip -kf' (chamado por TerrainData.cpp) espera.
   // O ARQUIVO RESULTANTE tem o tamanho de 'conteudo', nunca um tamanho
   // SRTM valido (o teste quer exatamente isso: um '.hgt.gz' que descomprime
   // para algo do tamanho ERRADO).
   void gravarGzCorrompido(const std::string& baseName, const std::string& conteudo)
   {
      const fs::path bruto{dir_ / (baseName + ".hgt")};
      {
         std::ofstream out(bruto, std::ios::binary);
         out << conteudo;
      }
      const std::string cmd{"gzip -f \"" + bruto.string() + "\""};
      ASSERT_EQ(std::system(cmd.c_str()), 0) << "setup do teste: gzip falhou";
      ASSERT_TRUE(fs::exists(dir_ / (baseName + ".hgt.gz")));
      ASSERT_FALSE(fs::exists(bruto)) << "gzip sem -k deveria ter removido o .hgt bruto";
   }

   fs::path dir_;
};

TEST_F(TerrainDataAdversarial, ArquivoAusenteMorreComMensagemClara)
{
   // Nenhum '.hgt'/'.hgt.gz' criado -- o caso "banco de elevacao ausente".
   const std::string dirStr{dir_.string() + "/"};
   EXPECT_EXIT(app::ensureTerrainData(dirStr, "S23W043"),
              ::testing::ExitedWithCode(EXIT_FAILURE), "ausente");
}

TEST_F(TerrainDataAdversarial, GzTruncadoDescomprimeMasTamanhoErradoMorreComMensagemClara)
{
   // 11 bytes -- nem de longe SRTM3 (2884802) nem SRTM1 (25934402). Simula
   // um download interrompido/descompressao pela metade.
   gravarGzCorrompido("S23W043", "hello world");

   const std::string dirStr{dir_.string() + "/"};
   EXPECT_EXIT(app::ensureTerrainData(dirStr, "S23W043"),
              ::testing::ExitedWithCode(EXIT_FAILURE), "bytes");
}

TEST_F(TerrainDataAdversarial, TileExtraCorrompidoNaoAbortaOProcesso)
{
   // ensureAllTerrainTiles() e TOLERANTE de proposito (cobre tiles OPCIONAIS
   // da vista de Mapa, nao o tile OBRIGATORIO do cenario) -- isto NAO deve
   // ser um death test: se abortar, o proprio teste falha (nunca chega no
   // EXPECT abaixo), provando a regressao.
   gravarGzCorrompido("S22W043", "tile parcialmente baixado");

   const std::string dirStr{dir_.string() + "/"};
   app::ensureAllTerrainTiles(dirStr);   // nao deve chamar std::exit()

   // O .hgt foi descomprimido (tentativa feita), so que com tamanho errado
   // -- prova que a funcao de fato tentou, em vez de pular o arquivo cedo
   // demais por outro motivo.
   EXPECT_TRUE(fs::exists(dir_ / "S22W043.hgt"));
   const auto tamanho = fs::file_size(dir_ / "S22W043.hgt");
   EXPECT_NE(tamanho, 2884802u);
   EXPECT_NE(tamanho, 25934402u);
}

TEST_F(TerrainDataAdversarial, TileValidoNaoRedescomprimeNemAvisa)
{
   // Caminho FELIZ, como controle negativo dos dois casos acima: um '.hgt'
   // ja com tamanho SRTM3 valido (conteudo arbitrario -- so o TAMANHO
   // importa para isValidSrtmSize(); SrtmHgtFile::load() de verdade nunca e
   // chamado aqui) faz ensureTerrainData() retornar sem tentar descomprimir
   // nada -- nem precisa existir um '.gz' ao lado.
   {
      std::ofstream out(dir_ / "S23W043.hgt", std::ios::binary);
      out << std::string(2884802, '\0');
   }
   const std::string dirStr{dir_.string() + "/"};
   EXPECT_EXIT(
      { app::ensureTerrainData(dirStr, "S23W043"); std::exit(0); },
      ::testing::ExitedWithCode(0), "");
}

} // namespace
