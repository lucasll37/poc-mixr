#pragma once

namespace mixr {
namespace models { class WorldModel; }
}

namespace app {

//------------------------------------------------------------------------------
// checkGrootMonitorTarget() -- confere se MIXR_GROOT_MONITOR nomeia um player
// que de fato existe no cenario carregado, e diz o que fazer quando nao.
//
// POR QUE ISTO MORA NO HOST, e nao no modelo (a pergunta obvia, ja que quem
// liga o monitor e' 'BtBehavior::startGrootMonitorIfRequested()'):
// nenhum BtBehavior consegue concluir "ninguem casou". Cada um so' sabe o
// PROPRIO nome, e um outro player pode casar depois -- entao, do lado do
// modelo, "nao casei" e "ninguem casou" sao indistinguiveis. O host e' o unico
// que tem a lista inteira de players ja montada.
//
// O caso que motivou isto e' real e era 100% SILENCIOSO: o exemplo documentado
// em 'make help'/CLAUDE.md/CONTRIBUTING.md era 'PLAYER=falcon1', e 'falcon1'
// nao existe em cenario nenhum de sandbox/ (os nomes de la sao a4, a4_1..a4_8,
// c130). Nenhuma porta abria, nada era logado, e o Groot so' mostrava
// "Was not able to connect" -- sem nada apontando para o nome errado.
//
// O segundo ramo (player existe) tambem carrega informacao: hoje SO' o modelo
// A-4 implementa o hook do monitor. 'MIXR_GROOT_MONITOR=c130' nomeia um player
// legitimo e mesmo assim e' no-op completo.
//
// Sem a variavel definida, retorna na hora -- custo zero no caminho normal.
//------------------------------------------------------------------------------
void checkGrootMonitorTarget(mixr::models::WorldModel* wm);

}  // namespace app
