#ifndef __xrlbridge_ObservationFields_H__
#define __xrlbridge_ObservationFields_H__

//------------------------------------------------------------------------------
// A ORDEM CANONICA DOS CAMPOS -- a fonte unica de verdade do contrato entre o
// TREINO (Python, src/rl) e a INFERENCIA (C++, um .onnx rodando no frame).
//
// O PROBLEMA QUE ISTO RESOLVE. A forma da observacao era mantida a mao em
// cinco lugares: domain::WorldView, xrlbridge::Observation, a conversao campo
// a campo de RLBridgeBehavior, o toDict() dos bindings e as listas
// _FLOAT_FIELDS/_BOOL_FIELDS de env.py. Enquanto a politica era um processo
// Python do outro lado de uma caixa de correio, divergir dava KeyError -- alto
// e na hora. Com um .onnx, divergir NAO da erro nenhum: o modelo recebe 28
// floats na ordem errada e voa errado, em silencio. Por isso a ordem virou
// dado, num lugar so.
//
// COMO USAR. Este arquivo nao define tipo nenhum, so a lista. Quem inclui
// define as duas macros e as expande contra a SUA struct:
//
//    #define XRLBRIDGE_F(nome) meuVetor[i++] = static_cast<float>(obs.nome);
//    #define XRLBRIDGE_B(nome) meuVetor[i++] = obs.nome ? 1.0F : 0.0F;
//    XRLBRIDGE_OBSERVATION_FIELDS
//    #undef XRLBRIDGE_F
//    #undef XRLBRIDGE_B
//
// Funciona contra domain::WorldView E contra xrlbridge::Observation sem
// nenhuma adaptacao, porque os nomes de campo das duas sao IDENTICOS -- o que
// ate agora era coincidencia mantida a mao, e daqui em diante e verificado
// (models/player/A4 compila a mesma macro contra WorldView; esta lib, contra
// Observation -- um nome que divergir nao compila).
//
// OS TRES CAMPOS DE TEXTO FICAM DE FORA de proposito (contactName,
// alertSender, alertContactName): nao sao numeros, nao entram num tensor, e
// env.py ja os excluia do observation_space pelo mesmo motivo. Continuam
// disponiveis para log e depuracao.
//
// A ORDEM E A DE env.py (23 floats, depois 5 bools) -- nao a ordem de
// declaracao de WorldView, que intercala os dois. Mudar a ordem aqui INVALIDA
// todo .onnx ja treinado: e uma quebra de contrato, nao um refactor.
//------------------------------------------------------------------------------

#define XRLBRIDGE_OBSERVATION_FIELDS \
   XRLBRIDGE_F(northM)               \
   XRLBRIDGE_F(eastM)                \
   XRLBRIDGE_F(altitudeM)            \
   XRLBRIDGE_F(headingDeg)           \
   XRLBRIDGE_F(speedKts)             \
   XRLBRIDGE_F(rollDeg)              \
   XRLBRIDGE_F(pitchDeg)             \
   XRLBRIDGE_F(fuelFraction)         \
   XRLBRIDGE_F(mach)                 \
   XRLBRIDGE_F(gLoad)                \
   XRLBRIDGE_F(alphaDeg)             \
   XRLBRIDGE_F(terrainElevM)         \
   XRLBRIDGE_F(altitudeAglM)         \
   XRLBRIDGE_F(contactRangeM)        \
   XRLBRIDGE_F(contactRelBearingDeg) \
   XRLBRIDGE_F(contactDeltaAltM)     \
   XRLBRIDGE_F(contactNorthM)        \
   XRLBRIDGE_F(contactEastM)         \
   XRLBRIDGE_F(contactAltitudeM)     \
   XRLBRIDGE_F(alertNorthM)          \
   XRLBRIDGE_F(alertEastM)           \
   XRLBRIDGE_F(alertAltitudeM)       \
   XRLBRIDGE_F(alertRangeM)          \
   XRLBRIDGE_B(valid)                \
   XRLBRIDGE_B(terrainValid)         \
   XRLBRIDGE_B(hasContact)           \
   XRLBRIDGE_B(hasAlert)             \
   XRLBRIDGE_B(weaponReady)

// Quantos floats o vetor tem. Conferido em tempo de COMPILACAO contra a
// expansao da macro (ver a static_assert em RLBridge.cpp) -- um campo
// acrescentado sem mexer neste numero nao compila.
#define XRLBRIDGE_OBSERVATION_SIZE 28

//------------------------------------------------------------------------------
// A ACAO -- os tres campos de domain::FlightCommand, com a faixa fisica de
// cada um.
//
// A faixa importa porque uma politica treinada com Box(low, high) e exportada
// para .onnx costuma emitir acao NORMALIZADA (o Tanh final do SB3 da [-1,1]),
// enquanto o Autopilot quer graus, metros e nos. A desnormalizacao tem de usar
// EXATAMENTE os mesmos limites dos dois lados -- sao estes, e sao os defaults
// que MixrFlightEnv.__init__ ja usava.
//------------------------------------------------------------------------------
#define XRLBRIDGE_ACTION_FIELDS                    \
   XRLBRIDGE_A(headingDeg,   0.0,   360.0)         \
   XRLBRIDGE_A(altitudeM,    0.0,  8000.0)         \
   XRLBRIDGE_A(speedKts,     0.0,   400.0)

#define XRLBRIDGE_ACTION_SIZE 3

#endif
