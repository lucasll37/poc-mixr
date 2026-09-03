#include "xtacview/TacviewOutput.hpp"

#include "mixr/recorder/DataRecordHandle.hpp"
#include "mixr/simulation/dataRecorderTokens.hpp"

#include "mixr/base/Pair.hpp"
#include "mixr/base/PairStream.hpp"
#include "mixr/base/String.hpp"
#include "mixr/base/numeric/Integer.hpp"
#include "mixr/base/util/nav_utils.hpp"

#include "mixr/simulation/Station.hpp"
#include "mixr/simulation/Simulation.hpp"
#include "mixr/models/player/Player.hpp"
#include "mixr/models/player/effect/Chaff.hpp"
#include "mixr/models/player/effect/Decoy.hpp"
#include "mixr/models/player/effect/Flare.hpp"
#include "mixr/models/player/weapon/Bomb.hpp"
#include "mixr/models/player/weapon/Bullet.hpp"
#include "mixr/models/player/weapon/Missile.hpp"

#include "DataRecord.pb.h"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace mixr {
namespace xtacview {

IMPLEMENT_SUBCLASS(TacviewOutput, "TacviewOutput")

BEGIN_SLOTTABLE(TacviewOutput)
   "host",        // 1: IP de escuta
   "port",        // 2: porta Real-Time Telemetry
   "fileName",    // 3: arquivo .acmi a gravar
   "callsign",    // 4: nome do host no handshake
   "typeMap",     // 5: type: do Player -> tag "Type=" do ACMI
   "colorMap",    // 6: side do Player -> "Color=" do ACMI
   "modelMap",    // 7: nome/type: do Player -> modelo "Name=" do ACMI
END_SLOTTABLE(TacviewOutput)

BEGIN_SLOT_MAP(TacviewOutput)
   ON_SLOT(1, setSlotHost,     base::String)
   ON_SLOT(2, setSlotPort,     base::Integer)
   ON_SLOT(3, setSlotFileName, base::String)
   ON_SLOT(4, setSlotCallsign, base::String)
   ON_SLOT(5, setSlotTypeMap,  base::PairStream)
   ON_SLOT(6, setSlotColorMap, base::PairStream)
   ON_SLOT(7, setSlotModelMap, base::PairStream)
END_SLOT_MAP()

EMPTY_DELETEDATA(TacviewOutput)

TacviewOutput::TacviewOutput()
{
   STANDARD_CONSTRUCTOR()
}

void TacviewOutput::copyData(const TacviewOutput& org, const bool)
{
   BaseClass::copyData(org);

   host = org.host;
   port = org.port;
   fileName = org.fileName;
   callsign = org.callsign;
   typeMap = org.typeMap;
   colorMap = org.colorMap;
   modelMap = org.modelMap;

   // O socket/arquivo NAO sao copiados: a copia reabre os seus.
   initialized = false;
   initFailed = false;
   frameOpen = false;
   currentFrameTime = -1.0;
   declared.clear();
   resolvedCache.clear();
}

//------------------------------------------------------------------------------
// Mapeamento default de tipo ACMI por major type do Player. O slot 'typeMap'
// (por type: do player) tem precedencia sobre isto.
//
// NAO existe tag ACMI oficial para satelite/espaconave -- a taxonomia do
// Tacview tem Air/Ground/Sea/Weapon/Sensor/Navaid/Misc e nada de espaco --
// entao SPACE_VEHICLE cai em "Misc", que e uma aproximacao honesta.
//------------------------------------------------------------------------------
namespace {

//------------------------------------------------------------------------------
// Tipo ACMI a partir da CLASSE C++ do player -- so possivel quando ha um
// Player vivo em maos (publishIdentities()); o caminho do protobuf continua
// caindo em defaultTypeForMajor(), abaixo.
//
// POR QUE PRECISA SER POR CLASSE: chaff, flare e decoy derivam de
// models::Effect, que deriva de AbstractWeapon -- logo getMajorType() devolve
// WEAPON para os tres, e o mapeamento por major type sozinho mandaria um
// chaff para o Tacview como "Weapon+Missile". A ordem dos dynamic_cast
// abaixo importa: do mais derivado para o menos (Sam deriva de Missile;
// Chaff/Flare/Decoy derivam de Effect).
//------------------------------------------------------------------------------
std::string defaultTypeForPlayer(const models::Player* const player)
{
   if (dynamic_cast<const models::Chaff*>(player)  != nullptr) return "Misc+Decoy+Chaff";
   if (dynamic_cast<const models::Flare*>(player)  != nullptr) return "Misc+Decoy+Flare";
   if (dynamic_cast<const models::Decoy*>(player)  != nullptr) return "Misc+Decoy";
   if (dynamic_cast<const models::Bomb*>(player)   != nullptr) return "Weapon+Bomb";
   if (dynamic_cast<const models::Bullet*>(player) != nullptr) return "Weapon+Bullet";
   if (dynamic_cast<const models::Missile*>(player) != nullptr) return "Weapon+Missile";
   return {};   // nao e arma/efeito: quem chama cai no default por major type
}

std::string defaultTypeForMajor(const unsigned int majorType)
{
   switch (majorType) {
      case 0x02: return "Air+FixedWing";      // AIR_VEHICLE
      case 0x04: return "Ground+Vehicle";     // GROUND_VEHICLE
      case 0x08: return "Weapon+Missile";     // WEAPON
      case 0x10: return "Sea+Watercraft";     // SHIP
      case 0x20: return "Ground+Static";      // BUILDING
      case 0x40: return "Ground+Infantry";    // LIFE_FORM
      case 0x80: return "Misc";               // SPACE_VEHICLE (sem tag oficial)
      default:   return "Misc";
   }
}

//------------------------------------------------------------------------------
// Cor ACMI por lado (models::Player::Side, Player.hpp:377-384).
//
// ARMADILHA CORRIGIDA AQUI -- "Grey" NAO e uma cor valida do formato: o ACMI
// aceita Red / Orange / Yellow / Green / Cyan / Blue / Violet, e so. O valor
// antigo era o default de TUDO que nao fosse BLUE/RED, incluindo o missil
// liberado em runtime; o Tacview descarta a propriedade e o objeto fica sem
// cor de lado nenhuma. Os seis valores do enum agora tem cada um a sua, e o
// default cai em Violet (uma cor real, visivelmente "nao e nem azul nem
// vermelho") em vez de uma string invalida.
//------------------------------------------------------------------------------
std::string defaultColorForSide(const unsigned int side)
{
   switch (side) {
      case 0x01: return "Blue";     // BLUE
      case 0x02: return "Red";      // RED
      case 0x04: return "Yellow";   // YELLOW -- 3a forca
      case 0x08: return "Cyan";     // CYAN   -- 4a forca
      case 0x10: return "Green";    // GRAY   -- neutro (nao ha cinza no ACMI)
      case 0x20: return "Green";    // WHITE  -- civil/comercial
      default:   return "Violet";
   }
}

// Busca no mapa do EDL por 'type:' e depois por NOME do player, caindo no
// default. As duas chaves existem porque os cenarios deste repositorio usam
// as DUAS convencoes: single-thread/multi-thread mapeiam por nome
// (falcon1: "Air+FixedWing"), e a chave documentada no slot e o 'type:'.
// Chave vazia nunca casa (uma base::String vazia devolve ponteiro nulo --
// ver a nota em resolveInfo()).
std::string lookupOr(const std::map<std::string, std::string>& table,
                     const std::string& typeKey, const std::string& nameKey,
                     const std::string& fallback)
{
   if (!typeKey.empty()) {
      const auto it = table.find(typeKey);
      if (it != table.end()) return it->second;
   }
   if (!nameKey.empty()) {
      const auto it = table.find(nameKey);
      if (it != table.end()) return it->second;
   }
   return fallback;
}

}

//------------------------------------------------------------------------------
// Resolucao de 'Type='/'Color=' ACMI.
//
// ARMADILHA CONFIRMADA RODANDO -- REID_PLAYER_DATA traz um PlayerId
// PARCIAL: so 'id' e 'name'. 'ac_type', 'major_type' e 'side' NAO vem
// preenchidos, e REID_NEW_PLAYER (que traria o PlayerId completo) nunca e
// emitido para os players que ja nascem declarados no .epp -- ele so
// dispara para entidades criadas em runtime (armas liberadas, entidades
// que chegam pela rede).
//
// Por isso a chave dos mapas casa por 'ac_type' QUANDO existe (caso das
// armas/efeitos, via REID_WEAPON_RELEASED) e cai para o 'name' do player,
// que e o unico campo sempre disponivel. Os defaults por major_type/side
// ficam como ultimo recurso. Essa precedencia inteira mora em
// resolveFrom(), mais abaixo -- e a MESMA usada por publishIdentities(),
// que e por onde a identidade de verdade chega hoje.
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// Texto do evento de contato de radar, com o que o TrackData nativo oferecer.
//------------------------------------------------------------------------------
std::string TacviewOutput::trackContactText(const std::string& trackId,
                                            const recorder::pb::PlayerId* const tgt,
                                            const recorder::pb::TrackData* const data) const
{
   std::ostringstream text;
   text << "Radar contact " << trackId;
   if (tgt != nullptr && tgt->has_name()) text << ": " << tgt->name();
   if (data != nullptr) {
      text << std::fixed << std::setprecision(1);
      if (data->has_range())    text << " range=" << (data->range() / 1852.0) << "NM";
      if (data->has_true_az())  text << " bearing=" << (data->true_az() * base::angle::R2DCC) << "deg";
   }
   return text.str();
}

//------------------------------------------------------------------------------
// resolveInfo() -- 'Type='/'Color=' a partir do Player real.
//
// Consultado uma unica vez por objeto (o resultado fica em cache): sobe ate
// a Station pelo encadeamento de containers, pega a lista de players do
// WorldModel e casa pelo id. Isso cobre o que o registro protobuf nao da:
// os players do cenario nao trazem 'type'/'side' em REID_PLAYER_DATA, e os
// objetos criados em runtime (chaff, flare, misseis) tem nome automatico.
//
// O slot 'typeMap'/'colorMap' continua tendo precedencia -- e como se
// sobrepoe um tipo especifico sem tocar em C++.
//
// LIMITACAO CONFIRMADA RODANDO -- esta busca so funciona se a cadeia de
// containers chegar ate a Station, e ELA NAO CHEGA: o DataRecorder nao
// chama container() no objeto do seu slot 'outputHandler', entao daqui
// enxerga-se no maximo o RecorderOutputHandler pai (findContainerByType
// para Station e para AbstractDataRecorder devolve nullptr).
//
// Na pratica, portanto, quem resolve tipo/cor hoje e o fallback por
// NOME abaixo (typeMap/colorMap do EDL). Objetos criados em runtime --
// chaff, flare, misseis -- recebem nomes automaticos ("W10001") e caem
// no default por major_type. O codigo de consulta fica porque passa a
// funcionar sozinho se o encadeamento for corrigido no framework.
//------------------------------------------------------------------------------
TacviewOutput::ResolvedInfo TacviewOutput::resolveFrom(const std::string& name,
                                                       const std::string& type,
                                                       const unsigned int majorType,
                                                       const unsigned int side,
                                                       const models::Player* const player) const
{
   // Com um Player em maos da pra distinguir chaff de missil, o que o major
   // type sozinho nao permite (os dois sao WEAPON) -- ver defaultTypeForPlayer().
   std::string fallbackType{player != nullptr ? defaultTypeForPlayer(player) : std::string{}};
   if (fallbackType.empty()) fallbackType = defaultTypeForMajor(majorType);

   ResolvedInfo info;
   info.type  = lookupOr(typeMap,  type, name, fallbackType);
   info.color = lookupOr(colorMap, type, name, defaultColorForSide(side));
   // Sem entrada no modelMap, o proprio 'type:' do EDL serve de modelo: se
   // ja for uma designacao conhecida do Tacview ("F-16C"), acerta; se nao
   // for, o Tacview cai na forma generica do Type= -- que ainda e a
   // categoria certa (ver ObjectInfo em RealtimeTelemetryServer.hpp).
   info.model = lookupOr(modelMap, type, name, type);
   info.valid = true;
   return info;
}

//------------------------------------------------------------------------------
// publishIdentities() -- ver o "porque" completo no cabecalho do .hpp.
//
// Varre os players vivos e grava a identidade REAL de cada um no cache que
// emitState() consulta.
//
// LIMITE QUE ISTO **NAO** RESOLVE, e por isso a ordem de chamada importa
// tanto: Name/Type/Color sao emitidos UMA UNICA VEZ por objeto, na primeira
// aparicao dele em cada destino ('declared' aqui, 'knownObjectsSocket_'/
// 'knownObjectsFile_' no servidor). Chegar com a identidade DEPOIS da
// primeira declaracao nao corrige o que ja foi escrito: o arquivo .acmi
// guarda a linha errada para sempre, e o socket so redeclara na RECONEXAO
// de um cliente (acceptIfNeeded() limpa 'knownObjectsSocket_'). Por isso
// publicar ANTES de station->updateData(dt) -- que e quem drena a fila e
// declara -- nao e detalhe de estilo: e o que faz a identidade estar no
// lugar quando a unica declaracao acontece.
//------------------------------------------------------------------------------
void TacviewOutput::publishIdentities(const simulation::Simulation* const sim)
{
   if (sim == nullptr) return;

   const base::PairStream* players{sim->getPlayers()};
   if (players == nullptr) return;

   for (const base::List::Item* item = players->getFirstItem(); item != nullptr; item = item->getNext()) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      const auto player = dynamic_cast<const models::Player*>(pair->object());
      if (player == nullptr) continue;

      // base::String::getString() devolve o ponteiro CRU, nulo numa String
      // vazia -- nunca construir std::string dele sem checar (foi assim que
      // o DataRecorder nativo abortou; mesma nota em resolveInfo()).
      const base::String* const typeStr{player->getType()};
      const char* const typeChars{typeStr != nullptr ? typeStr->getString() : nullptr};
      const base::String* const nameStr{player->getName()};
      const char* const nameChars{nameStr != nullptr ? nameStr->getString() : nullptr};

      resolvedCache[static_cast<std::uint32_t>(player->getID())] =
         resolveFrom(nameChars != nullptr ? nameChars : "",
                     typeChars != nullptr ? typeChars : "",
                     player->getMajorType(), player->getSide(), player);
   }
   players->unref();
}

const TacviewOutput::ResolvedInfo& TacviewOutput::resolveInfo(const recorder::pb::PlayerId& id)
{
   const std::uint32_t key{id.id()};
   auto cached = resolvedCache.find(key);
   if (cached != resolvedCache.end()) return cached->second;

   ResolvedInfo info;

   const auto station = static_cast<const simulation::Station*>(
      findContainerByType(typeid(simulation::Station)));
   const simulation::Simulation* sim{station != nullptr ? station->getSimulation() : nullptr};

   if (sim != nullptr) {
      const base::PairStream* players{sim->getPlayers()};
      if (players != nullptr) {
         const base::List::Item* item{players->getFirstItem()};
         while (item != nullptr && !info.valid) {
            const auto pair = static_cast<const base::Pair*>(item->getValue());
            const auto player = dynamic_cast<const models::Player*>(pair->object());
            if (player != nullptr && static_cast<std::uint32_t>(player->getID()) == key) {
               // String::getString() devolve o ponteiro cru, que pode ser
               // nulo numa String vazia -- nunca construir std::string dele
               // sem checar (foi assim que o DataRecorder nativo abortou).
               const base::String* const typeStr{player->getType()};
               const char* const typeChars{typeStr != nullptr ? typeStr->getString() : nullptr};
               const base::String* const nameStr{player->getName()};
               const char* const nameChars{nameStr != nullptr ? nameStr->getString() : nullptr};

               info = resolveFrom(nameChars != nullptr ? nameChars : "",
                                  typeChars != nullptr ? typeChars : "",
                                  player->getMajorType(), player->getSide(), player);
            }
            item = item->getNext();
         }
         players->unref();
      }
   }

   if (!info.valid) {
      // Nao achado: so o que o protobuf deu. Note que 'ac_type'/'major_type'/
      // 'side' quase sempre faltam em REID_PLAYER_DATA -- e por isso que
      // publishIdentities() existe.
      info = resolveFrom(id.has_name() ? id.name() : std::string{},
                         id.has_ac_type() ? id.ac_type() : std::string{},
                         id.has_major_type() ? id.major_type() : 0,
                         id.has_side() ? id.side() : 0);
   }

   resolvedCache[key] = info;
   return resolvedCache[key];
}

//------------------------------------------------------------------------------
// initIfNeeded() -- socket e arquivo, INDEPENDENTES um do outro.
//
// ARMADILHA CORRIGIDA AQUI (encontrada rodando): a versao anterior fazia
// 'return' assim que server.start() falhava, e com isso NAO abria o .acmi --
// uma porta ocupada matava tambem a gravacao local, que nao tem nada a ver
// com o socket. E acontece de verdade neste repositorio: basta uma segunda
// poc (ou uma segunda instancia do ./app) ja escutando a mesma porta e a
// missao inteira era perdida em silencio, com a unica pista num std::cerr
// que o FTXUI engole.
//
// Agora o transporte de rede e a gravacao sao tentados separadamente, e
// 'initialized' passa a significar "ha ALGUM destino" -- que e exatamente o
// que server.isActive() ja testava no resto da classe. 'initFailed' fica
// para o caso de nenhum dos dois subir.
//------------------------------------------------------------------------------
void TacviewOutput::initIfNeeded()
{
   if (initialized || initFailed) return;

   const bool socketUp{server.start(host, port, callsign)};
   const bool fileUp{!fileName.empty() && server.startRecording(fileName)};

   if (!socketUp && !fileUp) {
      initFailed = true;
      return;
   }
   initialized = true;
}

void TacviewOutput::syncFrame(const double simTimeSec)
{
   if (!frameOpen || std::fabs(simTimeSec - currentFrameTime) > 1.0e-9) {
      server.beginFrame(simTimeSec);
      currentFrameTime = simTimeSec;
      frameOpen = true;
   }
}

std::uint32_t TacviewOutput::declareObject(const recorder::pb::PlayerId& id,
                                           const recorder::pb::PlayerState* const state)
{
   const std::uint32_t objectId{id.id()};

   if (declared.find(objectId) == declared.end() && state != nullptr) {
      declared[objectId] = true;

      // A primeira linha ja leva posicao: o Tacview precisa de um T= junto
      // da declaracao para posicionar o objeto.
      emitState(id, *state);
   }
   return objectId;
}

void TacviewOutput::emitState(const recorder::pb::PlayerId& id,
                              const recorder::pb::PlayerState& state)
{
   if (!state.has_pos()) return;

   // PlayerState.pos e ECEF em metros (confirmado: magnitude ~6.37e6). O
   // TabPrinter nativo tambem converte para lat/lon/alt so na hora de exibir.
   const auto& p = state.pos();
   double lat{}, lon{}, alt{};
   base::nav::convertEcef2Geod(p.x(), p.y(), p.z(), &lat, &lon, &alt);

   // ARMADILHA CONFIRMADA RODANDO -- PlayerState.angles sao os Euler
   // GEOCENTRICOS (body/ECEF), nao os geodeticos (body/NED) que o Tacview
   // espera. Sem converter, uma aeronave nivelada em lat 37 / lon -116
   // aparecia com roll=-180, pitch=-53 (=90-37), yaw=64 (=180-116) -- os
   // angulos estavam medindo a posicao no globo, nao a atitude.
   double rollDeg{}, pitchDeg{}, yawDeg{};
   if (state.has_angles()) {
      const auto& a = state.angles();
      const base::Vec2d ll(lat, lon);
      const base::Vec3d ecefAngles(a.x(), a.y(), a.z());
      base::Vec3d nedAngles;
      if (base::nav::convertEcefAngles2GeodAngles(ll, ecefAngles, &nedAngles)) {
         rollDeg  = nedAngles[0] * base::angle::R2DCC;
         pitchDeg = nedAngles[1] * base::angle::R2DCC;
         yawDeg   = nedAngles[2] * base::angle::R2DCC;
      }
   }

   const bool first{declared.find(id.id()) == declared.end()};
   if (first) declared[id.id()] = true;

   const ResolvedInfo& resolved{resolveInfo(id)};
   const ObjectInfo info{
      resolved.model,                                        // Name=  (modelo)
      resolved.type,                                         // Type=
      resolved.color,                                        // Color=
      id.has_name() ? id.name() : std::string()              // CallSign=/Pilot=
   };

   server.updateObject(id.id(), lon, lat, alt, rollDeg, pitchDeg, yawDeg,
                       first ? &info : nullptr);
}

void TacviewOutput::updateRadarScan(const std::uint32_t playerId, const double simTimeSec,
                                    const double azimuthDeg, const double elevationDeg,
                                    const double rangeM,
                                    const double hBeamwidthDeg, const double vBeamwidthDeg)
{
   initIfNeeded();
   if (!initialized) return;

   server.acceptIfNeeded();
   if (!server.isActive()) return;

   if (declared.find(playerId) == declared.end()) return;   // ainda sem T= -- Tacview nao conhece o id

   syncFrame(simTimeSec);
   server.updateRadarBeam(playerId, azimuthDeg, elevationDeg, rangeM, hBeamwidthDeg, vBeamwidthDeg);
}

//------------------------------------------------------------------------------
// processRecordImp() -- traduz cada DataRecord nativo em linhas ACMI.
//
// Chamado pela cadeia de OutputHandlers (OutputHandler::processRecord), a
// partir da thread de background que drena a fila -- nao da thread T/C.
//------------------------------------------------------------------------------
void TacviewOutput::processRecordImp(const recorder::DataRecordHandle* const handle)
{
   if (handle == nullptr) return;

   const recorder::pb::DataRecord* rec{handle->getRecord()};
   if (rec == nullptr) return;

   initIfNeeded();
   if (!initialized) return;

   server.acceptIfNeeded();
   if (!server.isActive()) return;

   const double t{rec->has_time() ? rec->time().exec_time() : 0.0};


   switch (rec->id()) {

      case REID_NEW_PLAYER: {
         if (!rec->has_new_player_event_msg()) break;
         const auto& msg = rec->new_player_event_msg();
         syncFrame(t);
         declareObject(msg.id(), msg.has_state() ? &msg.state() : nullptr);
         break;
      }

      case REID_PLAYER_DATA: {
         if (!rec->has_player_data_msg()) break;
         const auto& msg = rec->player_data_msg();
         syncFrame(t);
         emitState(msg.id(), msg.state());
         break;
      }

      case REID_PLAYER_REMOVED: {
         if (!rec->has_player_removed_event_msg()) break;
         const auto& msg = rec->player_removed_event_msg();
         syncFrame(t);
         server.removeObject(msg.id().id());
         declared.erase(msg.id().id());
         break;
      }

      case REID_WEAPON_RELEASED: {
         if (!rec->has_weapon_release_event_msg()) break;
         const auto& msg = rec->weapon_release_event_msg();
         syncFrame(t);
         declareObject(msg.wpn_id(), msg.has_wpn_state() ? &msg.wpn_state() : nullptr);
         if (msg.has_shooter_id()) {
            std::ostringstream text;
            text << "Released " << (msg.wpn_id().has_name() ? msg.wpn_id().name() : "store");
            server.logEvent(msg.shooter_id().id(), text.str());
         }
         break;
      }

      case REID_WEAPON_DETONATION: {
         if (!rec->has_weapon_detonation_event_msg()) break;
         const auto& msg = rec->weapon_detonation_event_msg();
         syncFrame(t);
         server.removeObject(msg.wpn_id().id());
         declared.erase(msg.wpn_id().id());
         break;
      }

      //---------------------------------------------------------------
      // REID_NEW_TRACK (81) NUNCA CHEGA: o DataRecorder concreto desta
      // versao do MIXR nao tem handler para esse token e o degrada para
      // REID_UNHANDLED_ID_TOKEN (2), apesar do TrackManager nativo sinalizar
      // a pista nova. REID_TRACK_DATA (83, periodico) e REID_TRACK_REMOVED
      // (82) SAO tratados normalmente -- entao o "primeiro contato" e
      // deduzido aqui mesmo, pela primeira amostra de cada track_id.
      //
      // O case de REID_NEW_TRACK fica declarado assim mesmo: se uma versao
      // futura do framework passar a emiti-lo, ja funciona.
      //---------------------------------------------------------------
      case REID_NEW_TRACK: {
         if (!rec->has_new_track_event_msg()) break;
         const auto& msg = rec->new_track_event_msg();
         syncFrame(t);
         if (announcedTracks.insert(msg.track_id()).second) {
            server.logEvent(msg.player_id().id(),
                            trackContactText(msg.track_id(),
                                             msg.has_trk_player_id() ? &msg.trk_player_id() : nullptr,
                                             msg.has_track_data() ? &msg.track_data() : nullptr));
         }
         break;
      }

      case REID_TRACK_DATA: {
         if (!rec->has_track_data_msg()) break;
         const auto& msg = rec->track_data_msg();
         if (!announcedTracks.insert(msg.track_id()).second) break;   // ja anunciada
         syncFrame(t);
         server.logEvent(msg.player_id().id(),
                         trackContactText(msg.track_id(),
                                          msg.has_trk_player_id() ? &msg.trk_player_id() : nullptr,
                                          msg.has_track_data() ? &msg.track_data() : nullptr));
         break;
      }

      case REID_TRACK_REMOVED: {
         if (!rec->has_track_removed_event_msg()) break;
         const auto& msg = rec->track_removed_event_msg();
         syncFrame(t);
         announcedTracks.erase(msg.track_id());
         std::ostringstream text;
         text << "Radar track " << msg.track_id() << " lost";
         server.logEvent(msg.player_id().id(), text.str());
         break;
      }

      case REID_PLAYER_KILLED: {
         if (!rec->has_player_killed_event_msg()) break;
         const auto& msg = rec->player_killed_event_msg();
         syncFrame(t);
         server.logEvent(msg.id().id(), "Killed");
         break;
      }

      case REID_MARKER: {
         if (!rec->has_marker_msg()) break;
         const auto& msg = rec->marker_msg();
         syncFrame(t);
         std::ostringstream text;
         text << "Marker " << (msg.has_id() ? msg.id() : 0);
         server.logEvent(msg.has_source_id() ? msg.source_id() : 0, text.str());
         break;
      }

      default:
         break;
   }
}

bool TacviewOutput::shutdownNotification()
{
   server.stop();
   return BaseClass::shutdownNotification();
}

//------------------------------------------------------------------------------
// slot table helper methods
//------------------------------------------------------------------------------

bool TacviewOutput::setSlotHost(const base::String* const x)
{
   if (x == nullptr) return false;
   host = x->getString();
   return true;
}

bool TacviewOutput::setSlotPort(const base::Integer* const x)
{
   if (x == nullptr) return false;
   port = x->getInt();
   return true;
}

bool TacviewOutput::setSlotFileName(const base::String* const x)
{
   if (x == nullptr) return false;
   fileName = x->getString();
   return true;
}

bool TacviewOutput::setSlotCallsign(const base::String* const x)
{
   if (x == nullptr) return false;
   callsign = x->getString();
   return true;
}

// typeMap: { F4N: "Air+FixedWing"  Chaff: "Misc+Decoy+Chaff" }
//
// A label do par e o 'type:' do Player; o valor e a tag ACMI. Isso mantem o
// mapeamento no EDL em vez de uma tabela hardcoded por poc.
bool TacviewOutput::setSlotTypeMap(const base::PairStream* const x)
{
   if (x == nullptr) return false;

   typeMap.clear();
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      const auto value = dynamic_cast<const base::String*>(pair->object());
      if (value != nullptr) {
         typeMap[pair->slot()->getString()] = value->getString();
      }
      item = item->getNext();
   }
   return true;
}

// modelMap: { falcon1: "F-4E"  falcon2: "F-4E" } -- vira "Name=" no ACMI
bool TacviewOutput::setSlotModelMap(const base::PairStream* const x)
{
   if (x == nullptr) return false;

   modelMap.clear();
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      const auto value = dynamic_cast<const base::String*>(pair->object());
      if (value != nullptr) {
         modelMap[pair->slot()->getString()] = value->getString();
      }
      item = item->getNext();
   }
   return true;
}

// colorMap: { Blue: "Blue"  Red: "Red" }
bool TacviewOutput::setSlotColorMap(const base::PairStream* const x)
{
   if (x == nullptr) return false;

   colorMap.clear();
   const base::List::Item* item{x->getFirstItem()};
   while (item != nullptr) {
      const auto pair = static_cast<const base::Pair*>(item->getValue());
      const auto value = dynamic_cast<const base::String*>(pair->object());
      if (value != nullptr) {
         colorMap[pair->slot()->getString()] = value->getString();
      }
      item = item->getNext();
   }
   return true;
}

}
}
