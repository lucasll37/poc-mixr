#pragma once

namespace mixr {
namespace models {
namespace xparatrooper {
namespace domain {

//------------------------------------------------------------------------------
// Stage -- as tres fases da vida deste modelo, em ordem cronologica.
//
// FSM de MAO UNICA, nao um Schmitt trigger (ver o comentario de
// ParachuteThreshold::next() logo abaixo): um paraquedista nao "desabre" o
// paraquedas nem "decola" de volta depois de pousar. LANDED e absorvente;
// CANOPY nunca volta a FREEFALL mesmo que a AGL suba (ex.: sobrevoar um
// vale enquanto ainda esta descendo sob o velame).
//------------------------------------------------------------------------------
enum class Stage {
   FREEFALL,   // queda livre -- fisica nativa de arrasto+gravidade do Effect
   CANOPY,     // paraquedas aberto -- taxa de descida constante
   LANDED      // no chao -- inerte, posicao travada
};

//------------------------------------------------------------------------------
// JumpProfile -- os dois limiares que vem do .edl (slots de
// ubf::ParatrooperBtBehavior). Empacotados numa struct, e nao passados soltos
// para next(), porque os dois sempre viajam juntos e crescem juntos (um
// terceiro limiar futuro, ex. altitude minima de emergencia, entraria aqui,
// nao como um parametro a mais espalhado pelas assinaturas).
//------------------------------------------------------------------------------
struct JumpProfile {
   double deployAglM{300.0};   // abre o paraquedas em, ou abaixo de, esta AGL (metros)
   double groundAglM{2.0};     // considera pousado em, ou abaixo de, esta AGL (metros)
};

//------------------------------------------------------------------------------
// next() -- a regra pura desta FSM. Sem MIXR, sem estado global -- testavel
// isolada (ver tests/domain/test_ParachuteFsm.cpp) sem levantar Station
// nenhuma.
//
// Funcao TOTAL: qualquer combinacao de estagio atual + AGL produz um proximo
// estagio, mesmo em configuracao degenerada (groundAglM >= deployAglM) ou com
// AGL negativa (o caminho de "AGL cruzou zero antes do estagio CANOPY
// terminar" -- ver xnative::Paratrooper::crashNotification()).
//
// Quem AVANCA a regra e' ubf::ParatrooperBtBehavior::genAction(), UMA vez por
// ciclo de decisao -- nao um no da arvore (ver bt/DecisionContext.hpp para o
// porque: com ate 2 condicoes avaliadas no mesmo tick, avancar dentro de um
// no duplicaria o avanco).
//------------------------------------------------------------------------------
Stage next(Stage current, double aglM, const JumpProfile& profile);

// Fonte unica de verdade para os rotulos que atravessam a arvore, o xboard e
// o dump '-deterministic' -- evita um enum duplicado em xnative:: e uma
// tabela de mapeamento string<->enum reescrita em mais de um lugar.
const char* labelOf(Stage stage);

// Inverso de labelOf(). Devolve false (sem lancar) para um rotulo
// desconhecido -- o chamador decide o que fazer (ver
// ubf::ParatrooperAction::execute()).
bool stageFromLabel(const char* label, Stage& out);

} // namespace domain
} // namespace xparatrooper
} // namespace models
} // namespace mixr
