#include "ubf/Navstar3BtBehavior.hpp"

#include "ubf/Navstar3Action.hpp"
#include "ubf/Navstar3State.hpp"

#include "bt/bt_factory.hpp"

#include "xlog/Log.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/units/Angles.hpp"
#include "mixr/base/units/Distances.hpp"
#include "mixr/base/util/nav_utils.hpp"

#include "mixr/base/osg/Vec3d"

#include <cmath>
#include <mutex>

namespace mixr {
namespace models {
namespace xNavstar_3 {

namespace {

// BT::BehaviorTreeFactory NAO e' thread-safe na CONSTRUCAO da arvore, e um
// cenario com N satelites constroi N arvores -- potencialmente em paralelo,
// se o agente do UBF decidir na fase 3 do frame de tempo critico. Serializar
// so' a construcao e' barato: acontece uma vez por entidade, nunca por tick.
// Mesmo padrao de models/players/A-4/models/players/C-130.
std::mutex g_treeBuildMutex;

constexpr double kDegToRad{3.14159265358979323846 / 180.0};

} // namespace

IMPLEMENT_SUBCLASS(Navstar3BtBehavior, "Navstar3BtBehavior")

// clang-format off
BEGIN_SLOTTABLE(Navstar3BtBehavior)
   "treeFile",           // 1
   "altitude",           // 2
   "inclination",        // 3
   "raan",               // 4
   "argLat0",            // 5
   "sunRightAscension",  // 6
   "sunDeclination",     // 7
END_SLOTTABLE(Navstar3BtBehavior)

BEGIN_SLOT_MAP(Navstar3BtBehavior)
   ON_SLOT(1, setSlotTreeFile,          base::String)
   ON_SLOT(2, setSlotAltitude,          base::Distance)
   ON_SLOT(3, setSlotInclination,       base::Angle)
   ON_SLOT(4, setSlotRaan,              base::Angle)
   ON_SLOT(5, setSlotArgLat0,           base::Angle)
   ON_SLOT(6, setSlotSunRightAscension, base::Angle)
   ON_SLOT(7, setSlotSunDeclination,    base::Angle)
END_SLOT_MAP()

EMPTY_DELETEDATA(Navstar3BtBehavior)

Navstar3BtBehavior::Navstar3BtBehavior()
// clang-format on
{
   STANDARD_CONSTRUCTOR()
}

void Navstar3BtBehavior::copyData(const Navstar3BtBehavior& org, const bool)
{
   BaseClass::copyData(org);
   orbit_ = org.orbit_;
   sunRaDeg_ = org.sunRaDeg_;
   sunDecDeg_ = org.sunDecDeg_;
   simTimeS_ = org.simTimeS_;
   sunState_ = org.sunState_;
   decision_ = org.decision_;
   ecefXM_ = org.ecefXM_; ecefYM_ = org.ecefYM_; ecefZM_ = org.ecefZM_;
   ecefVelXMps_ = org.ecefVelXMps_; ecefVelYMps_ = org.ecefVelYMps_; ecefVelZMps_ = org.ecefVelZMps_;
   prevEcefXM_ = org.prevEcefXM_; prevEcefYM_ = org.prevEcefYM_; prevEcefZM_ = org.prevEcefZM_;
   havePreviousEcef_ = org.havePreviousEcef_;
   treeFile_ = org.treeFile_;

   // A arvore NAO e' copiada -- cada no carrega um NodeContext apontando
   // para o comportamento que o hospeda; copiar traria ponteiros para o
   // OUTRO objeto. Reconstruir e' o unico caminho correto.
   treeBuilt_ = false;
   treeValid_ = false;
   tree_ = BT::Tree();
}

void Navstar3BtBehavior::reset()
{
   BaseClass::reset();

   // Epoca da orbita = inicio do cenario. Sem isto, um RESET_EVENT no meio
   // de uma execucao (ex.: "reiniciar" no ./app) reaplicaria o simTimeS_
   // acumulado sobre uma orbita que acabou de "nascer de novo".
   simTimeS_ = 0.0;
   sunState_ = domain::SunState::SUNLIT;
   havePreviousEcef_ = false;
   decision_.reset();

   tree_ = BT::Tree();
   treeBuilt_ = false;
   treeValid_ = false;
}

//------------------------------------------------------------------------------
// buildTree() -- uma tentativa so' por ciclo de vida (ou por reset()).
//------------------------------------------------------------------------------
void Navstar3BtBehavior::buildTree()
{
   treeBuilt_ = true;

   if (treeFile_.empty()) {
      LOG(WARNING) << "[Navstar3BtBehavior] slot 'treeFile' vazio -- arvore nao construida";
      return;
   }

   bt::NodeContext context;
   context.behavior = this;

   std::lock_guard<std::mutex> lock(g_treeBuildMutex);

   btFactory_ = BT::BehaviorTreeFactory();
   bt::registerNodes(btFactory_, context);

   try {
      tree_ = btFactory_.createTreeFromFile(treeFile_, BT::Blackboard::create());
      treeValid_ = true;
   } catch (const std::exception& ex) {
      LOG(ERROR) << "[Navstar3BtBehavior] falha ao carregar a arvore '" << treeFile_
                 << "': " << ex.what();
      treeValid_ = false;
   }
}

//------------------------------------------------------------------------------
// genAction() -- chamado uma vez por ciclo de decisao pelo Agent do UBF.
//
// Propaga a orbita (referencial fixo na Terra, para atuacao) e a geometria
// sol/sombra (referencial inercial, para a arvore), tica a arvore, e devolve
// uma Navstar3Action carregando o resultado -- quem de fato move o Player e'
// a Action, nao esta classe (percepcao/decisao nunca escrevem no MIXR
// diretamente, so' leem via Navstar3State).
//------------------------------------------------------------------------------
base::ubf::AbstractAction* Navstar3BtBehavior::genAction(const base::ubf::AbstractState* const state,
                                                         const double dt)
{
   const auto s = dynamic_cast<const Navstar3State*>(state);
   if (s == nullptr || !s->hasReading()) return nullptr;

   if (dt > 0.0) simTimeS_ += dt;

   // --- posicao/velocidade, referencial fixo na Terra (para atuacao) ------
   const domain::GroundTrack gt{domain::groundTrack(orbit_, simTimeS_)};

   double x{}, y{}, z{};
   base::nav::convertGeod2Ecef(gt.latDeg, gt.lonDeg, gt.altM, &x, &y, &z);

   if (havePreviousEcef_ && dt > 0.0) {
      ecefVelXMps_ = (x - prevEcefXM_) / dt;
      ecefVelYMps_ = (y - prevEcefYM_) / dt;
      ecefVelZMps_ = (z - prevEcefZM_) / dt;
   } else {
      ecefVelXMps_ = 0.0; ecefVelYMps_ = 0.0; ecefVelZMps_ = 0.0;
   }
   ecefXM_ = x; ecefYM_ = y; ecefZM_ = z;
   prevEcefXM_ = x; prevEcefYM_ = y; prevEcefZM_ = z;
   havePreviousEcef_ = true;

   // --- sol/sombra, referencial INERCIAL (NUNCA o ground-track acima --
   // ver a armadilha documentada em domain/EclipseGeometry.hpp) ------------
   const domain::Vec3 eciPos{domain::eciPosition(orbit_, simTimeS_)};
   const double raRad{sunRaDeg_ * kDegToRad};
   const double decRad{sunDecDeg_ * kDegToRad};
   const domain::Vec3 sunDir{
      std::cos(decRad) * std::cos(raRad),
      std::cos(decRad) * std::sin(raRad),
      std::sin(decRad)
   };
   sunState_ = domain::sunState(eciPos, sunDir);

   if (!treeBuilt_) buildTree();

   decision_.reset();

   if (treeValid_) {
      tree_.tickRoot();
   } else {
      // DEGRADACAO, nao falha: sem arvore carregada o modelo continua
      // decidindo pela MESMA geometria, direto -- o satelite continua
      // orbitando (a posicao/velocidade acima ja foram calculadas) e o
      // rotulo ainda reflete sol/sombra de verdade.
      decision_.take(domain::labelOf(sunState_));
   }

   const auto action = new Navstar3Action(decision_.label,
                                          base::Vec3d(ecefXM_, ecefYM_, ecefZM_),
                                          base::Vec3d(ecefVelXMps_, ecefVelYMps_, ecefVelZMps_));
   action->setVote(getVote());
   return action;
}

bool Navstar3BtBehavior::setSlotTreeFile(const base::String* const msg)
{
   // getString(), nao c_str(): base::String guarda um 'char*' PRIVADO e
   // expoe so' este getter (mais um operator const char*). Pode voltar
   // nullptr num objeto default-construido.
   if (msg == nullptr || msg->getString() == nullptr) return false;
   treeFile_ = msg->getString();
   return true;
}

bool Navstar3BtBehavior::setSlotAltitude(const base::Distance* const msg)
{
   if (msg == nullptr) return false;
   orbit_.altitudeM = base::Meters::convertStatic(*msg);
   return true;
}

bool Navstar3BtBehavior::setSlotInclination(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   orbit_.inclinationDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool Navstar3BtBehavior::setSlotRaan(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   orbit_.raanDeg = base::Degrees::convertStatic(*msg);
   return true;
}

bool Navstar3BtBehavior::setSlotArgLat0(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   orbit_.argLatDeg0 = base::Degrees::convertStatic(*msg);
   return true;
}

bool Navstar3BtBehavior::setSlotSunRightAscension(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   sunRaDeg_ = base::Degrees::convertStatic(*msg);
   return true;
}

bool Navstar3BtBehavior::setSlotSunDeclination(const base::Angle* const msg)
{
   if (msg == nullptr) return false;
   sunDecDeg_ = base::Degrees::convertStatic(*msg);
   return true;
}

} // namespace xNavstar_3
} // namespace models
} // namespace mixr
