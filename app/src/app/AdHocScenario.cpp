#include "app/AdHocScenario.hpp"

#include <filesystem>

namespace app {

const std::vector<std::string>& falconFleet()
{
   static const std::vector<std::string> fleet{"falcon1", "falcon2", "falcon3", "falcon4"};
   return fleet;
}

ScenarioEntry adHocScenario(const std::string& path)
{
   // A chave sai do nome do arquivo (sem diretorio nem extensao) so para
   // batizar o '.generated.edl' e o cabecalho da TUI -- nada mais depende
   // dela quando o cenario vem por '-f'.
   //
   // O stem() do C++ tira UMA extensao so, e as fixtures se chamam
   // '<nome>.edl.in' -- sem o corte abaixo a chave sairia '<nome>.edl' e o
   // arquivo gerado viraria '<nome>.edl.generated.edl'.
   std::string key{std::filesystem::path(path).stem().string()};
   const std::string sufixo{".edl"};
   if (key.size() > sufixo.size() && key.compare(key.size() - sufixo.size(), sufixo.size(), sufixo) == 0) {
      key.erase(key.size() - sufixo.size());
   }
   if (key.empty()) key = "ad-hoc";

   // Fallback: falcon1..4. NAO e um palpite coincidente -- e o unico caso
   // que de fato chega aqui hoje e o das fixtures de tests/scenario/
   // make_fixture.py, e o formato delas garante exatamente essa frota. O
   // modo 'intruder' daquele gerador acrescenta um bandit1 LOCAL de
   // proposito FORA da frota rastreada (e o intruso que se detecta, nao um
   // dos aviões observados) -- uma descoberta generica (tentada e revertida:
   // ver o historico desta funcao) pegaria esse bandit1 tambem, quebrando
   // exatamente a garantia que os testes de intruder dependem. Um cenario
   // custom de verdade, com frota diferente de falcon1..4, deve ser
   // carregado por '-folder <pasta>' em vez de '-f' -- esse caminho descobre
   // a frota em runtime (app::discoverFleet()).
   return ScenarioEntry{key, key, "cenario carregado por -f", path,
                        "", "", "", "", falconFleet()};
}

} // namespace app
