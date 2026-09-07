// edlcheck -- oraculo de validacao PROFUNDA para o editor grafico de .edl
// (src/ui/edl_builder.jsx), para src/ui/scripts/edl_lint.py (o lint LEVE, por
// regex/paren-matching, nao pode garantir fidelidade 100% ao parser real) e
// para o editor de .edl EM MEMORIA embutido no proprio dashboard (aba "EDL",
// ver app/EdlEditorState.hpp/app/DashboardLoop.cpp) -- o botao "Validar" de
// la roda este mesmo binario contra o texto editado antes de "Rodar".
//
// Reaproveita a MESMA cadeia de factories de producao (mixr_factory.cpp,
// linkado aqui como um segundo executavel que a compila de novo -- mesmo
// padrao ja usado por src/rl/bindings/meson.build, nao ha lib estatica
// pronta pra isto no repositorio) e chama mixr::base::edl_parser()
// DIRETAMENTE, sem nada do resto que app::buildStation() (app/src/app/
// StationBuilder.cpp) exige: sem terreno fixo, sem frota, sem WorldModel
// obrigatorio. Isso e deliberado -- um cenario novo montado do zero no
// editor grafico pode nao ser "mais um caca" e nao devia falhar por um
// pressuposto do ./app que nao e da gramatica EDL.
//
// A sequencia abaixo -- setBuiltinFactory() / edl_parser() / seal() -- e
// EXATAMENTE a de StationBuilder.cpp::buildStation(), na mesma ordem,
// porque e o que faz este binario ser um oraculo FIEL (o mesmo caminho que
// o ./app de verdade percorre), nao uma reimplementacao paralela que possa
// divergir.
//
// Duas saidas possiveis, ambas por EXIT CODE (sem exigir grep de stderr
// para o caso comum):
//   0  -- parseou sem erro, pelo menos um objeto construido.
//   1  -- erro de parse/slot/isValid() (num_errors > 0), OU nenhum objeto
//         construido, OU o proprio mixrFactory() encontrou um nome de
//         fabrica desconhecido e chamou std::exit(EXIT_FAILURE) por dentro
//         (mixr::xplugin::reportUnknownFactoryName(), [[noreturn]]) -- esse
//         terceiro caso termina o processo ANTES deste main() ver
//         num_errors, entao a mensagem que aparece e a do proprio xplugin,
//         nao a daqui. Os tres casos colapsam no mesmo exit code de
//         proposito: para quem so quer saber "este .edl e valido?", os
//         tres sao a mesma resposta (nao).
//
// NUNCA passar mixrFactoryBuiltin() (que PODE devolver nullptr) para
// edl_parser() -- so mixrFactory() (que nunca devolve nullptr, aborta por
// dentro em vez disso). O proprio mixr_factory.hpp documenta por que:
// devolver nullptr pro parser termina em SIGSEGV (edl_parser.y, um
// unref() sem checar nulo), nao um erro contavel.

#include "mixr_factory.hpp"

#include "xplugin/PluginRegistry.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/edl_parser.hpp"

#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
   if (argc != 2) {
      std::cerr << "uso: edlcheck <arquivo.edl>" << std::endl;
      return 2;
   }
   const std::string filename{argv[1]};

   mixr::xplugin::setBuiltinFactory(mixrFactoryBuiltin);

   int num_errors{};
   mixr::base::Object* obj{mixr::base::edl_parser(filename, mixrFactory, &num_errors)};

   mixr::xplugin::seal();

   if (num_errors > 0) {
      std::cerr << "edlcheck: " << filename << ": " << num_errors << " erro(s) de parse" << std::endl;
      if (obj != nullptr) obj->unref();
      return 1;
   }
   if (obj == nullptr) {
      std::cerr << "edlcheck: " << filename << ": nenhum objeto construido (arquivo vazio, ou so comentarios/erro de sintaxe irrecuperavel)" << std::endl;
      return 1;
   }

   // Desembrulha o Pair de topo ('nome: ( Classe ... )'), se houver --
   // mesma logica de app::buildStation() (StationBuilder.cpp) -- mas SEM
   // exigir Station na ponta, ao contrario dele: edlcheck aceita QUALQUER
   // fragmento EDL valido, nao so um cenario completo (a gramatica em si
   // tambem nao exige -- edl_parser.y:139-141 nao checa tipo nenhum).
   const auto pair = dynamic_cast<mixr::base::Pair*>(obj);
   if (pair != nullptr) {
      obj = pair->object();
      obj->ref();
      pair->unref();
   }

   std::cout << "edlcheck: " << filename << ": OK" << std::endl;
   obj->unref();
   return 0;
}
