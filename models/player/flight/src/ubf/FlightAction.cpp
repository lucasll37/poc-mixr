#include "ubf/FlightAction.hpp"

#include "xnative/AlertDatalink.hpp"

#include "xboard/Board.hpp"
#include "xlog/Log.hpp"

#include "mixr/models/WorldModel.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/weapon/AbstractWeapon.hpp"
#include "mixr/models/player/weapon/Missile.hpp"
#include "mixr/models/system/Autopilot.hpp"
#include "mixr/models/system/StoresMgr.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/units/distance_utils.hpp"

#include <map>
#include <mutex>
#include <string>

namespace mixr {
namespace xnative {

namespace {

//------------------------------------------------------------------------------
// LOG(...) DESTE MODELO -- e daqui que vem o conteudo da aba "Log" (F5) do
// ./app.
//
// Nada disso e ponte: shared/xlog e uma shared_library(), entao ha UMA copia
// no processo e o LOG(...) emitido de dentro deste .so (aberto por dlopen)
// cai no mesmo buffer em memoria que o host le -- ver o cabecalho de
// app/LogPanel.hpp e a secao shared/xlog do CLAUDE.md. O log tambem vai pro
// console e pro arquivo das outras pocs (single-thread/multi-thread), que
// nao tem aba nenhuma; sob '-deterministic' o main.cpp desliga tudo
// (setLoggingEnabled(false)), entao os dumps comparaveis nao mudam.
//
// execute() roda a cada decisao ATUADA -- ate 50 Hz por aeronave. Nada aqui
// pode logar por decisao: as duas regras abaixo existem so pra isso.
//------------------------------------------------------------------------------

// 1) Transformar um estado CONTINUO em evento de BORDA: guarda a ultima
//    "chave" vista por player e diz se ela mudou agora. Medido antes de
//    existir: o pedido de broadcast de alerta fica ligado enquanto a
//    aeronave evade, entao logar direto no 'if (broadcast)' dava ~50
//    linhas/s por aeronave -- em 20 s de intercepto o buffer de 500
//    entradas ja tinha girado tres vezes e engolido as transicoes, que sao
//    justamente o que interessa. Com a borda, sai UMA linha por episodio
//    de alerta.
//
//    O mapa e estatico e compartilhado entre as threads do pool T/C (os
//    agentes decidem em paralelo na multi-thread/app), dai o mutex.
bool changedFor(std::map<int, std::string>& last, const int playerId, const std::string& key)
{
   static std::mutex mutex;
   const std::lock_guard<std::mutex> lock(mutex);
   const auto it = last.find(playerId);
   if (it != last.end() && it->second == key) return false;
   last[playerId] = key;
   return true;
}

// 2) Batimento periodico: uma linha a cada N decisoes ATUADAS do player
//    (~10 s a 50 Hz). E a contagem que o xboard ja mantem -- nenhum
//    contador novo, e a cadencia acompanha a taxa de decisao real em vez
//    de tempo de parede.
const long kHeartbeatEveryDecisions{500};

} // namespace

IMPLEMENT_SUBCLASS(FlightAction, "FlightAction")
EMPTY_SLOTTABLE(FlightAction)
EMPTY_DELETEDATA(FlightAction)

FlightAction::FlightAction()
{
   STANDARD_CONSTRUCTOR()
}

void FlightAction::copyData(const FlightAction& org, const bool)
{
   BaseClass::copyData(org);

   command = org.command;
   label = org.label;
   broadcast = org.broadcast;
   alertContactName = org.alertContactName;
   alertNorthM = org.alertNorthM;
   alertEastM = org.alertEastM;
   alertAltitudeM = org.alertAltitudeM;
   alertRangeM = org.alertRangeM;
   launch = org.launch;
   launchTargetName = org.launchTargetName;
}

void FlightAction::setLaunchRequest(const std::string& targetName)
{
   launch = true;
   launchTargetName = targetName;
}

void FlightAction::setAlertBroadcast(const std::string& contactName,
                                     const double northM, const double eastM,
                                     const double altitudeM, const double rangeM)
{
   broadcast = true;
   alertContactName = contactName;
   alertNorthM = northM;
   alertEastM = eastM;
   alertAltitudeM = altitudeM;
   alertRangeM = rangeM;
}

//------------------------------------------------------------------------------
// execute() -- atuacao, inteiramente sobre subsistemas NATIVOS: comanda o
// models::Autopilot do framework, que por sua vez fala com o JSBSimModel
// (ap/heading_hold, ap/altitude_hold, ap/airspeed_hold) -- nenhuma lei de
// controle propria no caminho.
//
// GOTCHA DE UNIDADE: Autopilot::setCommandedAltitudeFt() e em PES, enquanto
// o domain::FlightCommand (e o resto deste subprojeto) trabalha em metros.
// A conversao acontece aqui, na fronteira.
//
// O rotulo do comportamento vai para o quadro de status (ver
// xnative/BehaviorBoard.hpp): o Aircraft nativo nao tem onde guarda-lo.
//------------------------------------------------------------------------------
bool FlightAction::execute(base::Component* actor)
{
   const auto player = dynamic_cast<models::Player*>(actor);
   if (player == nullptr) return false;

   base::Pair* const pilotPair{player->getPilotByType(typeid(models::Autopilot))};
   const auto autopilot = (pilotPair != nullptr)
                           ? dynamic_cast<models::Autopilot*>(pilotPair->object())
                           : nullptr;
   if (autopilot == nullptr) {
      // Ate aqui esta falha era MUDA: o comportamento decidia, o arbitro
      // escolhia, e a atuacao voltava 'false' sem nada em lugar nenhum --
      // a aeronave simplesmente nao obedecia. Uma vez por player (ver
      // firstTimeFor()).
      static std::map<int, std::string> reported;
      if (changedFor(reported, player->getID(), "sem-autopilot")) {
         LOG(ERROR) << "[FlightAction] " << player->getName()->getString()
                    << ": sem Autopilot -- decisao '" << label << "' nao pode ser atuada";
      }
      return false;
   }

   // Estado ANTERIOR do quadro, lido antes de sobrescrever logo abaixo --
   // e o que permite logar a TRANSICAO de comportamento (evento raro) em
   // vez do comportamento corrente (50 Hz por aeronave).
   const xboard::Readout before{xboard::get(player->getID())};

   autopilot->setHeadingHoldMode(true);
   autopilot->setAltitudeHoldMode(true);
   autopilot->setVelocityHoldMode(true);

   autopilot->setCommandedHeadingD(command.headingDeg);
   autopilot->setCommandedAltitudeFt(command.altitudeM * base::distance::M2FT);
   autopilot->setCommandedVelocityKts(command.speedKts);

   // O quadro de leitura (shared/xboard) e a UNICA coisa que este modelo e o
   // host compartilham: escrevemos aqui, o dump e a linha de status leem la.
   // Ele mora numa .so de verdade justamente porque este codigo passou a rodar
   // dentro de um plugin -- ver o cabecalho de shared/xboard/Board.hpp.
   //
   // Conta DECISAO, nao candidatura: estamos depois de o UbfArbiter ter
   // escolhido o vencedor.
   xboard::setBehaviorLabel(player->getID(), label);
   xboard::bumpDecisionCount(player->getID());

   // Transicao de comportamento -- o evento que conta a historia da missao
   // ("falcon1: PATROL -> EVADE"). A primeira decisao de cada aeronave
   // aparece como "-- -> PATROL", que e o valor inicial do quadro.
   if (before.label != label) {
      LOG(INFO) << "[FlightAction] " << player->getName()->getString()
                << ": " << before.label << " -> " << label
                << "  (hdg=" << command.headingDeg
                << "deg alt=" << command.altitudeM
                << "m vel=" << command.speedKts << "kt)";
   }

   // Batimento: prova que a aeronave continua decidindo mesmo sem trocar
   // de comportamento, e da a cadencia real de decisao. Cadenciado pela
   // contagem do proprio quadro (ver kHeartbeatEveryDecisions).
   if (before.decisions > 0 && (before.decisions % kHeartbeatEveryDecisions) == 0) {
      LOG(DEBUG) << "[FlightAction] " << player->getName()->getString()
                 << ": " << before.decisions << " decisoes atuadas, em '" << label
                 << "' (thread " << xboard::threadTag() << ")";
   }

   // Qual thread decidiu -- unico ponto de atuacao comum aos DOIS agentes
   // (o SimAgent nativo, background, e o FlightAgentTC, pool T/C), entao e
   // aqui que o quadro fica correto pros dois: FlightAgentTC::controller()
   // ja escreve o mesmo valor antes de chegar aqui (redundante, inofensivo,
   // mesma tag); o SimAgent nunca escrevia nada -- o campo ficava preso em
   // -1 ("-") pra sempre, nao porque a decisao nao tivesse thread, mas
   // porque ninguem contava qual. threadTag() e por-thread (cache
   // thread_local), entao aqui sai sempre a MESMA tag pras 4 aeronaves --
   // resposta honesta: elas decidem, de fato, todas na mesma thread de
   // background.
   xboard::setThreadTag(player->getID(), xboard::threadTag());

   // O pedido de broadcast fica LIGADO enquanto a aeronave evade -- e
   // estado, nao evento. A linha de log sai so na BORDA: quando comeca a
   // alertar, ou quando troca de contato (ver changedFor()). Sair do
   // alerta zera a chave, entao um episodio novo volta a logar.
   static std::map<int, std::string> lastAlertContact;
   if (broadcast) {
      const auto datalink = dynamic_cast<AlertDatalink*>(player->getDatalink());
      if (datalink != nullptr) {
         datalink->broadcastAlert(alertContactName, alertNorthM, alertEastM,
                                  alertAltitudeM, alertRangeM);
         // WARNING e nivel OPERACIONAL aqui, nao "defeito de software": e
         // literalmente um alerta tatico saindo pro resto da esquadrilha, e
         // e o que se quer enxergar destacado no meio das transicoes.
         if (changedFor(lastAlertContact, player->getID(), alertContactName)) {
            LOG(WARNING) << "[FlightAction] " << player->getName()->getString()
                         << ": alerta tatico -- contato '" << alertContactName
                         << "' a " << (alertRangeM * base::distance::M2NM) << " NM";
         }
      }
   } else {
      changedFor(lastAlertContact, player->getID(), std::string{});
   }

   // --- lancamento de missil -------------------------------------------
   //
   // O UNICO ponto deste modelo que toca um objeto MIXR de arma. StoresMgr e
   // opcional (getStoresManagement() devolve nullptr sem 'stores:' no EDL) --
   // inerte em qualquer aviao de producao.
   //
   // releaseOneMissile() ja faz tudo que o framework nativo oferece: clona o
   // 'missile' do EDL num flyout e o enfileira em Simulation::addNewPlayer()
   // (materializado no proximo updatePlayerList(), no laco de background) --
   // e assim, sem nenhum codigo nosso, que um player novo entra na simulacao
   // EM EXECUCAO. Devolve pre-ref()'d (ver StoresMgr.hpp) -- por isso o
   // unref() no fim.
   if (launch) {
      models::StoresMgr* const storesMgr{player->getStoresManagement()};
      models::WorldModel* const world{player->getWorldModel()};
      if (storesMgr != nullptr && world != nullptr) {
         const auto target = dynamic_cast<models::Player*>(
            world->findPlayerByName(launchTargetName.c_str()));
         if (target != nullptr) {
            models::AbstractWeapon* const flyout{storesMgr->releaseOneMissile()};
            if (flyout != nullptr) {
               flyout->setTargetPlayer(target, true);
               LOG(INFO) << "[FlightAction] " << player->getName()->getString()
                         << ": missil lancado contra '" << launchTargetName
                         << "' (flyout '" << flyout->getName()->getString() << "')";
               flyout->unref();
            } else {
               // Pediu-se lancamento e o cabide esta vazio -- a arvore
               // continuaria pedindo a cada frame sem nada acontecer.
               LOG(WARNING) << "[FlightAction] " << player->getName()->getString()
                            << ": lancamento pedido, mas releaseOneMissile() nao devolveu arma";
            }
         } else {
            LOG(WARNING) << "[FlightAction] " << player->getName()->getString()
                         << ": lancamento pedido contra '" << launchTargetName
                         << "', que nao existe na simulacao";
         }
      }
   }

   return true;
}

} // namespace xnative
} // namespace mixr
