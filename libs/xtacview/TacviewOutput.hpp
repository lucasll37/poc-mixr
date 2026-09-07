#ifndef __xtacview_TacviewOutput_H__
#define __xtacview_TacviewOutput_H__

#include "mixr/recorder/OutputHandler.hpp"
#include "xtacview/RealtimeTelemetryServer.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace mixr {
namespace base { class Integer; class Number; class PairStream; class String; }
namespace recorder { class DataRecordHandle; namespace pb { class PlayerId; class PlayerState; class TrackData; } }
namespace simulation { class Simulation; }
namespace models { class Player; }

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

   //---------------------------------------------------------------------
   // Identidade REAL de cada player (tipo/lado/major type), empurrada pelo
   // HOST -- mesma porta de servico de updateRadarScan(), e pelo mesmo
   // motivo: o pipeline do recorder nao entrega o dado, e quem o tem em
   // maos e quem roda o laco de background.
   //
   // POR QUE ISTO EXISTE (medido, nao suposto):
   //   1. REID_PLAYER_DATA (43) -- o UNICO token por onde os objetos
   //      chegam aqui, ja que 'enabledList: [ 43 42 ]' deixa de fora
   //      REID_NEW_PLAYER (41) e REID_WEAPON_RELEASED (61, que aborta o
   //      DataRecorder nativo) -- traz um PlayerId PARCIAL: so 'id' e
   //      'name'. Sem 'ac_type', sem 'major_type', sem 'side'.
   //   2. resolveInfo() tentaria buscar o Player real subindo ate a
   //      Station, mas o encadeamento de containers NAO chega la (o
   //      DataRecorder nao chama container() no objeto do slot
   //      'outputHandler') -- ver o comentario de resolveInfo() no .cpp.
   //   3. Sobra casar por NOME nos mapas do EDL. Isso cobre os players
   //      declarados no cenario, e NAO cobre os criados em runtime: um
   //      missil liberado ganha nome automatico "W%05d"
   //      (AbstractWeapon::release() -> Simulation::getNewReleasedWeaponID(),
   //      faixa 10001..65535), impossivel de escrever num mapa. Resultado
   //      antes disto: o missil ia pro Tacview como "Misc"/"Grey", sem
   //      modelo -- um quadradinho cinza em vez de um missil.
   //
   // Com a identidade empurrada, cada objeto resolve por
   // typeMap/colorMap/modelMap[type:] -> [nome] -> default por
   // majorType/side -- um SUPERCONJUNTO dos dois caminhos anteriores,
   // entao nada que ja funcionava muda.
   //
   // ORDEM IMPORTA: chame ANTES de station->updateData(dt) no laco. E o
   // updateData() que drena a fila do gravador e DECLARA o objeto no
   // stream (Name/Type/Color so vao na primeira aparicao de cada id --
   // ver RealtimeTelemetryServer::updateObject()). Um player materializado
   // em runtime entra na lista em updatePlayerList(), dentro de
   // updateData(), e so emite seu primeiro REID_PLAYER_DATA no tcFrame
   // SEGUINTE -- entao publicar no topo do laco da uma iteracao inteira de
   // folga.
   //
   // THREAD: o mesmo laco de background que chama updateData() e
   // updateRadarScan(). Nenhuma das tres pocs cria a thread de background
   // nativa (Station::createBackgroundProcess()), entao publicacao e
   // consumo acontecem na MESMA thread -- sem lock, como updateRadarScan().
   void publishIdentities(const simulation::Simulation* const sim);

   //---------------------------------------------------------------------
   // Estado observavel da exportacao -- para a aba "Tempo Nao-Critico" do
   // ./app. Repassa o que RealtimeTelemetryServer ja contabiliza (ver a
   // secao de introspeccao la) mais o que so este nivel conhece: se a
   // inicializacao falhou (porta ocupada, diretorio de gravacao
   // inexistente) e quantos objetos ja ganharam um T= no stream.
   //---------------------------------------------------------------------
   const RealtimeTelemetryServer& telemetry() const { return server; }
   bool isInitialized() const                       { return initialized; }
   bool didInitFail() const                         { return initFailed; }
   std::size_t declaredObjectCount() const          { return declared.size(); }
   std::size_t identifiedObjectCount() const        { return resolvedCache.size(); }
   double currentStreamTime() const                 { return currentFrameTime; }

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

   std::string trackContactText(const std::string& trackId,
                                const recorder::pb::PlayerId* const tgt,
                                const recorder::pb::TrackData* const) const;

   // Propriedades resolvidas uma vez por objeto, consultando o WorldModel.
   struct ResolvedInfo { std::string type; std::string color; std::string model; bool valid{}; };

   // A regra de precedencia, num lugar so: mapa por 'type:' do EDL, depois
   // por NOME do player, depois o default por majorType/side. Usada tanto
   // por publishIdentities() (que tem os quatro campos de verdade) quanto
   // pelo fallback de resolveInfo() (que tem so o que o protobuf deu).
   // 'player' e opcional: quando ha um Player vivo (publishIdentities()) o
   // tipo default sai da CLASSE dele -- e o unico jeito de distinguir chaff
   // de missil, ja que os dois sao majorType WEAPON.
   ResolvedInfo resolveFrom(const std::string& name, const std::string& type,
                            const unsigned int majorType, const unsigned int side,
                            const models::Player* const player = nullptr) const;

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
