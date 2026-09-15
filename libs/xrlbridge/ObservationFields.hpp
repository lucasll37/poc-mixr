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
// (models/players/A-4 compila a mesma macro contra WorldView; esta lib, contra
// Observation -- um nome que divergir nao compila).
//
// OS CAMPOS DE TEXTO FICAM DE FORA de proposito (contactName, alertSender,
// alertContactName, rwrThreatName): nao sao numeros, nao entram num tensor, e
// env.py ja os excluia do observation_space pelo mesmo motivo. Continuam
// disponiveis para log e depuracao.
//
// A ORDEM DOS 28 PRIMEIROS E A DE env.py (23 floats, depois 5 bools) -- nao a
// ordem de declaracao de WorldView, que intercala os dois. Mudar a ORDEM
// DESSES 28 invalida todo .onnx ja treinado contra ela: e uma quebra de
// contrato, nao um refactor -- por isso xrlbridge::classicSchema28() (ver
// RLBridge.hpp) fixa esses 28 nomes/ordem numa lista PROPRIA, independente
// desta macro, para nunca ser afetada se esta lista for reordenada de novo.
//
// Os 10 campos finais (RWR + navegacao) ja existiam em domain::WorldView,
// mas a macro nunca tinha sido atualizada para inclui-los -- ficavam
// invisiveis para qualquer politica de RL/ONNX mesmo ja existindo no sensor
// da aeronave. Foram acrescentados no fim, na ordem de declaracao de
// WorldView, nunca inseridos no meio, para nao perturbar a posicao dos 28
// campos originais -- seguro porque nenhum consumidor depende de posicao
// absoluta alem deles (protegidos a parte por classicSchema28()).
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
   XRLBRIDGE_B(weaponReady)          \
   XRLBRIDGE_F(rwrThreatRangeM)      \
   XRLBRIDGE_F(rwrThreatRelBearingDeg) \
   XRLBRIDGE_F(rwrThreatDeltaAltM)   \
   XRLBRIDGE_B(hasRwrThreat)         \
   XRLBRIDGE_F(navTrueBrgDeg)        \
   XRLBRIDGE_F(navCmdAltM)           \
   XRLBRIDGE_F(navCmdSpeedKts)       \
   XRLBRIDGE_B(hasNavSteering)       \
   XRLBRIDGE_B(hasNavCmdAlt)         \
   XRLBRIDGE_B(hasNavCmdSpeed)

// Quantos floats o vetor tem. Conferido em tempo de COMPILACAO contra a
// expansao da macro (ver a static_assert em RLBridge.cpp) -- um campo
// acrescentado sem mexer neste numero nao compila. 29 floats + 9 bools.
#define XRLBRIDGE_OBSERVATION_SIZE 38

//------------------------------------------------------------------------------
// A ACAO -- os tres campos de domain::FlightCommand, com a faixa fisica de
// cada um.
//
// A normalizacao para [-1,1] nao e automatica: o SB3 padrao (PPO/MlpPolicy,
// sem squash_output+use_sde) nao aplica Tanh no forward da policy. Quem
// normaliza e o exportador (export_onnx.py::exportar_sb3()), escalando a
// saida fisica crua da policy para [-1,1] com os mesmos limites usados em
// unscaleCommand() -- os dois lados tem de usar exatamente os mesmos
// limites, os defaults de MixrFlightEnv.__init__ (o exportador le os
// limites reais de modelo.action_space, nao um valor fixo).
//------------------------------------------------------------------------------
#define XRLBRIDGE_ACTION_FIELDS                    \
   XRLBRIDGE_A(headingDeg,   0.0,   360.0)         \
   XRLBRIDGE_A(altitudeM,    0.0,  8000.0)         \
   XRLBRIDGE_A(speedKts,     0.0,   400.0)

#define XRLBRIDGE_ACTION_SIZE 3

#endif
