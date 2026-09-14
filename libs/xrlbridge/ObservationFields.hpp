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
// OS 10 CAMPOS NO FIM (RWR + navegacao) foram ACRESCENTADOS numa passada
// posterior -- domain::WorldView (models/players/A-4) ja os tinha havia tempo
// (RWR e navegacao nativa), mas ninguem tinha atualizado esta macro; ficavam
// invisiveis para qualquer politica de RL/ONNX, mesmo ja existindo no sensor
// da aeronave (achado de auditoria). Acrescentados NO FIM, na ordem de
// declaracao de WorldView (RWR primeiro, depois navegacao) -- nunca inseridos
// no meio -- para nao perturbar posicao nenhuma dos 28 originais. Isso e
// seguro porque NENHUM consumidor real depende da posicao ABSOLUTA de um
// campo dentro desta lista: os quatro pontos que hoje leem 'a lista inteira,
// nesta ordem' (RLBridge.cpp, os tres nos de arvore de models/players/A-4)
// continuam expandindo a macro do mesmo jeito de sempre; e o schema nomeado
// (xrlbridge::Schema/bind(), ver FieldRegistry.hpp/Schema.hpp) resolve por
// NOME, nunca por posicao.
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
// A faixa importa porque um .onnx exportado precisa emitir acao NORMALIZADA
// em [-1,1] (o que `unscaleCommand()` abaixo espera) -- mas isso NAO e
// automatico: o SB3 padrao (PPO/MlpPolicy, sem squash_output+use_sde) NAO
// aplica Tanh nenhum no forward da policy (achado por auditoria -- o
// comentario aqui antes afirmava o contrario). Quem faz a normalizacao e o
// EXPORTADOR (`src/poc/rl-training/tools/export_onnx.py::exportar_sb3()`),
// escalando a saida fisica crua da policy para [-1,1] com estes MESMOS
// limites antes de gravar o .onnx. A desnormalizacao (abaixo) e a
// exportacao tem de usar EXATAMENTE os mesmos limites dos dois lados -- sao
// estes, e sao os defaults que MixrFlightEnv.__init__ ja usava (o script de
// exportacao le os limites REAIS de `modelo.action_space`, nao um valor
// fixo -- o construtor aceita faixas customizadas).
//------------------------------------------------------------------------------
#define XRLBRIDGE_ACTION_FIELDS                    \
   XRLBRIDGE_A(headingDeg,   0.0,   360.0)         \
   XRLBRIDGE_A(altitudeM,    0.0,  8000.0)         \
   XRLBRIDGE_A(speedKts,     0.0,   400.0)

#define XRLBRIDGE_ACTION_SIZE 3

#endif
