#include "app/ScenarioCatalog.hpp"

#include <filesystem>
#include <system_error>

namespace app {

namespace {

// Modelo/tipo/cor dos falcon1..4 no Tacview -- 'intercept_missile' acrescenta
// bandit1 por cima (ver withBandit1() abaixo).
const char* const kFalconsModelMap{
   "falcon1: \"A-4E\"  falcon2: \"A-4E\"  falcon3: \"A-4E\"  falcon4: \"A-4E\""};
const char* const kFalconsTypeMap{
   "falcon1: \"Air+FixedWing\"  falcon2: \"Air+FixedWing\"\n"
   "                           falcon3: \"Air+FixedWing\"  falcon4: \"Air+FixedWing\""};
const char* const kFalconsColorMap{
   "falcon1: \"Blue\"  falcon2: \"Blue\"  falcon3: \"Blue\"  falcon4: \"Blue\""};

// O missil de 'intercept_missile' e criado em RUNTIME: AbstractWeapon::
// release() clona o ( GuidedMissile ) do 'stores:' e batiza o flyout de
// "W%05d" a partir do proximo id de arma liberada -- um nome que nao da pra
// escrever num mapa com confianca. Quem resolve tipo/cor dele e o C++
// (TacviewOutput::publishIdentities(), que le a CLASSE e o lado do Player
// vivo). O que sobra para o EDL e so o MODELO 3D, e esse da pra mapear pela
// chave de 'type:' -- "AIM1" e o type: declarado no proprio stores:, ao
// contrario do nome, que so existe depois do lancamento.
const char* const kMissileModelMap{"\n                           AIM1: \"AIM-120C\""};

std::string withBandit1(const std::string& falcons, const std::string& bandit1Suffix)
{
   return falcons + bandit1Suffix;
}

} // namespace

const std::vector<std::string>& falconFleet()
{
   static const std::vector<std::string> fleet{"falcon1", "falcon2", "falcon3", "falcon4"};
   return fleet;
}

const std::vector<ScenarioEntry>& scenarioCatalog()
{
   static const std::vector<ScenarioEntry> catalog{
      {
         "intercept_missile", "Intercepto + Missil",
         "+ falcon1 com um missil guiado -- lancamento/detonacao, otimo pra pausar no meio",
         "./app/configs/scenario_intercept_missile.edl.in",
         "intercept-missile",   // note o hifen -- ja era assim no eventName/fileName de producao
         withBandit1(kFalconsModelMap, "\n                           bandit1: \"A-4E\"")
            + kMissileModelMap,
         withBandit1(kFalconsTypeMap, "  bandit1: \"Air+FixedWing\""),
         withBandit1(kFalconsColorMap, "  bandit1: \"Red\""),
         falconFleet(),
      },

      // ---------------------------------------------------------------
      // AS POCS. Cada uma traz o proprio bloco 'dataRecorder:' escrito no
      // .edl.in (com a porta de Tacview e o diretorio de dados dela), entao
      // os quatro campos de Tacview ficam vazios aqui -- nao ha token para
      // substituir. O 'tacviewId' tambem: ele so alimenta o fragmento.
      // ---------------------------------------------------------------
      {
         "single-thread", "single-thread (DIS)",
         "decisao no ( SimAgent ) nativo da Station, em updateData() -- Tacview 1234",
         "./src/poc/dis/single-thread/configs/scenario.edl.in",
         "", "", "", "", falconFleet(),
      },
      {
         "multi-thread", "multi-thread (DIS)",
         "a mesma pilha decidindo na fase 3 do frame de tempo critico -- Tacview 1234",
         "./src/poc/dis/multi-thread/configs/scenario.edl.in",
         "", "", "", "", falconFleet(),
      },
      {
         "bandit", "bandit (DIS)",
         "so o intruso: joystick fisico ou Autopilot de fallback, emitindo DIS -- Tacview 1235",
         "./src/poc/dis/bandit/configs/scenario.edl",
         "", "", "", "", {"bandit1"},
      },
      {
         "python-flight", "python-flight",
         "a multi-thread com as leis de voo em .py, lidos em tempo de execucao -- Tacview 1237",
         "./src/poc/python-flight/configs/scenario.edl.in",
         "", "", "", "", falconFleet(),
      },
      {
         "onnx-policy", "onnx-policy",
         "a multi-thread com a decisao numa rede neural (.onnx) -- Tacview 1238",
         "./src/poc/onnx-policy/configs/scenario.edl.in",
         "", "", "", "", falconFleet(),
      },
      {
         "built-in_mixr_1", "built-in_mixr_1",
         "o player maximo: falcon1 com 53 classes nativas do mixr::models -- Tacview 1239",
         "./src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
         "", "", "", "", falconFleet(),
      },
      {
         "full-systems-nav", "full-systems-nav",
         "o player maximo voando de verdade: BT de um no so segue Route/Steerpoint nativos -- Tacview 1240",
         "./src/poc/full-systems-nav/configs/scenario_full_nav.edl.in",
         "", "", "", "", {"a4"},
      },

      // >>> PROXIMO MODELO/CENARIO AQUI -- acrescente seu bloco ACIMA desta
      // linha, sempre no FIM da lista, nunca no meio das entradas de cima.
      // Isto e um ponto de insercao unico de proposito: dois PRs de modelos
      // diferentes que so ACRESCENTAM aqui colidem da forma mais barata de
      // resolver (duas adicoes consecutivas -- a resolucao e sempre "manter
      // as duas"). Ver models/README.md secao 4.1 para os campos, e
      // CONTRIBUTING.md secao "Pontos de conflito de merge conhecidos". <<<
   };
   return catalog;
}

ScenarioEntry adHocScenario(const std::string& path)
{
   // Se o caminho de '-f' e o MESMO arquivo do templatePath de uma entrada
   // ja catalogada, reusa a entrada INTEIRA -- inclusive a frota. Sem isto,
   // um cenario com frota diferente de falcon1..4 (ex.: full-systems-nav,
   // frota {"a4"}) abortava em collectFleet() quando carregado por '-f',
   // mesmo ja cadastrado corretamente no catalogo -- o mesmo arquivo se
   // comportando diferente conforme e invocado por chave ou por caminho.
   // weakly_canonical() (nao a versao que lanca excecao -- isto e so uma
   // checagem de UX) resolve os dois lados relativos ao MESMO CWD do
   // processo, a mesma convencao que ja rege '-scenario'.
   std::error_code ec;
   const std::filesystem::path normalized{std::filesystem::weakly_canonical(path, ec)};
   if (!ec) {
      for (const auto& entry : scenarioCatalog()) {
         std::error_code ecEntry;
         const std::filesystem::path entryNormalized{
            std::filesystem::weakly_canonical(entry.templatePath, ecEntry)};
         if (!ecEntry && entryNormalized == normalized) return entry;
      }
   }

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
   // custom de verdade, sem fixture nenhuma, deve ser CADASTRADO no
   // catalogo (ver a entrada 'full-systems-nav' acima) -- isso e' o que faz
   // o casamento do bloco anterior encontrar a frota certa.
   return ScenarioEntry{key, key, "cenario carregado por -f", path,
                        "", "", "", "", falconFleet()};
}

const ScenarioEntry* findScenario(const std::string& key)
{
   for (const auto& entry : scenarioCatalog()) {
      if (entry.key == key) return &entry;
   }
   return nullptr;
}

} // namespace app
