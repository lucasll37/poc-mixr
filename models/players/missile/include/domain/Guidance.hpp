#pragma once

// namespace ANINHADO em mixr::models::xmissile -- nao um "domain" solto no
// escopo global. Mesmo motivo ja documentado no template de origem: dois
// tipos com o MESMO nome qualificado ("domain::Foo") em dois .so's
// distintos, no mesmo processo, teriam o mesmo simbolo mangled -- a
// comparacao de type_info deste toolchain degrada para strcmp entre objetos
// RTLD_LOCAL. `flight` (models/players/A-4) usa domain:: solto porque chegou
// primeiro; este modelo nao repete a excecao.
namespace mixr {
namespace models {
namespace xmissile {
namespace domain {

//------------------------------------------------------------------------------
// Vec3 -- vetor NED (norte, leste, baixo). O campo que representa depende do
// contexto de quem chama: posicao relativa (metros) ou velocidade relativa
// (m/s). Sem MIXR: nao e base::Vec3d de proposito -- esta camada nao inclui
// nenhum header do framework (ver docs/ARCHITECTURE.md).
//------------------------------------------------------------------------------
struct Vec3
{
   double n{};
   double e{};
   double d{};
};

struct GuidanceCommand
{
   double cmdHeadingRad{};
   double cmdPitchRad{};
   double cmdSpeedMps{};
};

struct GuidanceGains
{
   // N' da navegacao proporcional -- tipico 3..5. Quanto maior, mais forte a
   // correcao em cima da taxa da linha de visada (mais agressivo contra um
   // alvo em curva, mais sensivel a ruido se a leitura de posicao/velocidade
   // do alvo for ruidosa -- aqui nao e, ver TargetData no FlightAction).
   double navRatio{4.0};

   // Velocidade de cruzeiro comandada (m/s) -- weaponDynamics() em
   // xnative/GuidedMissile.cpp converge a velocidade real ate aqui, limitado
   // por maxAccel (slot herdado de mixr::models::Missile).
   double cruiseSpeedMps{260.0};
};

//------------------------------------------------------------------------------
// proportionalNavigation() -- perseguicao pura (aponta direto pro alvo, pelo
// angulo da linha de visada) somada a correcao proporcional a TAXA da linha
// de visada (o termo classico N' x taxaLOS da navegacao proporcional de
// verdade). E o que separa isto de uma perseguicao ingenua: perseguicao pura
// sozinha sempre mira onde o alvo ESTA agora, entao contra um alvo em curva
// o missil fica cronicamente atras da trajetoria; corrigir pela taxa da LOS
// adianta a rota ANTES do erro angular crescer.
//
// relPos/relVel: ALVO MENOS PROPRIO, em NED (metros / m/s) -- a mesma
// convencao que mixr::models::Player::getPosition()/getVelocity() usam, so
// que ja subtraidas (quem chama, em xnative/GuidedMissile.cpp, calcula
// alvo->getPosition() - getPosition() antes de passar aqui).
//
// Pura: sem estado, sem MIXR. Testada em tests/domain/test_Guidance.cpp
// simulando um alvo em movimento e conferindo que o alcance de menor
// aproximacao cai com o tempo.
//------------------------------------------------------------------------------
GuidanceCommand proportionalNavigation(const Vec3& relPos, const Vec3& relVel, const GuidanceGains& gains);

//------------------------------------------------------------------------------
// Espoleta de proximidade -- estado explicito, passado e devolvido (nao
// guardado aqui): quem chama (xnative::GuidedMissile) mantem o FuzeState
// entre frames como membro privado.
//------------------------------------------------------------------------------
struct FuzeState
{
   bool hasSample{false};
   bool wasApproaching{false};
};

struct FuzeOutcome
{
   // true no primeiro frame em que o alcance PARA de diminuir -- o instante
   // do ponto de menor aproximacao.
   bool closestApproachReached{false};

   // so' significa algo quando closestApproachReached == true: dentro de
   // burstRangeM nesse instante?
   bool hit{false};

   double rangeAtEventM{0.0};

   FuzeState nextState{};
};

//------------------------------------------------------------------------------
// proximityFuze() -- detecta a transicao "aproximando -> nao mais
// aproximando" comparando o sinal da taxa de alcance ATUAL contra o sinal do
// frame anterior (por isso o estado explicito: um frame so' nao basta,
// precisa de dois pontos pra saber se HOUVE transicao). "hit" so' e' true se
// o alcance nesse instante de transicao estiver dentro de burstRangeM --
// caso contrario e' autodestruicao por ter passado do alvo sem acertar
// (mesmo comportamento de mixr::models::Missile::weaponGuidance() nativo:
// sempre detona ao passar do ponto de menor aproximacao, com ou sem
// acerto -- so' o resultado do detonation muda).
//
// Chame a cada frame com o (relPos,relVel) do frame atual e o FuzeState
// devolvido na chamada anterior (comece com FuzeState{} no primeiro frame
// depois do lancamento).
//------------------------------------------------------------------------------
FuzeOutcome proximityFuze(const Vec3& relPos, const Vec3& relVel, double burstRangeM, const FuzeState& prev);

} // namespace domain
} // namespace xmissile
} // namespace models
} // namespace mixr
