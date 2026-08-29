#ifndef __xtacview_TacviewOutput_H__
#define __xtacview_TacviewOutput_H__

#include "mixr/recorder/OutputHandler.hpp"
#include "xtacview/RealtimeTelemetryServer.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace mixr {
namespace base { class Integer; class Number; class PairStream; class String; }
namespace recorder { class DataRecordHandle; namespace pb { class PlayerId; class PlayerState; class TrackData; } }

namespace xtacview {

//------------------------------------------------------------------------------
// Class: TacviewOutput
// Description: Exporta a simulacao para o Tacview como um elo da cadeia
//              nativa de OutputHandlers do mixr::recorder.
//
// Factory name: TacviewOutput
//
// Slots:
//    host       <String>      ! IP de escuta (default: "0.0.0.0" -- ver nota WSL2)
//    port       <Integer>     ! Porta Real-Time Telemetry (default: 1234)
//    fileName   <String>      ! Arquivo .acmi a gravar (default: nenhum)
//    callsign   <String>      ! Nome do host no handshake (default: "poc-mixr")
//    typeMap    <PairStream>  ! type: do Player -> tag ACMI "Type=" (ver abaixo)
//    colorMap   <PairStream>  ! side do Player ("blue"/"red"/...) -> cor ACMI
//    modelMap   <PairStream>  ! nome/type: do Player -> MODELO ACMI ("F-4E", "F-16C")
//
// Em vez de o main.cpp varrer a lista de players a cada tick e montar as
// linhas ACMI na mao, o framework EMPURRA os dados para ca: cada registro
// REID_* vira uma linha do stream. Isso e o que da, de graca, os eventos que
// as pocs montavam manualmente:
//
//    REID_PLAYER_DATA        -> linha de posicao/atitude do objeto
//    REID_NEW_PLAYER         -> declaracao (Name/Type/Color) do objeto
//    REID_PLAYER_REMOVED     -> "-<id>" (some do replay)
//    REID_WEAPON_RELEASED    -> declaracao do chaff/flare/missil lancado
//    REID_WEAPON_DETONATION  -> "-<id>" + Event=Message
//    REID_NEW_TRACK          -> Event=Message com a pista do radar
//    REID_PLAYER_KILLED      -> Event=Message
//    REID_MARKER             -> Event=Message (eventos proprios da aplicacao)
//
// ATENCAO -- 'dataLogTime' e slot do Player e NASCE ZERO. A emissao de
// REID_PLAYER_DATA e guardada por 'if (dataLogTime > 0.0)' em
// Player::updateTC(), entao um player sem esse slot declarado NUNCA aparece
// no Tacview, e o sintoma parece bug deste handler.
//
// A taxa do stream passa a ser, portanto, per-player e declarativa
// (dataLogTime: ( Seconds 0.1 ) = 10 Hz), em vez de um ritmo unico fixado no
// laco do main.cpp.
//------------------------------------------------------------------------------
class TacviewOutput : public recorder::OutputHandler
{
   DECLARE_SUBCLASS(TacviewOutput, recorder::OutputHandler)

public:
   TacviewOutput();

   // Feixe/varredura do radar de um player -- chamado FORA da cadeia do
   // recorder, direto do laco de tempo real (ver app/RealTimeRun.cpp).
   //
   // O pipeline REID/DataRecorder nao tem schema para dado de sensor/gimbal
   // (DataRecord.proto so cobre PlayerState/TrackData/EmissionData -- nenhum
   // deles e "para onde a antena esta apontando agora"), e a cadeia de
   // containers que resolveria o Player vivo a partir DAQUI (resolveInfo(),
   // acima) esta documentada como quebrada. Quem ja tem o AirVehicle nativo
   // -- a Fleet do main.cpp, via xnative::radarScanOf() -- empurra o dado
   // direto. So emite se o objeto ja recebeu um T= (ver 'declared' abaixo);
   // sem isso o Tacview receberia RadarAzimuth= para um id desconhecido.
   void updateRadarScan(const std::uint32_t playerId, const double simTimeSec,
                        const double azimuthDeg, const double elevationDeg, const double rangeM,
                        const double hBeamwidthDeg, const double vBeamwidthDeg);

protected:
   void processRecordImp(const recorder::DataRecordHandle* const) override;
   bool shutdownNotification() override;

private:
   // Garante socket/arquivo abertos; segue o padrao de NetOutput (tenta uma
   // vez, e nao fica repetindo se falhar).
   void initIfNeeded();

   // Emite "#<t>" quando o tempo do registro avanca.
   void syncFrame(const double simTimeSec);

   // Declara o objeto (Name/Type/Color) e devolve o id ACMI.
   std::uint32_t declareObject(const recorder::pb::PlayerId&, const recorder::pb::PlayerState* const);

   // Emite a linha de posicao/atitude de um objeto ja declarado.
   void emitState(const recorder::pb::PlayerId&, const recorder::pb::PlayerState&);

   std::string acmiTypeFor(const recorder::pb::PlayerId&) const;
   std::string acmiColorFor(const recorder::pb::PlayerId&) const;

   std::string acmiModelFor(const recorder::pb::PlayerId&) const;
   std::string trackContactText(const std::string& trackId,
                                const recorder::pb::PlayerId* const tgt,
                                const recorder::pb::TrackData* const) const;

   // Propriedades resolvidas uma vez por objeto, consultando o WorldModel.
   struct ResolvedInfo { std::string type; std::string color; std::string model; bool valid{}; };

   // REID_PLAYER_DATA nao traz 'type'/'side' (ver nota no .cpp), e objetos
   // criados em runtime (chaff/flare/misseis) recebem nomes automaticos
   // ("W10001"...) que nao dao para mapear no EDL. Entao a fonte de
   // verdade e o proprio Player: achado pelo id na lista do WorldModel,
   // exatamente como o Datalink nativo faz para alcancar outro player.
   const ResolvedInfo& resolveInfo(const recorder::pb::PlayerId&);

   std::map<std::uint32_t, ResolvedInfo> resolvedCache;

   RealtimeTelemetryServer server;

   std::string host{"0.0.0.0"};
   int port{1234};
   std::string fileName;
   std::string callsign{"poc-mixr"};

   std::map<std::string, std::string> typeMap;    // type: do Player -> "Type=" ACMI
   std::map<std::string, std::string> colorMap;   // side -> "Color=" ACMI
   std::map<std::string, std::string> modelMap;   // nome/type: do Player -> "Name=" (modelo)

   bool initialized{};
   bool initFailed{};
   bool frameOpen{};
   double currentFrameTime{-1.0};

   // Ids ja declarados no stream (para nao repetir Name/Type/Color).
   std::map<std::uint32_t, bool> declared;

   // Pistas ja anunciadas -- ver nota sobre REID_NEW_TRACK no .cpp.
   std::set<std::string> announcedTracks;

private:
   // slot table helper methods
   bool setSlotHost(const base::String* const);
   bool setSlotPort(const base::Integer* const);
   bool setSlotFileName(const base::String* const);
   bool setSlotCallsign(const base::String* const);
   bool setSlotTypeMap(const base::PairStream* const);
   bool setSlotColorMap(const base::PairStream* const);
   bool setSlotModelMap(const base::PairStream* const);
};

}
}

#endif
