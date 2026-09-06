#include "app/Options.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace app {

namespace {

// std::stol/std::stoi lancam std::invalid_argument (token nao numerico) ou
// std::out_of_range (estoura o tipo) -- sem isto a excecao escapava ate o topo
// e virava um abort() sem nenhuma mensagem apontando QUAL argumento estava
// errado. Toda outra entrada invalida desta aplicacao (arquivo de cenario,
// player ausente, banco de elevacao corrompido -- ver TerrainData/
// StationBuilder/Fleet) sai por std::exit com uma mensagem clara; um
// '-threads abc' devia se comportar da mesma forma, nao travar com um sinal.
// Erro comum aos dois de baixo -- extraido para nao repetir a mensagem.
[[noreturn]] void dieNaoNumero(const std::string& flag, const std::string& token)
{
   std::cerr << "[app] " << flag << " espera um numero inteiro, recebi '"
             << token << "'" << std::endl;
   std::exit(EXIT_FAILURE);
}

long parseLongOrDie(const std::string& flag, const std::string& token)
{
   std::size_t consumido{};
   long valor{};
   try {
      valor = std::stol(token, &consumido);
   } catch (const std::exception&) {
      dieNaoNumero(flag, token);
   }
   // std::stol para no primeiro caractere que nao e' digito -- NAO lanca
   // excecao para '3.5'/'7xyz' (converte so o prefixo numerico e ignora o
   // resto, comportamento documentado da funcao). Achado rodando
   // (stress-sweep desta sessao): '-threads 3.5' virava '3' sem nenhum
   // aviso do '.5' descartado -- exatamente o tipo de entrada que este
   // arquivo promete recusar com mensagem clara (ver o comentario acima).
   if (consumido != token.size()) dieNaoNumero(flag, token);
   return valor;
}

int parseIntOrDie(const std::string& flag, const std::string& token)
{
   std::size_t consumido{};
   int valor{};
   try {
      valor = std::stoi(token, &consumido);
   } catch (const std::exception&) {
      dieNaoNumero(flag, token);
   }
   if (consumido != token.size()) dieNaoNumero(flag, token);
   return valor;
}

} // namespace

Options parseCommandLine(const int argc, char* argv[], const Options& defaults)
{
   Options opts{defaults};

   for (int i = 1; i < argc; i++) {
      const std::string arg{argv[i]};
      if (arg == "-scenario" && (i + 1) < argc) {
         opts.scenarioKey = argv[++i];
      } else if (arg == "-f" && (i + 1) < argc) {
         opts.scenarioPath = argv[++i];
      } else if (arg == "-folder" && (i + 1) < argc) {
         opts.scenarioFolder = argv[++i];
      } else if (arg == "-internal-picker") {
         opts.internalPicker = true;
      } else if (arg == "-deterministic" && (i + 1) < argc) {
         opts.deterministicFrames = parseLongOrDie(arg, argv[++i]);
         // isDeterministic() (Options.hpp) e' 'deterministicFrames > 0' -- um
         // '-deterministic 0' ou negativo passava o parse (e' um numero
         // valido) e so se revelava depois, em main.cpp, caindo NO MODO
         // INTERATIVO por omissao: sem TTY (script, CI, pipe), a TUI trava
         // pra sempre em ScreenInteractive::Fullscreen(), sem mensagem
         // nenhuma. Achado rodando: quem passa um N computado (nunca um
         // literal digitado) pode legitimamente computar 0. Falhar aqui,
         // alto e cedo, e' o mesmo padrao ja usado para token nao-numerico.
         if (opts.deterministicFrames <= 0) {
            std::cerr << "[app] -deterministic espera um numero de frames > 0, recebi '"
                      << opts.deterministicFrames << "'" << std::endl;
            std::exit(EXIT_FAILURE);
         }
      } else if (arg == "-parallel-decision") {
         opts.parallelDecision = true;
      } else if (arg == "-threads" && (i + 1) < argc) {
         opts.threadsOverride = parseIntOrDie(arg, argv[++i]);
      }
   }

   return opts;
}

} // namespace app
