#ifndef __events_EventTokens_H__
#define __events_EventTokens_H__

#include "mixr/base/Component.hpp"

namespace mixr {
namespace events {

//------------------------------------------------------------------------------
// Registro central de tokens de evento CUSTOMIZADOS deste repositorio -- ver
// events/README.md para a convencao completa.
//
// LEDGER UNICO, DE PROPOSITO -- nao um arquivo por evento (o payload de cada
// evento SIM vira uma pasta propria em events/payloads/<TOKEN>/, NOMEADA
// IGUAL ao token, mas o TOKEN em si fica aqui). O valor deste arquivo esta
// inteiro em ser o UNICO lugar onde dois eventos poderiam colidir de
// numero: espalhar a alocacao por varios arquivos destruiria essa garantia
// bem no momento em que ela mais importa (events/ crescendo). Cresce em
// LINHAS, nao em estrutura -- se um dia isso virar um problema de verdade
// (dezenas de tokens, categorias claras emergindo), a resposta e agrupar em
// blocos comentados aqui dentro, nao quebrar o arquivo: continua sendo UM
// grep, nunca uma busca em N lugares.
//
// mixr::base::Component::USER_EVENTS (2000) e o primeiro token que o proprio
// MIXR reserva para uso da aplicacao (eventTokens.hpp, faixa nativa vai ate
// 1999). Cada bloco abaixo e UM evento: uma constante aqui + uma classe de
// payload (base::Object-derivada, em events/payloads/<MESMO_NOME_DO_TOKEN>/
// se o evento precisa atravessar fronteira de plugin) sao a "interface".
// Quem trata o evento escreve, depois e em qualquer classe pertinente, um
// ON_EVENT_OBJ(token, handler, TipoDoPayload) dentro do seu proprio
// BEGIN_EVENT_HANDLER -- sem registro de "quem escuta o que" em lugar
// nenhum, pelo mesmo motivo de nao haver broker no restante do MIXR (ver
// src/poc/dis/single-thread/README.md secao 9).
//
// EID_ALERT (payload: events::TacticalAlert, events/payloads/EID_ALERT/
// TacticalAlert.hpp): broadcast DIRETO (Component::event() sobre
// getPlayers(), sem passar pelo subsistema Datalink) de um alerta tatico --
// alcanca qualquer player local ativo, tenha ele um Datalink ou nao. E o
// caminho que xnative::AlertDatalink::broadcastAlert() usa, ALEM do
// sendMessage() nativo (que so entrega a quem tem Datalink).
//
// Proximo token livre: USER_EVENTS + 2.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// REFERENCIA -- tokens NATIVOS do MIXR (mixr::base::Component, faixa 0-1999).
//
// So COMENTARIO, de proposito: estas constantes JA EXISTEM (enum dentro de
// mixr/base/eventTokens.hpp, incluido dentro de Component.hpp -- ver o
// comentario la: "DO NOT include this file directly"). Redeclara-las aqui
// seria duplicata morta e arriscaria colidir nome com o enum nativo se
// algum dia os dois headers forem incluidos juntos. O motivo de listar
// mesmo assim: quem aloca o PROXIMO token customizado (USER_EVENTS+N) so
// precisa evitar colisao com outro token customizado -- a faixa nativa e
// numericamente separada (USER_EVENTS = 2000 e o primeiro livre) -- mas ter
// a lista nativa INTEIRA aqui do lado poupa o ir-e-vir ate o pacote Conan
// toda vez que surge duvida tipo "RESET_EVENT e 1301 mesmo?" ou "esse nome
// ja existe no framework?". Fonte, para conferencia (regra do CLAUDE.md:
// "em caso de divergencia, quem vale e o pacote Conan"):
//   ~/.conan2/p/b/mixr*/p/include/mixr/base/eventTokens.hpp
//
// --- Eventos de TECLA (0..MAX_KEY_EVENT=999) -- sobem pela arvore de
//     containers se ninguem tratar (Component::event()) ---
//   CLR_KEY=1  BACK_SPACE=8  TAB_KEY=9  ENTER_KEY=13  ESC_KEY=27
//   FORWARD_SPACE=28  UP_ARROW_KEY=30  DOWN_ARROW_KEY=31  SPACE_BAR=32
//   DELETE_KEY=127
//   BUTTON_HIT=128  INPUT_RIGHT_EDGE=129  INPUT_LEFT_EDGE=130
//   ON_SINGLE_CLICK=131  ON_DOUBLE_CLICK=132  ON_CANCEL=133  ON_MOTION=134
//   LEFT_ARROW_KEY=BACK_SPACE  RIGHT_ARROW_KEY=FORWARD_SPACE
//   PAGE_UP_KEY=136  PAGE_DOWN_KEY=137  HOME_KEY=138  END_KEY=139
//   INSERT_KEY=140
//   F1_KEY=141  F2_KEY=142  F3_KEY=143  F4_KEY=144  F5_KEY=145  F6_KEY=146
//   F7_KEY=147  F8_KEY=148  F9_KEY=149  F10_KEY=150  F11_KEY=151
//   F12_KEY=152
//   OSB_T1..T10 = 200..209   (bezel de MFD, esquerda->direita)
//   OSB_R1..R10 = 210..219   (bezel de MFD, topo->base)
//   OSB_B1..B10 = 220..229   (bezel de MFD, esquerda->direita)
//   OSB_L1..L10 = 230..239   (bezel de MFD, topo->base)
//   USER_KEY_EVENT = 512     (primeiro evento de tecla livre p/ aplicacao)
//   MAX_KEY_EVENT  = 999     (teto da faixa de tecla)
//
// --- Eventos NAO-tecla (1000..1999) -- reservados ao framework, NAO sobem
//     pela arvore de containers ---
//   SHUTDOWN_EVENT = 1001                       // notificacao de desligamento
//
//   ON_ENTRY=1201  ON_EXIT=1202  ON_RETURN=1203  SELECT=1204   (graficos)
//   UPDATE_INSTRUMENTS=1210
//   UPDATE_VALUE=UPDATE_VALUE1=1211  UPDATE_VALUE2=1212  UPDATE_VALUE3=1213
//   UPDATE_VALUE4=1214  UPDATE_VALUE5=1215  UPDATE_VALUE6=1216
//   UPDATE_VALUE7=1217  UPDATE_VALUE8=1218  UPDATE_VALUE9=1219
//   SET_VISIBILITY=1221  SET_COLOR=1222  SET_LINEWIDTH=1223
//   SET_POSITION=1224  SET_WIDTH=1225  SET_HIGHLIGHT=1226
//   SET_UNDERLINE=1227  SET_REVERSED=1228  SET_SPECIAL=1229
//   SET_JUSTIFICATION=1230  SET_FLASHRATE=1231  SET_LINE=1232
//   SET_COLUMN=1233  SET_MATERIAL=1234  SET_TEXTURE=1235
//
//   RESET_EVENT=1301          // reset do cenario
//   FREEZE_EVENT=1302         // freeze (Boolean) -- so este player
//   FREEZE_EVENT_ALL=1303     // freeze (Boolean) -- todos os players
//   KILL_EVENT=1304           // kill -- payload opcional: quem matou
//   CRASH_EVENT=1305          // crash -- payload: player colidido ou 0 (terreno)
//   JETTISON_EVENT=1306       // ejetar carga externa
//   RF_EMISSION=1307          // emissao de RF (radar)          -- Antenna.cpp
//   RF_EMISSION_RETURN=1308   // retorno de emissao RF
//   DESIGNATOR_EVENT=1309     // mensagem de designador (laser)
//   DATALINK_MESSAGE=1310     // mensagem de Datalink            -- caminho (a) do EID_ALERT
//   ON_OWNSHIP_CONNECT=1311   // para o NOVO player 'ownship'
//   ON_OWNSHIP_DISCONNECT=1312 // para o VELHO player 'ownship', na desconexao
//   SCAN_START=1313  SCAN_END=1314           // varredura de radar
//   WPN_RELOAD=1315                          // recarregar armas
//   RF_REFLECTED_EMISSION=1316
//   RF_REFLECTIONS_REQUEST=1317
//   RF_REFLECTIONS_CANCEL=1318
//   IR_QUERY=1319  IR_QUERY_RETURN=1320      // consulta/retorno IR
//   SAT_COMM_MSG=1321                        // mensagem de satelite
//   DE_EMISSION=1322                         // emissao de energia direcionada
//   REFUEL_EVENT=1323                        // reabastecimento
//
//   --- HOTAS (manche/manete fisicos) ---
//   SENSOR_RTS=1400  TGT_STEP_EVENT=1401  TGT_DESIGNATE=1402
//   WPN_REL_EVENT=1405        // liberar arma (Boolean ou disparo unico)
//   TRIGGER_SW_EVENT=1406     // gatilho    (Boolean ou disparo unico)
//   TMS_FWD_EVENT=1407  TMS_AFT_EVENT=1408
//   TMS_LEFT_EVENT=1409  TMS_RIGHT_EVENT=1410      // TMS (target mgmt switch)
//   DMS_FWD_EVENT=1411  DMS_AFT_EVENT=1412
//   DMS_LEFT_EVENT=1413  DMS_RIGHT_EVENT=1414      // DMS (display mgmt switch)
//   CMS_FWD_EVENT=1415  CMS_AFT_EVENT=1416
//   CMS_LEFT_EVENT=1417  CMS_RIGHT_EVENT=1418      // CMS (countermeasure switch)
//   PINKY_SW_EVENT=1419  NWS_SW_EVENT=1420  CURSOR_ZERO_EVENT=1421
//   CURSOR_X_EVENT=1422  CURSOR_Y_EVENT=1423       // posicao de cursor (double)
//
//   USER_EVENTS = 2000   // PRIMEIRO token livre p/ aplicacao -- base de
//                        // TODA constante deste arquivo (EID_ALERT =
//                        // USER_EVENTS + 1, e assim por diante).
//------------------------------------------------------------------------------
constexpr int EID_ALERT{base::Component::USER_EVENTS + 1};

} // namespace events
} // namespace mixr

#endif
