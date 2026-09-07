#pragma once

#include <string>
#include <vector>

namespace app {

//------------------------------------------------------------------------------
// A descricao de um cenario carregado por '-f <arquivo>' ou '-folder <pasta>'
// -- as duas fontes de cenario que este app conhece (nao ha mais catalogo
// estatico embutido no binario: toda pasta com um '.edl'/'.edl.in' proprio ja
// e alcancavel).
//
// Os 4 campos de Tacview (tacviewId/tacviewModelMap/tacviewTypeMap/
// tacviewColorMap) existem para um mecanismo GENERICO ('@include:nome@' +
// tokens, ver app::generateScenario()/ScenarioTemplate.cpp) que permite a um
// '.edl.in' compartilhar um fragmento identico entre cenarios -- hoje nenhum
// cenario deste repositorio usa esse caminho (ficam vazios sempre), mas o
// mecanismo continua disponivel.
//------------------------------------------------------------------------------
struct ScenarioEntry
{
   std::string key;            // usado so para nomear '<key>.generated.edl'/o log/o cabecalho da TUI
   std::string label;          // titulo curto, para o cabecalho do dashboard
   std::string description;    // uma linha, informativa
   std::string templatePath;   // o .edl/.edl.in

   // Os 4 tokens do mecanismo generico de fragmento -- ver o comentario
   // acima. Vazios em todo cenario deste repositorio hoje.
   std::string tacviewId;
   std::string tacviewModelMap;
   std::string tacviewTypeMap;
   std::string tacviewColorMap;

   // A FROTA observada: quais players entram no dump de '-deterministic' e
   // recebem o manete de cruzeiro. Vazia sinaliza para main.cpp descobrir os
   // players em runtime (app::discoverFleet()) em vez de assumir uma lista
   // fixa -- o caso de '-folder', onde o cenario pode ter qualquer nome de
   // player. Quando nao vazia, collectFleet() aborta se um nome daqui nao
   // existir -- um cenario que perdeu um player tem de falhar alto, nao
   // silenciosamente imprimir menos linhas.
   std::vector<std::string> fleet;
};

// A frota das pocs de voo: os quatro falcons. E o fallback assumido para
// '-f <arquivo>' (ver adHocScenario() abaixo) -- o unico caso real que cai
// nisso hoje sao as fixtures de tests/scenario/make_fixture.py, que SEMPRE
// derivam de um cenario com falcon1..4. O intruso (bandit1), quando existe,
// NAO entra -- ele nao e observado, e nenhum dump deste repositorio jamais o
// imprimiu.
const std::vector<std::string>& falconFleet();

// Entrada para '-f <arquivo>': monta uma ScenarioEntry a partir so do
// caminho, assumindo falcon1..4 como frota e sem token de Tacview nenhum
// para substituir (o arquivo ja traz o bloco 'dataRecorder:' inteiro). Um
// cenario com frota diferente (ex.: um unico player de nome custom) deve ser
// carregado por '-folder <pasta>' em vez de '-f' -- esse caminho descobre a
// frota em runtime (app::discoverFleet()) em vez de assumir esta lista.
ScenarioEntry adHocScenario(const std::string& path);

} // namespace app
