#ifndef __xrlbridge_RLBridge_H__
#define __xrlbridge_RLBridge_H__

#include "xrlbridge/ObservationFields.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace xrlbridge {

//------------------------------------------------------------------------------
// A ponte de comando/observacao entre um host de RL (src/rl/bindings, um modulo
// de extensao Python) e o comportamento UBF que decide por fora do processo
// MIXR (models/flight/include/ubf/RLBridgeBehavior.hpp).
//
// Uma unica questao: o comando que o host quer aplicar, e a observacao que o
// modelo capturou no ultimo ciclo de decisao -- os dois lados de uma troca
// sincrona (ver src/rl/README.md para o protocolo completo: NativeSimulation::
// step() escreve o comando, chama station->tcFrame()/updateData(), depois LE
// a observacao que RLBridgeBehavior::genAction() cacheou durante esse mesmo
// frame).
//
// SEM CHAVE POR PLAYER ID, de proposito -- v1 e um UNICO agente RL por
// processo (ver o "Escopo" do plano de implementacao e o mesmo limite ja
// documentado em RLBridgeBehavior.hpp). genAction() nao tem como descobrir o
// ID do player que o hospeda sem subir a arvore de componentes por
// container() -- caminho ja documentado como fragil neste framework para
// objetos aninhados em slot (ver a armadilha de TacviewOutput::resolveInfo()
// no CLAUDE.md raiz). Generalizar para varios agentes RL trocaria estas duas
// funcoes por um mapa por playerId, no mesmo espirito de xboard::Board --
// nao feito aqui porque nao ha cenario que precise disso ainda.
//
// MESMO MOTIVO ESTRUTURAL de shared/xboard/Board.hpp para ser a UNICA
// shared_library() desta dupla (as outras libs de shared/ sao estaticas):
// quem ESCREVE o comando e LE a observacao e o host (executavel); quem LE o
// comando e ESCREVE a observacao e o modelo (um .so carregado com dlopen).
// Uma lib estatica daria a cada lado a SUA PROPRIA copia do mapa -- o host
// nunca veria o comando chegar no modelo, e vice-versa.
//
// Os campos de Observation espelham domain::WorldView (models/flight/include/
// domain/WorldView.hpp) CAMPO A CAMPO, mas deliberadamente NAO reusam o tipo:
// esta lib nao pode incluir headers do modelo (ver
// tests/guard/check_host_opaco.sh -- o host nao pode conhecer o fonte do
// modelo), entao ela define a sua PROPRIA copia da forma. O modelo converte
// domain::WorldView -> Observation campo a campo em
// RLBridgeBehavior::genAction(); nao ha conversao nenhuma do lado do host,
// que so ve Observation.
//------------------------------------------------------------------------------
// CONCORRENCIA: mesmo padrao de Board.hpp -- um mutex so, mapa minusculo (um
// agente RL por processo hoje, ver o "Escopo" do plano de implementacao).
//------------------------------------------------------------------------------

struct Command
{
   double headingDeg{};
   double altitudeM{};
   double speedKts{};
};

struct Observation
{
   bool valid{};

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
};

//--- escrita: SO o host (src/rl/bindings/NativeSimulation.cpp) chama -------------
void setPendingCommand(const Command& cmd);

//--- leitura: SO o modelo (RLBridgeBehavior::genAction()) chama --------------
Command getPendingCommand();

//--- escrita: SO o modelo (RLBridgeBehavior::genAction()) chama --------------
void setObservation(const Observation& obs);

//--- leitura: SO o host chama -------------------------------------------------
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

} // namespace xrlbridge
} // namespace mixr

#endif
