#ifndef __xrlbridge_RLBridge_H__
#define __xrlbridge_RLBridge_H__

#include "xrlbridge/ObservationFields.hpp"
#include "xrlbridge/Schema.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace xrlbridge {

//------------------------------------------------------------------------------
// A ponte de comando/observacao entre um core de RL (src/rl/bindings, um modulo
// de extensao Python) e o comportamento UBF que decide por fora do processo
// MIXR (models/players/air/A-4/include/ubf/RLBridgeBehavior.hpp).
//
// Uma unica questao: o comando que o core quer aplicar, e a observacao que o
// modelo capturou no ultimo ciclo de decisao -- os dois lados de uma troca
// sincrona (ver src/rl/README.md para o protocolo completo: NativeSimulation::
// step() escreve o comando, chama station->tcFrame()/updateData(), depois LE
// a observacao que RLBridgeBehavior::genAction() cacheou durante esse mesmo
// frame).
//
// Sem chave por player ID, de proposito -- v1 e um unico agente RL por
// processo. domain::WorldView::ownerName ja resolve o nome do player
// hospedeiro, e Observation.ownerName ja o propaga; o que falta para
// multi-agente de verdade e a armazenagem -- setObservation()/
// getObservation()/setPendingCommand()/getPendingCommand() continuam um
// unico par global (mesmo espirito de xboard::Board), nao um mapa por
// playerId.
//
// MESMO MOTIVO ESTRUTURAL de libs/xboard/Board.hpp para ser a UNICA
// shared_library() desta dupla (as outras libs de libs/ sao estaticas):
// quem ESCREVE o comando e LE a observacao e o core (executavel); quem LE o
// comando e ESCREVE a observacao e o modelo (um .so carregado com dlopen).
// Uma lib estatica daria a cada lado a SUA PROPRIA copia do mapa -- o core
// nunca veria o comando chegar no modelo, e vice-versa.
//
// Os campos de Observation espelham domain::WorldView (models/players/air/A-4/include/
// domain/WorldView.hpp) CAMPO A CAMPO, mas deliberadamente NAO reusam o tipo:
// esta lib nao pode incluir headers do modelo (ver
// tests/guard/check_core_opaco.sh -- o core nao pode conhecer o fonte do
// modelo), entao ela define a sua PROPRIA copia da forma. O modelo converte
// domain::WorldView -> Observation campo a campo em
// RLBridgeBehavior::genAction(); nao ha conversao nenhuma do lado do core,
// que so ve Observation.
//------------------------------------------------------------------------------
// CONCORRENCIA: mesmo padrao de Board.hpp -- um mutex so, mapa minusculo (um
// agente RL por processo hoje, ver o "Escopo" do plano de implementacao).
//------------------------------------------------------------------------------

struct Command
{
   // Sem este flag, RLBridgeBehavior::genAction() nao tem como distinguir "o
   // core ainda nao publicou nenhuma acao" de "o core publicou
   // heading=0/altitude=0/speed=0 de proposito" num Command
   // default-construido. Sem ele, o frame de priming que
   // NativeSimulation::reset() dispara mandaria a aeronave para o nivel do
   // mar, parada; em resets seguintes no mesmo processo, o comando aplicado
   // no priming seria o ultimo do episodio anterior, vazando entre
   // episodios.
   bool valid{};

   double headingDeg{};
   double altitudeM{};
   double speedKts{};
};

struct Observation
{
   bool valid{};

   // ownerName e preenchido em toObservation() a partir de snap.ownerName
   // (domain::WorldView::ownerName). Ainda nao habilita multi-agente por si
   // so -- a armazenagem abaixo continua um unico par global -- mas resolve
   // a lacuna de "nao ha como saber de quem e a observacao", primeiro passo
   // para quem trocar o par global por um mapa.
   std::string ownerName;

   double northM{};
   double eastM{};
   double altitudeM{};
   double headingDeg{};
   double speedKts{};
   double rollDeg{};
   double pitchDeg{};
   double fuelFraction{1.0};
   double mach{};
   double gLoad{1.0};
   double alphaDeg{};

   bool terrainValid{};
   double terrainElevM{};
   double altitudeAglM{};

   bool hasContact{};
   std::string contactName;
   double contactRangeM{};
   double contactRelBearingDeg{};
   double contactDeltaAltM{};
   double contactNorthM{};
   double contactEastM{};
   double contactAltitudeM{};

   bool hasAlert{};
   std::string alertSender;
   std::string alertContactName;
   double alertNorthM{};
   double alertEastM{};
   double alertAltitudeM{};
   double alertRangeM{};

   bool weaponReady{};

   // RWR + navegacao nativa espelham domain::WorldView campo a campo, mesma
   // regra do resto da struct (ver ObservationFields.hpp para a ordem/motivo
   // de terem sido acrescentados no fim da macro).
   double rwrThreatRangeM{};
   double rwrThreatRelBearingDeg{};
   double rwrThreatDeltaAltM{};
   bool hasRwrThreat{};
   double navTrueBrgDeg{};
   double navCmdAltM{};
   double navCmdSpeedKts{};
   bool hasNavSteering{};
   bool hasNavCmdAlt{};
   bool hasNavCmdSpeed{};
};

//--- escrita: SO o core (src/rl/bindings/NativeSimulation.cpp) chama -------------
void setPendingCommand(const Command& cmd);

//--- leitura: SO o modelo (RLBridgeBehavior::genAction()) chama --------------
Command getPendingCommand();

//--- escrita: SO o modelo (RLBridgeBehavior::genAction()) chama --------------
void setObservation(const Observation& obs);

//--- leitura: SO o core chama -------------------------------------------------
Observation getObservation();

//------------------------------------------------------------------------------
// O CONTRATO DE DADOS com o treino -- ver xrlbridge/ObservationFields.hpp.
//
// Estas tres funcoes existem para que a ordem dos campos deixe de ser mantida
// a mao em cinco lugares. Sao usadas pelo lado do MODELO (que empacota o
// WorldView para o .onnx) e expostas ao Python pelos bindings, que constroi o
// observation_space a partir delas em vez de repetir a lista.
//------------------------------------------------------------------------------

// Os nomes dos 28 campos numericos, na ordem canonica.
std::vector<std::string> observationFieldNames();

// Quais desses nomes sao booleanos (os 5 do fim). O lado Python precisa para
// montar Discrete(2) em vez de Box -- e para nao ter de repetir a lista.
std::vector<std::string> observationBoolFields();

// Observation -> os 28 floats, na ordem canonica. 'out' tem de ter pelo menos
// XRLBRIDGE_OBSERVATION_SIZE posicoes.
void packObservation(const Observation& obs, float* out);

// Acao NORMALIZADA em [-1,1] (o que um .onnx exportado do SB3 emite) -> as
// unidades fisicas que o Autopilot espera. Fora de [-1,1] e recortado: a
// politica nao pode comandar 40 mil pes so porque a rede saiu de escala.
Command unscaleCommand(const float* normalized3);

//------------------------------------------------------------------------------
// classicSchema28() -- os 28 nomes historicos, NESTA ordem, como um Schema
// nomeado ("classic28"). Usado como default de todo consumidor que ganhou
// selecao de schema nesta passada (os tres nos de arvore de
// models/players/air/A-4/src/bt/nodes/, e o lado Python via
// mixr_gym._native.classic_schema_28()) -- preserva byte a byte o
// comportamento de antes desta macro crescer para 38 campos.
//
// HARDCODED de proposito, e NAO derivado de "os primeiros 28 nomes da macro
// atual": mesmo que XRLBRIDGE_OBSERVATION_FIELDS seja reordenada ou cresca de
// novo no futuro, este preset continua exatamente estes 28 nomes, nesta
// ordem -- e' o que protege qualquer .onnx ja treinado contra ele.
//------------------------------------------------------------------------------
Schema classicSchema28();

} // namespace xrlbridge
} // namespace mixr

#endif
