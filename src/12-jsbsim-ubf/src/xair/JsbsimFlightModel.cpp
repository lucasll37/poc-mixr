#include "xair/JsbsimFlightModel.hpp"

#include "xair/runtime_utils.hpp"

#include "mixr/models/player/Player.hpp"

#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Number.hpp"
#include "mixr/base/units/distance_utils.hpp"

#include "FGFDMExec.h"

#include <cmath>
#include <exception>

namespace mixr {
namespace xair {

IMPLEMENT_SUBCLASS(JsbsimFlightModel, "JsbsimFlightModel")

BEGIN_SLOTTABLE(JsbsimFlightModel)
   "rootDir",       // 1: raiz dos dados JSBSim
   "model",         // 2: nome da aeronave
   "trimOnReset",   // 3: !=0 => roda o trim no reset
   "debugLevel",    // 4: debug_lvl do JSBSim
   "subSteps",      // 5: passos de integracao por frame
END_SLOTTABLE(JsbsimFlightModel)

BEGIN_SLOT_MAP(JsbsimFlightModel)
   ON_SLOT(1, setSlotRootDir,     base::String)
   ON_SLOT(2, setSlotModel,       base::String)
   ON_SLOT(3, setSlotTrimOnReset, base::Number)
   ON_SLOT(4, setSlotDebugLevel,  base::Number)
   ON_SLOT(5, setSlotSubSteps,    base::Number)
END_SLOT_MAP()

namespace {

const double FT2M{base::distance::FT2M};
const double RAD2DEG{57.295779513082320876798154814105};

// A carga do modelo (parse de varios XML) e serializada: cada aeronave tem
// a SUA FGFDMExec, mas o carregamento toca estruturas compartilhadas da
// biblioteca. O passo de integracao (Run()) e por instancia e roda em
// paralelo sem lock.
std::mutex g_loadMutex;

// Modo de trim do JSBSim: 0 = tLongitudinal (voo reto e nivelado), que e o
// que queremos ao iniciar em cruzeiro.
const int TRIM_LONGITUDINAL{0};

}

JsbsimFlightModel::JsbsimFlightModel()
{
   STANDARD_CONSTRUCTOR()
}

void JsbsimFlightModel::copyData(const JsbsimFlightModel& org, const bool)
{
   BaseClass::copyData(org);

   rootDir = org.rootDir;
   modelName = org.modelName;
   trimOnReset = org.trimOnReset;
   debugLevel = org.debugLevel;
   subSteps = org.subSteps;

   // A FGFDMExec NAO e copiada: a copia cria a sua no proximo reset().
   fdm = nullptr;

   aileronCmd = org.aileronCmd;
   elevatorCmd = org.elevatorCmd;
   rudderCmd = org.rudderCmd;
   throttleCmd = org.throttleCmd;
}

void JsbsimFlightModel::deleteData()
{
   delete fdm;
   fdm = nullptr;
}

bool JsbsimFlightModel::shutdownNotification()
{
   // Convencao do framework (models/System.hpp, nota 3): soltar no
   // SHUTDOWN_EVENT o que a classe criou.
   delete fdm;
   fdm = nullptr;
   return BaseClass::shutdownNotification();
}

//------------------------------------------------------------------------------
// reset() -- (re)cria a FGFDMExec e aplica as condicoes iniciais do Player.
//------------------------------------------------------------------------------
void JsbsimFlightModel::reset()
{
   BaseClass::reset();

   delete fdm;
   fdm = nullptr;

   if (!createFdm()) return;

   applyInitialConditions();
   publishToPlayer();
}

bool JsbsimFlightModel::createFdm()
{
   std::lock_guard<std::mutex> lock(g_loadMutex);

   fdm = new JSBSim::FGFDMExec();
   fdm->SetDebugLevel(debugLevel);

   fdm->SetRootDir(SGPath(rootDir));
   fdm->SetAircraftPath(SGPath("aircraft"));
   fdm->SetEnginePath(SGPath("engine"));
   fdm->SetSystemsPath(SGPath("systems"));

   bool ok{};
   try {
      ok = fdm->LoadModel(modelName);
   } catch (const std::exception& ex) {
      logLine(std::string("[JsbsimFlightModel] excecao ao carregar '") + modelName + "': " + ex.what());
      ok = false;
   }

   if (!ok) {
      logLine("[JsbsimFlightModel] falha ao carregar o modelo '" + modelName + "' de " + rootDir);
      delete fdm;
      fdm = nullptr;
      return false;
   }

   // Semente fixa: o JSBSim tem um gerador aleatorio proprio (usado por
   // turbulencia/vento, desligados aqui). Fixar deixa explicito que nada
   // nesta poc depende de sorteio -- ver o modo '-deterministic'.
   fdm->SetPropertyValue("simulation/randomseed", 1.0);
   return true;
}

void JsbsimFlightModel::applyInitialConditions()
{
   const models::Player* const player{getOwnship()};
   if (player == nullptr || fdm == nullptr) return;

   double lat{}, lon{}, alt{};
   player->getPositionLLA(&lat, &lon, &alt);

   // 'ic/...' sao as condicoes iniciais do JSBSim; RunIC() as aplica sem
   // integrar. A fonte de verdade e o estado que o Player ja montou a
   // partir dos slots init* do .epp -- ou seja, o cenario continua sendo
   // declarado em EDL, como nas demais pocs.
   fdm->SetPropertyValue("ic/lat-geod-deg", lat);
   fdm->SetPropertyValue("ic/long-gc-deg", lon);
   fdm->SetPropertyValue("ic/h-sl-ft", alt / FT2M);
   fdm->SetPropertyValue("ic/psi-true-deg", player->getHeadingD());
   fdm->SetPropertyValue("ic/vt-kts", player->getTotalVelocityKts());
   fdm->SetPropertyValue("ic/gamma-deg", 0.0);

   fdm->RunIC();

   // Liga todos os motores (-1 = todos). Sem isto a aeronave nasce planando.
   fdm->SetPropertyValue("propulsion/set-running", -1.0);

   // GOTCHA DA poc/04 RESOLVIDO AQUI: o JSBSimModel nativo do MIXR chama
   // RunIC() mas NAO roda o trim, e a aeronave comeca destrimada (transiente
   // energetico nos primeiros segundos). Com acesso direto a FGFDMExec da
   // para chamar DoTrim() -- que e deterministico (solver iterativo com as
   // mesmas entradas) e deixa a aeronave estavel desde o primeiro frame.
   if (trimOnReset) {
      try {
         fdm->DoTrim(TRIM_LONGITUDINAL);
      } catch (const std::exception& ex) {
         logLine(std::string("[JsbsimFlightModel] trim nao convergiu: ") + ex.what()
                 + " -- seguindo destrimado");
      }
   }

   fuelCapacityLbs = fdm->GetPropertyValue("propulsion/total-fuel-lbs");
   if (fuelCapacityLbs <= 0.0) fuelCapacityLbs = 1.0;
}

void JsbsimFlightModel::setControls(const double aileron, const double elevator,
                                    const double rudder, const double throttle)
{
   aileronCmd = aileron;
   elevatorCmd = elevator;
   rudderCmd = rudder;
   throttleCmd = throttle;
}

double JsbsimFlightModel::getFuelFraction() const
{
   return (fuelCapacityLbs > 0.0) ? (fuelLbs / fuelCapacityLbs) : 0.0;
}

//------------------------------------------------------------------------------
// FASE 0 -- integra o 6-DOF e publica o resultado no Player.
//------------------------------------------------------------------------------
void JsbsimFlightModel::dynamics(const double dt)
{
   if (fdm == nullptr || dt <= 0.0) return;

   fdm->SetPropertyValue("fcs/aileron-cmd-norm", aileronCmd);
   fdm->SetPropertyValue("fcs/elevator-cmd-norm", elevatorCmd);
   fdm->SetPropertyValue("fcs/rudder-cmd-norm", rudderCmd);
   fdm->SetPropertyValue("fcs/throttle-cmd-norm[0]", throttleCmd);
   fdm->SetPropertyValue("fcs/throttle-cmd-norm[1]", throttleCmd);

   // Subpassos: o frame do MIXR (50 Hz por padrao) e grosso demais para a
   // dinamica rapida de um caca. Integrar N vezes com dt/N mantem o passo
   // FIXO (e portanto deterministico) e melhora a estabilidade numerica.
   const int steps{(subSteps > 0) ? subSteps : 1};
   fdm->Setdt(dt / static_cast<double>(steps));
   for (int i = 0; i < steps; i++) fdm->Run();

   publishToPlayer();
}

//------------------------------------------------------------------------------
// publishToPlayer() -- traduz o estado do JSBSim para o Player do MIXR.
//------------------------------------------------------------------------------
void JsbsimFlightModel::publishToPlayer()
{
   models::Player* const player{getOwnship()};
   if (player == nullptr || fdm == nullptr) return;

   const double lat{fdm->GetPropertyValue("position/lat-geod-deg")};
   const double lon{fdm->GetPropertyValue("position/long-gc-deg")};
   const double altM{fdm->GetPropertyValue("position/h-sl-meters")};

   const double phi{fdm->GetPropertyValue("attitude/phi-rad")};
   const double theta{fdm->GetPropertyValue("attitude/theta-rad")};
   const double psi{fdm->GetPropertyValue("attitude/psi-rad")};

   const double vN{fdm->GetPropertyValue("velocities/v-north-fps") * FT2M};
   const double vE{fdm->GetPropertyValue("velocities/v-east-fps") * FT2M};
   const double vD{fdm->GetPropertyValue("velocities/v-down-fps") * FT2M};

   const double p{fdm->GetPropertyValue("velocities/p-rad_sec")};
   const double q{fdm->GetPropertyValue("velocities/q-rad_sec")};
   const double r{fdm->GetPropertyValue("velocities/r-rad_sec")};

   // 'slaved = true': desliga a integracao de posicao do Player -- quem
   // manda na posicao e o JSBSim (ver o cabecalho desta classe).
   player->setPositionLLA(lat, lon, altM, true);
   player->setEulerAngles(phi, theta, psi);
   player->setVelocity(vN, vE, vD);
   player->setAngularVelocities(p, q, r);

   alphaDeg = fdm->GetPropertyValue("aero/alpha-rad") * RAD2DEG;
   betaDeg = fdm->GetPropertyValue("aero/beta-rad") * RAD2DEG;
   mach = fdm->GetPropertyValue("velocities/mach");
   gLoad = fdm->GetPropertyValue("accelerations/Nz");
   fuelLbs = fdm->GetPropertyValue("propulsion/total-fuel-lbs");
}

//------------------------------------------------------------------------------
// slots
//------------------------------------------------------------------------------
bool JsbsimFlightModel::setSlotRootDir(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   rootDir = msg->getString();
   return true;
}

bool JsbsimFlightModel::setSlotModel(const base::String* const msg)
{
   if (msg == nullptr || msg->getString() == nullptr) return false;
   modelName = msg->getString();
   return true;
}

bool JsbsimFlightModel::setSlotTrimOnReset(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   trimOnReset = (msg->getInt() != 0);
   return true;
}

bool JsbsimFlightModel::setSlotDebugLevel(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   debugLevel = msg->getInt();
   return true;
}

bool JsbsimFlightModel::setSlotSubSteps(const base::Number* const msg)
{
   if (msg == nullptr) return false;
   subSteps = msg->getInt();
   return (subSteps > 0);
}

} // namespace xair
} // namespace mixr
