#include "domain/ThreatPolicy.hpp"

#include "domain/geometry.hpp"

namespace domain {

namespace {
const double MIN_SAFE_ALT_M{200.0};   // piso simples de seguranca
}

void ThreatPolicy::reset()
{
   cmd_ = FlightCommand{};
   engaged_ = false;
   contactLive_ = false;
   holdTimer_ = 0.0;
}

//------------------------------------------------------------------------------
// breakCommand() -- o alvo da manobra, calculado UMA vez (ver o .hpp).
//------------------------------------------------------------------------------
FlightCommand ThreatPolicy::breakCommand(const ThreatContact& contact,
                                         const double ownHeadingDeg,
                                         const double ownAltM,
                                         const GroundReference& ground) const
{
   FlightCommand cmd;

   // Marcacao ABSOLUTA do contato: meu rumo mais a marcacao relativa dele.
   const double bearingToContact{wrap360(ownHeadingDeg + contact.relBearingDeg)};

   // Fugir e sair da marcacao dele pela quebra, para o lado oposto ao que
   // ele ocupa: contato a boreste (relBearing >= 0) => quebra a bombordo.
   const double turn{(contact.relBearingDeg >= 0.0) ? -limits_.breakTurnDeg
                                                    : limits_.breakTurnDeg};
   cmd.headingDeg = wrap360(bearingToContact + turn);

   // Desconflito vertical no sentido contrario ao do contato, tambem fixo.
   cmd.altitudeM = (contact.deltaAltM >= 0.0) ? (ownAltM - limits_.climbM)
                                              : (ownAltM + limits_.climbM);

   // ... e nunca abaixo do terreno mais a folga. Sobre relevo de verdade e
   // este piso que decide, nao o absoluto: MIN_SAFE_ALT_M so volta a valer
   // quando nao ha elevacao (ver domain/TerrainFloor.hpp).
   cmd.altitudeM = clampToTerrain(cmd.altitudeM, ground,
                                  limits_.terrainClearanceM, MIN_SAFE_ALT_M);

   cmd.speedKts = limits_.dashSpeedKts;
   return cmd;
}

//------------------------------------------------------------------------------
// update() -- uma vez por ciclo de decisao.
//
// Tres estados, e a transicao entre eles e tudo o que esta classe faz:
//
//    livre            sem contato e sem histerese  -> engaged() == false
//    em manobra       com contato                  -> alvo fixado, timer cheio
//    em arrasto       sem contato, timer > 0       -> mesmo alvo, timer caindo
//------------------------------------------------------------------------------
void ThreatPolicy::update(const double dt, const bool hasContact, const ThreatContact& contact,
                          const double ownHeadingDeg, const double ownAltM,
                          const GroundReference& ground)
{
   contactLive_ = hasContact;

   if (hasContact) {
      // O alvo so e (re)calculado na ENTRADA. Enquanto a manobra estiver
      // valendo, ver o contato de novo apenas re-arma a histerese -- nao
      // move o alvo, que e justamente o que fazia a curva nunca terminar.
      if (!engaged_) {
         cmd_ = breakCommand(contact, ownHeadingDeg, ownAltM, ground);
         engaged_ = true;
      }
      holdTimer_ = limits_.holdSeconds;
      return;
   }

   if (!engaged_) return;

   holdTimer_ -= dt;
   if (holdTimer_ <= 0.0) reset();
}

} // namespace domain
