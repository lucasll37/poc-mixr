#include "app/Options.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <vector>

// app::parseCommandLine() -- so traducao de argv para Options, sem tocar
// disco nem Station. As flags que aceitam valor (-threads, -deterministic, e
// as string puras -scenario/-f/-folder) sao cobertas pelo caminho feliz; os
// die() (token nao numerico, flag sem valor, -deterministic <= 0) sao death
// tests -- e a unica forma de testar um std::exit() sem derrubar o binario
// de teste inteiro.

namespace {

app::Options parse(std::vector<std::string> args, const app::Options& defaults = {})
{
   // argv[0] e sempre o nome do programa -- parseCommandLine comeca o laco
   // em i=1, entao ele precisa estar presente mesmo que nunca seja lido.
   args.insert(args.begin(), "app");

   std::vector<char*> argv;
   argv.reserve(args.size());
   for (auto& a : args) argv.push_back(a.data());

   return app::parseCommandLine(static_cast<int>(argv.size()), argv.data(), defaults);
}

} // namespace

TEST(ParseCommandLine, SemArgumentosMantemOsDefaults)
{
   app::Options defaults;
   defaults.scenarioKey = "patrol";
   defaults.threadsOverride = 3;

   const app::Options opts{parse({}, defaults)};

   EXPECT_EQ(opts.scenarioKey, "patrol");
   EXPECT_EQ(opts.threadsOverride, 3);
   EXPECT_FALSE(opts.isDeterministic());
}

TEST(ParseCommandLine, FlagDesconhecidaEIgnoradaSemConsumirOProximoToken)
{
   // '-bogus' nao casa com nenhum 'else if': tem que ser descartada
   // sozinha, sem "engolir" o argumento seguinte como se fosse o valor dela.
   const app::Options opts{parse({"-bogus", "-threads", "2"})};
   EXPECT_EQ(opts.threadsOverride, 2);
}

TEST(ParseCommandLine, ScenarioFFolderSaoIndependentes)
{
   // A exclusividade das tres formas de escolher cenario e uma REGRA DE USO
   // (documentada no header), nao uma validacao desta funcao -- ela so
   // traduz argv, sem decidir o que fazer com o resultado.
   const app::Options opts{parse({"-scenario", "intercept", "-f", "/tmp/x.edl",
                                  "-folder", "./sandbox"})};
   EXPECT_EQ(opts.scenarioKey, "intercept");
   EXPECT_EQ(opts.scenarioPath, "/tmp/x.edl");
   EXPECT_EQ(opts.scenarioFolder, "./sandbox");
}

TEST(ParseCommandLine, ThreadsConverteInteiro)
{
   const app::Options opts{parse({"-threads", "4"})};
   EXPECT_EQ(opts.threadsOverride, 4);
}

TEST(ParseCommandLine, DeterministicConverteLongEHabilitaIsDeterministic)
{
   const app::Options opts{parse({"-deterministic", "600"})};
   EXPECT_EQ(opts.deterministicFrames, 600);
   EXPECT_TRUE(opts.isDeterministic());
}

//------------------------------------------------------------------------------
// Death tests -- os quatro die() de app/Options.cpp. EXPECT_EXIT roda a
// expressao num processo filho (fork) e verifica o exit code e uma parte da
// saida; e o unico jeito de exercitar um std::exit() sem matar a suite
// inteira.
//------------------------------------------------------------------------------

TEST(ParseCommandLineMorte, ThreadsComTokenNaoNumericoMorreComMensagemClara)
{
   EXPECT_EXIT(parse({"-threads", "abc"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "numero inteiro");
}

TEST(ParseCommandLineMorte, ThreadsComTokenParcialmenteNumericoMorre)
{
   // std::stoi("3xyz", &consumido) converte so o prefixo "3" e NAO lanca --
   // e o 'consumido != token.size()' que pega isto (ver o comentario do
   // .cpp). Sem essa checagem, '-threads 3xyz' viraria silenciosamente 3.
   EXPECT_EXIT(parse({"-threads", "3xyz"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "numero inteiro");
}

TEST(ParseCommandLineMorte, ThreadsQueEstouraIntMorre)
{
   EXPECT_EXIT(parse({"-threads", "99999999999999999999"}),
              ::testing::ExitedWithCode(EXIT_FAILURE), "numero inteiro");
}

TEST(ParseCommandLineMorte, ThreadsSemValorNoFinalDeArgvMorre)
{
   EXPECT_EXIT(parse({"-threads"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "valor apos a flag");
}

TEST(ParseCommandLineMorte, DeterministicSemValorNoFinalDeArgvMorre)
{
   EXPECT_EXIT(parse({"-deterministic"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "valor apos a flag");
}

TEST(ParseCommandLineMorte, FMSemValorNoFinalDeArgvMorre)
{
   EXPECT_EXIT(parse({"-f"}), ::testing::ExitedWithCode(EXIT_FAILURE), "valor apos a flag");
}

TEST(ParseCommandLineMorte, FolderSemValorNoFinalDeArgvMorre)
{
   EXPECT_EXIT(parse({"-folder"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "valor apos a flag");
}

TEST(ParseCommandLineMorte, ScenarioSemValorNoFinalDeArgvMorre)
{
   EXPECT_EXIT(parse({"-scenario"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "valor apos a flag");
}

TEST(ParseCommandLineMorte, DeterministicComTokenNaoNumericoMorre)
{
   EXPECT_EXIT(parse({"-deterministic", "abc"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "numero inteiro");
}

TEST(ParseCommandLineMorte, DeterministicZeroMorre)
{
   // isDeterministic() e 'deterministicFrames > 0' -- um valor valido pro
   // std::stol, mas invalido pro contrato desta flag (ver o comentario do
   // .cpp: sem TTY, 0 cairia no modo interativo e travaria pra sempre).
   EXPECT_EXIT(parse({"-deterministic", "0"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "numero de frames");
}

TEST(ParseCommandLineMorte, DeterministicNegativoMorre)
{
   EXPECT_EXIT(parse({"-deterministic", "-5"}), ::testing::ExitedWithCode(EXIT_FAILURE),
              "numero de frames");
}
