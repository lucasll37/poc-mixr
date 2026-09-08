import React, { useState, useEffect, useLayoutEffect, useMemo, useRef, useCallback } from "react";

/* ==================================================================== *
 * MIXR — Explorador de execução, EDL e classes built-in   (v6)
 *
 * MODEL, FACTORIES, SNIPPETS e STATS NAO sao declarados neste arquivo --
 * sao injetados em tempo de build por docs/manual/compile.js, concatenando
 * o texto de docs/manual/catalog.generated.js (escrito por
 * tools/generate_manual_catalog.py; 'make docs' roda o gerador ANTES do
 * compile.js) como um <script> proprio, antes do app transpilado. Universo:
 * as 7 factories nativas que models/BUILT-IN.md ja usa como escopo (base,
 * models, simulation, terrain, interop/dis, linkage, recorder) mais o
 * plugin de producao models/players/A-4 -- so classe com despacho REAL num
 * factory.cpp, nunca "toda classe com DECLARE_SUBCLASS em algum header".
 * Nada nesses dados e digitado a mao:
 *   - heranca      <- DECLARE_SUBCLASS nos headers
 *   - nome fabrica <- IMPLEMENT_*SUBCLASS nos .cpp
 *   - registro     <- name == X::getFactoryName() nos factory.cpp (universo
 *                      ja filtrado por isso -- toda entrada e registrada)
 *   - slots        <- BEGIN_SLOTTABLE / END_SLOTTABLE
 *   - fases        <- definicoes de dynamics/transmit/receive/process
 *   - trechos      <- corpo do metodo, com arquivo e linha reais
 * ==================================================================== */

const PHASES = [
  { n: 0, m: "dynamics", label: "Dinâmica" },
  { n: 1, m: "transmit", label: "Transmitem" },
  { n: 2, m: "receive", label: "Recebem" },
  { n: 3, m: "process", label: "Lógica" },
];
const DEPTH_LABELS = ["raiz", "executivo e E/S", "players", "sistemas primários", "subsistemas", "detalhe", "ações"];

/* ---------- consultas ao modelo ---------- */
const cls = (c) => MODEL[c] || null;
const chainOf = (c) => (MODEL[c] ? MODEL[c].ch : [c]);
// As 4 raízes do ciclo de decisão (mixr::base::ubf) -- usado só pelo filtro
// "Decisão (UBF)" do Catálogo. Pega Agent/AgentTC/SimAgent/MultiActorAgent
// (via "Agent" na cadeia), Arbiter/qualquer behavior futuro (via
// "AbstractBehavior"), e os dois papéis restantes.
const UBF_ROOTS = ["Agent", "AbstractBehavior", "AbstractState", "AbstractAction"];
const workPhases = (c) => (MODEL[c] ? MODEL[c].wp : []);
const phaseOwner = (c, p) => (MODEL[c] && MODEL[c].po ? MODEL[c].po[String(p)] : null);
const dispatches = (c) => !!(MODEL[c] && MODEL[c].d);
const allSlots = (c) => {
  const out = [];
  chainOf(c).forEach((a) => (MODEL[a] ? MODEL[a].sl : []).forEach((s) => out.push([s, a])));
  return out;
};
const factoryOf = (c) => (MODEL[c] && MODEL[c].f ? MODEL[c].f : c);

/* ============================= cenário ============================== */
/* Só o que é escolha de cenário. Herança, fases e slots vêm do MODEL.  */

const N = (id, c, o = {}) => ({ id, cls: c, children: [], ...o });

// Um único ( Aircraft ) carregando os DEZ sistemas primários que
// Player::updateSystemPointers() resolve por TIPO (Player.cpp:3141-3151) — e,
// dentro de cada um, tudo que a fábrica nativa de mixr::models sabe construir.
// É o mesmo desenho de src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in
// ("qual o player mais elaborado que dá para montar só com componentes NATIVOS
// do mixr::models?"), com uma única diferença deliberada: ali o Datalink é
// ( AlertDatalink ) — a ÚNICA classe não nativa daquele cenário — e aqui é
// ( Datalink ) puro, porque esta página é sobre o framework, não sobre um
// plugin. falcon2 (o alvo, pilha mínima) e o míssil dinâmico completam o
// quadro: side vermelho, alvo do RWR/TWS, e o player que nasce em runtime.
const SCENARIO = N("station", "Station", {
  edl: "station", via: null, thread: "tc",
  children: [
    N("io", "IoHandler", { edl: "io", via: "ioHandler:", thread: "tc",
      note: "Não está registrada em linkage/factory.cpp — nome de fábrica BaseIoHandler. Escrever ( IoHandler ) no EDL não constrói nada." }),
    N("rec", "DataRecorder", { edl: "rec", via: "dataRecorder:", thread: "fundo" }),
    N("net", "NetIO", { edl: "net1", via: "networks:", thread: "rede",
      note: "Nome de fábrica DisNetIO. A mesma classe NetIO existe em dis, hla e rprfom, cada uma com o seu nome." }),
    N("sim", "WorldModel", {
      edl: "sim", via: "simulation:", thread: "tc",
      children: [
        N("terr", "QuadMap", { edl: "terrain", via: "terrain:", thread: "tc" }),

        N("ac", "Aircraft", {
          edl: "falcon1", via: "players:", player: true, thread: "tc",
          note: "53 das 96 classes que mixr::models::factory publica, num Aircraft só — ver 'built-in_mixr_1' no CLAUDE.md.",
          children: [
            // --- 1) DynamicsModel ---------------------------------------
            N("dyn", "JSBSimModel", { edl: "dyn", via: "components:", thread: "tc" }),

            // --- 2) Pilot ------------------------------------------------
            N("ap", "Autopilot", { edl: "ap", via: "components:", thread: "tc",
              note: "leadPlayerName aponta pra ac2 por nome — mesmo mecanismo do antennaName/trackManagerName abaixo, aqui pra formação em vez de sensor." }),

            // --- 3) Navigation --------------------------------------------
            N("nav", "Ins", {
              edl: "nav", via: "components:", thread: "tc+fundo",
              note: "( Ins ) É uma ( Navigation ) (Ins : public Navigation) — por isso o Gps entra como FILHO, não irmão: findByType() pegaria só o primeiro.",
              children: [
                N("gps", "Gps", { edl: "gps", via: "components:", thread: "tc+fundo" }),
                N("bull", "Bullseye", { edl: "bull", via: "bullseye:", thread: "fundo" }),
                N("route", "Route", {
                  edl: "rota", via: "route:", thread: "fundo",
                  note: "autoSequencer() dispara a ( Action ) do steerpoint que a aeronave acabou de passar — por DISTÂNCIA, independe de navMode.",
                  children: [
                    N("wp1", "Steerpoint", { edl: "wp1", via: "components:", thread: "fundo",
                      children: [N("act1", "ActionDecoyRelease", { edl: "wp1.action", via: "action:", thread: "tc" })] }),
                    N("wp2", "Steerpoint", { edl: "wp2", via: "components:", thread: "fundo",
                      children: [N("act2", "ActionImagingSar", { edl: "wp2.action", via: "action:", thread: "tc" })] }),
                    N("wp3", "Steerpoint", { edl: "wp3", via: "components:", thread: "fundo",
                      children: [N("act3", "ActionCamouflageType", { edl: "wp3.action", via: "action:", thread: "tc",
                        note: "Troca camouflageType em runtime — é o índice que SigSwitch::getRCS() usa pra escolher qual dos 6 filhos de assinatura responde." })] }),
                    N("wp4", "Steerpoint", { edl: "wp4", via: "components:", thread: "fundo",
                      children: [N("act4", "ActionWeaponRelease", { edl: "wp4.action", via: "action:", thread: "tc",
                        note: "'station:' não escolhe a estação — trigger() chama sms->releaseOneBomb() e ignora o valor; quem sai é a primeira Bomb livre." })] }),
                  ],
                }),
              ],
            }),

            // --- 4) Datalink ----------------------------------------------
            N("dl", "Datalink", { edl: "dl", via: "components:", thread: "tc",
              note: "Implementa dynamics() — fase 0, não fase 3. Aqui é o Datalink NATIVO — no cenário real este é o único slot ocupado por um plugin (AlertDatalink)." }),

            // --- 5) Radio ---------------------------------------------------
            N("comm", "CommRadio", {
              edl: "comm1", via: "components:", thread: "tc",
              children: [
                N("iff", "Iff", { edl: "iff", via: "components:", thread: "tc",
                  note: "Iff DERIVA de Radio — por isso entra ANINHADO dentro do CommRadio, nunca como irmão (mesma regra do Gps dentro do Ins)." }),
              ],
            }),

            // --- 6) Gimbal ----------------------------------------------
            N("gim", "Gimbal", {
              edl: "antennas", via: "components:", thread: "tc",
              note: "UMA antena por sensor de RF: Antenna::setSystem() guarda um único ponteiro — por isso são 6 antenas, não 1 compartilhada.",
              children: [
                N("a1", "Antenna", { edl: "ant_tws", via: "components:", thread: "tc+fundo" }),
                N("a2", "Antenna", { edl: "ant_stt", via: "components:", thread: "tc+fundo" }),
                N("a3", "Antenna", { edl: "ant_gmti", via: "components:", thread: "tc+fundo" }),
                N("a4", "Antenna", { edl: "ant_rwr", via: "components:", thread: "tc+fundo",
                  note: "Cobertura esférica, ganho baixo — não ilumina nada, só escuta o que os outros transmitem." }),
                N("a5", "Antenna", { edl: "ant_jam", via: "components:", thread: "tc+fundo" }),
                N("stab", "StabilizingGimbal", {
                  edl: "estab", via: "components:", thread: "tc",
                  note: "Gimbal DENTRO de gimbal: a antena do SAR pendurada aqui, contra-rolada. findByName() é recursivo — 'ant_sar' continua alcançável por nome simples.",
                  children: [N("a6", "Antenna", { edl: "ant_sar", via: "components:", thread: "tc+fundo" })],
                }),
                N("irst", "IrSeeker", { edl: "irst", via: "components:", thread: "tc+fundo",
                  note: "É um ScanGimbal (logo um Gimbal) — por isso mora aqui, não solto no player: solto disputaria o ponteiro primário de Gimbal." }),
              ],
            }),

            // --- 7) RfSensor -------------------------------------------
            N("sens", "SensorMgr", {
              edl: "sensors", via: "components:", thread: "tc",
              note: "É um RfSensor: o contêiner que permite mais de um sensor de RF no mesmo player.",
              children: [
                N("tws", "Tws", { edl: "tws", via: "components:", thread: "tc+fundo" }),
                N("stt", "Stt", { edl: "stt", via: "components:", thread: "tc+fundo" }),
                N("gmti", "Gmti", { edl: "gmti", via: "components:", thread: "tc+fundo" }),
                N("sar", "Sar", { edl: "sar", via: "components:", thread: "tc+fundo" }),
                N("rwr", "Rwr", { edl: "rwr", via: "components:", thread: "tc+fundo",
                  note: "disableEmissions:true — só recebe. Não entrega a posição da própria aeronave a quem também tem RWR." }),
                N("jam", "Jammer", { edl: "jam", via: "components:", thread: "tc+fundo" }),
              ],
            }),

            // --- 8) IrSystem ---------------------------------------------
            N("irs", "IrSensor", { edl: "irsystem", via: "components:", thread: "tc+fundo",
              note: "Não é ( MergingIrSensor ): essa exige um AirAngleOnlyTrkMgrPT, referenciado por reset() mas sem branch em models/factory.cpp — não construível neste fork." }),

            // --- 9) OnboardComputer --------------------------------------
            N("obc", "OnboardComputer", {
              edl: "obc", via: "components:", thread: "tc",
              note: "O contêiner de TrackManager. A ordem só importa pra quem pede o 'primário' por tipo — o resto pede por NOME (twsTrkMgr).",
              children: [
                N("ttm", "AirTrkMgr", { edl: "twsTrkMgr", via: "components:", thread: "tc" }),
                N("rtm", "RwrTrkMgr", { edl: "rwrTrkMgr", via: "components:", thread: "tc" }),
                N("gtm", "GmtiTrkMgr", { edl: "gmtiTrkMgr", via: "components:", thread: "tc" }),
                N("itm", "AirAngleOnlyTrkMgr", { edl: "irTrkMgr", via: "components:", thread: "tc" }),
              ],
            }),

            // --- 10) StoresMgr --------------------------------------------
            N("sto", "SimpleStoresMgr", {
              edl: "stores", via: "components:", thread: "tc",
              note: "( StoresMgr ) no EDL constrói ESTA classe — a abstrata StoresMgr registra-se como BaseStoresMgr e não é construível.",
              children: [
                N("s1", "Aam", { edl: "1", via: "stores:", thread: "tc",
                  note: "Fábrica registra como \"AamMissile\" — nome de classe e nome de fábrica divergem." }),
                N("s2", "Aam", { edl: "2", via: "stores:", thread: "tc", dynamic: true }),
                N("s3", "Agm", { edl: "3", via: "stores:", thread: "tc" }),
                N("s4", "Sam", { edl: "4", via: "stores:", thread: "tc" }),
                N("s5", "Bomb", { edl: "5", via: "stores:", thread: "tc",
                  note: "A primeira Bomb livre da lista — é esta que ( ActionWeaponRelease ) do wp4 solta, não importa o 'station:' pedido." }),
                N("s6", "Chaff", { edl: "6", via: "stores:", thread: "tc" }),
                N("s7", "Flare", { edl: "7", via: "stores:", thread: "tc" }),
                N("s8", "Decoy", { edl: "8", via: "stores:", thread: "tc",
                  note: "A primeira Decoy livre — é esta que ( ActionDecoyRelease ) do wp1 solta." }),
                N("s9", "Gun", { edl: "9", via: "stores:", thread: "tc" }),
                N("s10", "FuelTank", { edl: "10", via: "stores:", thread: "—",
                  note: "( ExternalStore ), não arma: a mesma lista de 'stores:' aceita as duas famílias." }),
                N("s11", "AvionicsPod", { edl: "11", via: "stores:", thread: "—" }),
              ],
            }),

            // --- extra: detecção de colisão -----------------------------
            N("col", "CollisionDetect", { edl: "colisao", via: "components:", thread: "tc",
              note: "Não é sistema primário — é um Component comum, atualizado como qualquer outro filho." }),

            // --- assinatura RF comutável ---------------------------------
            N("sig", "SigSwitch", {
              edl: "sig", via: "signature:", thread: "—",
              note: "Não tem slot próprio: getRCS() escolhe o filho de índice camouflageType — é o que ( ActionCamouflageType ) do wp3 troca em runtime.",
              children: [
                N("sg1", "SigSphere", { edl: "sig.limpo", via: "components:", thread: "—" }),
                N("sg2", "SigPlate", { edl: "sig.placa", via: "components:", thread: "—" }),
                N("sg3", "SigConstant", { edl: "sig.const", via: "components:", thread: "—" }),
                N("sg4", "SigDihedralCR", { edl: "sig.died", via: "components:", thread: "—" }),
                N("sg5", "SigTrihedralCR", { edl: "sig.tried", via: "components:", thread: "—" }),
                N("sg6", "SigAzEl", { edl: "sig.azel", via: "components:", thread: "—" }),
              ],
            }),

            // --- assinatura IR (do ALVO, não do sensor) -----------------
            N("irsig", "IrSignature", {
              edl: "irsig", via: "irSignature:", thread: "—",
              note: "( AircraftIrSignature ) seria mais elaborada, mas getAirframeSignature() desreferencia airframeSignatureTable SEM checar nulo — derruba o processo sem as 6 tabelas.",
              children: [N("irsph", "IrSphere", { edl: "irsig.shape", via: "irShapeSignature:", thread: "—" })],
            }),

            // --- ciclo de decisão UBF (mixr::base::ubf) -------------------
            // Sem dado deste repositório: AgentTC/Arbiter/AbstractState/
            // AbstractBehavior são classes REAIS do fork (mesma cadeia/slots
            // do MODEL já usado no resto da página). Os votos 10/6/3 e os 3
            // behaviors são DIDÁTICOS — não correspondem a nenhum
            // comportamento deste repositório (ver a fase 0 da trilha "Thread
            // de Tempo Crítico", onde a decisão de fato roda).
            // Componente do Aircraft, não mais irmão de "sim" ligado por
            // nome: AgentTC não sobrescreve initActor(), então o ator
            // default é o próprio container() -- containment, e por isso
            // este nó tem de morar aqui dentro, como o ÚLTIMO item de
            // components: (mesma posição de FlightAgentTC na produção).
            N("agent", "AgentTC", {
              edl: "agent", via: "components:", thread: "tc",
              note: "AgentTC roda no laço de TEMPO CRÍTICO (updateTC), sem filtro de fase — por isso mora dentro do Aircraft: sem initActor() próprio, o ator é o container() (containment, não actorPlayerName: como o SimAgent).",
              children: [
                N("ubfstate", "AbstractState", {
                  edl: "state", via: "state:", thread: "—",
                  note: "updateState(actor) monta a percepção a cada ciclo. O corpo em AbstractState já é real: só recursa nos filhos (é um estado composto) — quem de fato lê algo do ator é uma subclasse própria." }),
                N("ubfarb", "Arbiter", {
                  edl: "behavior", via: "behavior:", thread: "—",
                  note: "UbfArbiter — ele MESMO é um AbstractBehavior, com uma lista de behaviors filhos (slot behaviors:).",
                  children: [
                    N("ubfbeh1", "AbstractBehavior", { edl: "b1", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 10 (o maior)." }),
                    N("ubfbeh2", "AbstractBehavior", { edl: "b2", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 6." }),
                    N("ubfbeh3", "AbstractBehavior", { edl: "b3", via: "behaviors:", thread: "—", note: "Exemplo didático — voto 3." }),
                  ],
                }),
              ],
            }),
          ],
        }),

        N("ac2", "Aircraft", {
          edl: "falcon2", via: "players:", player: true, thread: "tc",
          note: "Pilha mínima, de propósito: contraste e alvo do TWS/RWR de ac.",
          children: [
            N("dyn2", "RacModel", { edl: "dyn", via: "components:", thread: "tc" }),
            N("sig2", "SigConstant", { edl: "sig", via: "signature:", thread: "—" }),
          ],
        }),

        N("flyout", "Aam", { edl: "addNewPlayer()", via: "addNewPlayer()", player: true, dynamic: true, thread: "tc" }),
      ],
    }),
  ],
});

function normalize(n, parent) {
  n.parent = parent ? parent.id : null;
  n.phases = workPhases(n.cls);
  n.disp = dispatches(n.cls);
  (n.children || []).forEach((c) => normalize(c, n));
  return n;
}
normalize(SCENARIO, null);
const flat = (n, out = []) => (out.push(n), (n.children || []).forEach((c) => flat(c, out)), out);
const ALL = flat(SCENARIO);
const byId = Object.fromEntries(ALL.map((n) => [n.id, n]));
const IN_SCENARIO = new Set(ALL.map((n) => n.cls));
const ancestors = (id) => { const o = []; let c = byId[id]; while (c && c.parent) { o.push([c.parent, c.id]); c = byId[c.parent]; } return o; };

// `dy`: ajuste vertical fino do rotulo, em cima do y natural ((a.y+b.y)/2) --
// ver o comentario perto de onde NAME_LINKS e desenhado. Links que saem do
// MESMO no colapsariam no mesmo ponto sem isto (dois links do mesmo "from"
// caem na mesma zona de folga, e o rotulo "via:"/"dt:" do proprio no ocupa o
// centro da linha) -- valores calibrados olhando o resultado renderizado,
// nao adivinhados.
// `dy` NAO foi adivinhado: a coluna "sensors"/"antennas"/"stores" empilha
// dezenas de nos a cada ROW (42px), cada um com seu proprio rotulo "via:"
// (banda de 12px, folga de so 30px entre uma banda e a proxima) -- o meio-
// termo natural ((a.y+b.y)/2) de varios destes links caia bem em cima da
// banda de ALGUM no da coluna, ou de OUTRO link do mesmo "from". Calculado
// medindo as posicoes renderizadas de verdade (script python offline, nao
// tentativa-e-erro visual) e escolhendo o CENTRO do vao de 30px mais proximo
// do meio-termo natural -- ver doc-polish-progress.md para o metodo.
const NAME_LINKS = [
  { from: "tws", to: "a1", slot: "antennaName:", dy: -7 },
  { from: "tws", to: "ttm", slot: "trackManagerName:", dy: -7 },
  { from: "stt", to: "ttm", slot: "trackManagerName:", dy: 14 },
  { from: "gmti", to: "gtm", slot: "trackManagerName:", dy: 0 },
  { from: "rwr", to: "rtm", slot: "trackManagerName:", dy: 0 },
  { from: "sar", to: "a6", slot: "antennaName:", dy: 0 },
  { from: "irs", to: "itm", slot: "trackManagerName:", dy: -20 },
  { from: "irs", to: "irst", slot: "seekerName:", dy: 10 },
  { from: "ap", to: "ac2", slot: "leadPlayerName:", dy: 0 },
];

/* =============================== EDL ================================ */

// Condensado do MESMO cenário real que o motivou --
// src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in ("qual o player
// mais elaborado que dá para montar só com componentes NATIVOS do
// mixr::models?") -- com uma troca deliberada: datalink: ( Datalink ) puro no
// lugar de ( AlertDatalink ), a única classe NÃO nativa daquele cenário. Aqui
// é só sobre o framework.
const EDL_TEXT = `( Station
   tcRate: 50            // thread TC a 50 Hz
   bgRate: 20            // thread de fundo -- taxa PROPRIA
   netRate: 50
   ownship: "falcon1"
   startupResetTimer: ( Seconds 0.1 )   // sem isto nada roda

   ioHandler:    ( IoHandler )     // NAO registrada na fabrica
   dataRecorder: ( DataRecorder )
   networks:   { net1: ( DisNetIO ) }

   simulation: ( WorldModel
      terrain: ( QuadMap )

      players: {

         // ===========================================================
         // falcon1 -- os DEZ sistemas primarios que
         // Player::updateSystemPointers() acha por TIPO, com tudo que a
         // fabrica nativa sabe construir dentro de cada um
         // ===========================================================
         falcon1: ( Aircraft
            side: blue   type: "A4"   id: 101
            initPosition: [ 0 0 -1750 ]

            // -- assinatura RF comutavel: 6 filhos, camouflageType escolhe
            signature: ( SigSwitch
               components: {
                  limpo: ( SigSphere radius: 3.0 )
                  placa: ( SigPlate a: ( Meters 6.0 ) b: ( Meters 2.0 ) )
                  const: ( SigConstant rcs: ( SquareMeters 2.5 ) )
                  died:  ( SigDihedralCR a: ( Meters 1.5 ) b: ( Meters 1.5 ) )
                  tried: ( SigTrihedralCR a: ( Meters 1.5 ) b: ( Meters 1.5 ) )
                  azel:  ( SigAzEl inDegrees: true inDecibel: true
                           table: ( Table2 x: [ ... ] y: [ ... ] data: { [ ... ] } ) )
               }
            )

            // -- assinatura IR: e do ALVO, nao do sensor
            irSignature: ( IrSignature
               baseHeatSignature: 320.0   emissivity: 0.75
               effectiveArea: ( SquareMeters 3.0 )
               irShapeSignature: ( IrSphere radius: ( Meters 2.0 ) )
            )

            components: {

               // --- 1) DynamicsModel -------------------------------
               dyn: ( JSBSimModel
                  rootDir: "./dist/share/mixr-plugins/flight/jsbsim/"
                  model: "A4"
               )

               // --- 2) Pilot ----------------------------------------
               ap: ( Autopilot
                  navMode: false
                  headingHoldMode: true   altitudeHoldMode: true
                  velocityHoldMode: true
                  leadPlayerName: falcon2       // resolvido por STRING
               )

               // --- 3) Navigation -----------------------------------
               // ( Ins ) E uma ( Navigation ) -- Gps entra como FILHO,
               // nao irmao (findByType() pegaria so o primeiro).
               nav: ( Ins
                  bullseye: ( Bullseye
                     latitude: ( Degrees -22.25 ) longitude: ( Degrees -42.48 )
                  )
                  route: ( Route
                     to: 1   autoSequence: true
                     components: {
                        wp1: ( Steerpoint
                           stptType: DEST   xPos: ( Meters 9290 ) yPos: ( Meters 3000 )
                           action: ( ActionDecoyRelease numToLaunch: 2 interval: ( Seconds 1.0 ) )
                        )
                        wp2: ( Steerpoint
                           stptType: TGT   xPos: ( Meters 6000 ) yPos: ( Meters 7370 )
                           action: ( ActionImagingSar
                              sarLatitude:  ( LatLon direction: "s" degrees: 22 minutes: 12 )
                              sarLongitude: ( LatLon direction: "w" degrees: 42 minutes: 24 )
                           )
                        )
                        wp3: ( Steerpoint
                           stptType: FIX   xPos: ( Meters 3230 ) yPos: ( Meters 4000 )
                           action: ( ActionCamouflageType camouflageType: 1 )
                        )
                        wp4: ( Steerpoint
                           stptType: IP   xPos: ( Meters 6000 ) yPos: ( Meters 1600 )
                           action: ( ActionWeaponRelease
                              targetLatitude:  ( LatLon direction: "s" degrees: 22 minutes: 18 )
                              targetLongitude: ( LatLon direction: "w" degrees: 42 minutes: 33 )
                              station: 5              // IGNORADO por trigger()
                           )
                        )
                     }
                  )
                  components: { gps: ( Gps ) }
               )

               // --- 4) Datalink -------------------------------------
               // nativo aqui -- no cenario real este slot e o UNICO
               // ocupado por um plugin (AlertDatalink)
               dl: ( Datalink )

               // --- 5) Radio ----------------------------------------
               // Iff E um Radio -- entra ANINHADO, nao irmao
               comm1: ( CommRadio
                  radioID: 1   numChannels: 4   channel: 1
                  components: {
                     iff: ( Iff
                        mode1: 3   mode2: 4096   mode3a: 1200
                        enableMode1: true   enableMode2: true   enableMode3a: true
                     )
                  }
               )

               // --- 6) Gimbal ---------------------------------------
               // UMA antena por sensor -- Antenna::setSystem() guarda
               // UM ponteiro, por isso sao 6, nao 1 compartilhada
               antennas: ( Gimbal
                  components: {
                     ant_tws: ( Antenna polarization: horizontal gain: ( dB 42 ) )
                     ant_stt: ( Antenna polarization: horizontal gain: ( dB 44 ) )
                     ant_gmti: ( Antenna polarization: vertical gain: ( dB 38 ) )
                     ant_rwr: ( Antenna polarization: vertical gain: ( dB 3 ) )
                     ant_jam: ( Antenna polarization: vertical gain: ( dB 20 ) )
                     // gimbal DENTRO de gimbal: a antena do SAR contra-rolada
                     estab: ( StabilizingGimbal
                        stabilizingMode: roll
                        components: { ant_sar: ( Antenna polarization: vertical gain: ( dB 40 ) ) }
                     )
                     // e um ScanGimbal (logo um Gimbal) -- por isso mora
                     // aqui, nao solto no player
                     irst: ( IrSeeker searchVolume: [ 0.5236 0.1745 ] numBars: 2 )
                  }
               )

               // --- 7) RfSensor -------------------------------------
               // SensorMgr E um RfSensor: o conteiner que permite mais
               // de um sensor de RF no mesmo player
               sensors: ( SensorMgr
                  components: {
                     tws: ( Tws trackManagerName: twsTrkMgr antennaName: ant_tws
                                frequency: ( GigaHertz 3.0 ) PRF: ( Hertz 500.0 ) )
                     stt: ( Stt trackManagerName: twsTrkMgr antennaName: ant_stt
                                frequency: ( GigaHertz 3.0 ) PRF: ( Hertz 2000.0 ) )
                     gmti: ( Gmti trackManagerName: gmtiTrkMgr antennaName: ant_gmti
                                  frequency: ( GigaHertz 9.5 ) )
                     sar: ( Sar antennaName: ant_sar chipSize: 512 )
                     rwr: ( Rwr trackManagerName: rwrTrkMgr antennaName: ant_rwr
                                disableEmissions: true )         // so RECEBE
                     jam: ( Jammer antennaName: ant_jam disableEmissions: true )
                  }
               )

               // --- 8) IrSystem --------------------------------------
               // NAO ( MergingIrSensor ): exige AirAngleOnlyTrkMgrPT, sem
               // branch em models/factory.cpp -- nao construivel neste fork
               irsystem: ( IrSensor
                  seekerName: irst   trackManagerName: irTrkMgr
                  sensorType: "contrast"
               )

               // --- 9) OnboardComputer -------------------------------
               // o conteiner de TrackManager -- resto do sistema pede
               // por NOME (twsTrkMgr), a ordem so importa pro findByType()
               obc: ( OnboardComputer
                  components: {
                     twsTrkMgr: ( AirTrkMgr maxTracks: 20 alpha: 1.0 beta: 0.5 )
                     rwrTrkMgr: ( RwrTrkMgr maxTracks: 20 alpha: 2.0 )
                     gmtiTrkMgr: ( GmtiTrkMgr maxTracks: 20 alpha: 1.0 beta: 0.5 )
                     irTrkMgr: ( AirAngleOnlyTrkMgr
                        maxTracks: 20   azimuthBin: ( Degrees 5 ) elevationBin: ( Degrees 5 )
                     )
                  }
               )

               // --- 10) StoresMgr -------------------------------------
               // nome de fabrica de SimpleStoresMgr e "StoresMgr" -- a
               // classe abstrata StoresMgr registra-se como BaseStoresMgr
               stores: ( StoresMgr
                  numStations: 11
                  stores: {
                     1: ( AamMissile id: 501 type: "AIM-9"  maxTOF: ( Seconds 60 ) )
                     2: ( AamMissile id: 502 type: "AIM-9"  maxTOF: ( Seconds 60 ) )
                     3: ( AgmMissile id: 503 type: "AGM-65" maxTOF: ( Seconds 90 ) )
                     4: ( Sam        id: 504 type: "SAM-demo" )
                     5: ( Bomb       id: 505 type: "MK-82" arming: free_fall )
                     6: ( Chaff      id: 506 type: "chaff" )
                     7: ( Flare      id: 507 type: "flare" )
                     8: ( Decoy      id: 508 type: "decoy" )
                     9: ( Gun type: "M61A1" rounds: 510 rate: 6000
                              bulletType: ( Bullet id: 509 type: "20mm" ) )
                     10: ( FuelTank  type: "tanque-ventral" jettisonable: true )
                     11: ( AvionicsPod type: "pod-recon" )
                  }
               )

               // --- extra: nao e sistema primario, e Component comum --
               colisao: ( CollisionDetect collisionRange: ( Meters 100 ) maxPlayers: 20 )
            }
         )

         // ===========================================================
         // falcon2 -- pilha MINIMA: contraste e alvo do TWS/RWR de falcon1
         // ===========================================================
         falcon2: ( Aircraft
            side: red   type: "A4"   id: 102
            signature: ( SigConstant rcs: ( SquareMeters 12.0 ) )
            components: { dyn: ( RacModel ) }
         )
      }
   )
)`.split("\n");

const EDL_RANGE = {
  station: [0, 9], io: [7, 7], rec: [8, 8], net: [9, 9],
  sim: [11, 212], terr: [12, 12],
  ac: [21, 201], dyn: [48, 51], ap: [54, 59],
  nav: [64, 97], gps: [96, 96], bull: [65, 67], route: [68, 95],
  wp1: [71, 74], act1: [73, 73], wp2: [75, 81], act2: [77, 80],
  wp3: [82, 85], act3: [84, 84], wp4: [86, 93], act4: [88, 92],
  dl: [102, 102], comm: [106, 114], iff: [109, 112],
  gim: [119, 135], a1: [121, 121], a2: [122, 122], a3: [123, 123],
  a4: [124, 124], a5: [125, 125], stab: [127, 130], a6: [129, 129],
  irst: [133, 133],
  sens: [140, 153], tws: [142, 143], stt: [144, 145], gmti: [146, 147],
  sar: [148, 148], rwr: [149, 150], jam: [151, 151],
  irs: [158, 161],
  obc: [166, 175], ttm: [168, 168], rtm: [169, 169], gtm: [170, 170], itm: [171, 173],
  sto: [180, 196], s1: [183, 183], s2: [184, 184], s3: [185, 185], s4: [186, 186],
  s5: [187, 187], s6: [188, 188], s7: [189, 189], s8: [190, 190], s9: [191, 192],
  s10: [193, 193], s11: [194, 194],
  col: [199, 199],
  sig: [26, 36], sg1: [28, 28], sg2: [29, 29], sg3: [30, 30], sg4: [31, 31],
  sg5: [32, 32], sg6: [33, 34],
  irsig: [39, 43], irsph: [42, 42],
  ac2: [206, 210], dyn2: [209, 209], sig2: [208, 208],
  flyout: [183, 184],
};

// Ilustrativo -- NAO e o EDL de produção deste repositório. A forma dos
// slots é real (state/behavior/behaviors/vote — os mesmos nomes que aparecem
// em MODEL para AgentTC/Agent/Arbiter/AbstractBehavior); o conteúdo concreto
// (classe do state, número de behaviors, votos) é didático. Sem
// actorPlayerName: -- essa é a diferença que justifica "components:" aqui:
// AgentTC não sobrescreve initActor(), então resolve o ator por containment
// (é filho do Aircraft), não por nome. Ver a nota no subtree de SCENARIO e a
// fase 0 da trilha "Thread de Tempo Crítico".
const UBF_EDL_TEXT = `// ilustrativo -- forma real dos slots, conteudo didatico (nao e a configuracao deste repositorio)
// AgentTC roda no laco de TEMPO CRITICO (updateTC), nao no de fundo -- por
// isso e um COMPONENTE do Aircraft (resolve o ator por containment: default
// initActor() usa container()), nao mais por actorPlayerName:.
// "UbfAgentTC" nao esta encadeada em base/factory.cpp (so "UbfAgent" esta) --
// um projeto real precisaria de uma subclasse propria, com fabrica propria,
// pra isto valer em EDL de verdade.
components: {                          // do Aircraft (falcon1) -- ver a aba EDL de "ac"
   ...
   agent: ( UbfAgentTC
      state:    ( AbstractState )         // concreto: uma subclasse propria
      behavior: ( UbfArbiter
         behaviors: {
            ( AbstractBehavior vote: 10 )  // exemplo didatico
            ( AbstractBehavior vote: 6  )
            ( AbstractBehavior vote: 3  )
         }
      )
   )
}`.split("\n");

const UBF_EDL_RANGE = {
  agent: [9, 18], ubfstate: [10, 10],
  ubfarb: [11, 17], ubfbeh1: [13, 13], ubfbeh2: [14, 14], ubfbeh3: [15, 15],
};

/* ============================ geradores ============================= */

const DT = 0.02, FRAMES = 3, LAUNCH_FRAME = 1;
const fmt = (v) => `${(v * 1000).toFixed(1)} ms`;

/* ------------------------------------------------------------------------ *
 * windowLines() -- a razao de nao existir MAIS caixa de rolagem em cima de
 * codigo/EDL: em vez de mostrar o arquivo inteiro numa caixa de altura fixa
 * com overflow:auto (e um scrollTop calculado em JS pra "pular" ate a linha
 * certa), corta-se aqui, ANTES do render, uma janela de no maximo 'max'
 * linhas centrada no trecho destacado. A caixa cresce so ate o que sobrou --
 * nunca mais alto que o conteudo, nunca com barra de rolagem.
 * ------------------------------------------------------------------------ */
function windowLines(lines, hl, max) {
  const n = lines.length;
  if (n <= max) return { lines, offset: 0, cutBefore: false, cutAfter: false };
  const [hs, he] = hl || [0, 0];
  const mid = Math.floor((hs + he) / 2);
  let start = mid - Math.floor(max / 2);
  start = Math.max(0, Math.min(start, n - max));
  return {
    lines: lines.slice(start, start + max),
    offset: start,
    cutBefore: start > 0,
    cutAfter: start + max < n,
  };
}

/* método de fonte a mostrar para um nó numa fase */
function srcFor(node, ph) {
  const owner = phaseOwner(node.cls, ph);
  const key = owner ? `${owner}::${PHASES[ph].m}` : null;
  if (key && SNIPPETS[key]) return key;
  if (node.player) return "Player::phaseSwitch";
  if (node.disp) return "System::updateTC";
  return "Component::updateTC";
}

function traceFrames() {
  const steps = [];
  let stack = [], released = false, exec = 0, simT = 0;
  const push = (s) => steps.push({ ...s, i: steps.length, stack: [...stack], counters: { ...s.counters, exec, simT } });

  for (let fr = 0; fr < FRAMES; fr++) {
    const ctr = { cycle: 0, frame: fr, phase: null };
    stack = [{ label: "Station::updateTC", node: "station" }];
    push({ kind: "frame", node: "station", counters: ctr, dt: DT, src: "Station::updateTC", hl: [2, 10],
      title: `Quadro ${fr} — Station::updateTC(dt)`,
      body: `dt = ${fmt(DT)}. Os Timers avançam antes de tudo, para que isExpired() responda certo durante o resto do quadro; depois o hardware é lido. A ordem dos sete passos é fixa.` });

    stack.push({ label: "Simulation::updateTC", node: "sim" });
    PHASES.forEach((ph) => {
      const c = { ...ctr, phase: ph.n };
      push({ kind: "phase", node: "sim", counters: c, dt: DT / 4, src: "Simulation::phaseLoop", hl: [10, 22],
        title: `setPhase(${ph.n}) — ${ph.label}`,
        body: `A lista de players é percorrida inteira com dt/4 = ${fmt(DT / 4)} antes de a próxima fase começar. Com pool de threads, waitForAllCompleted() é a barreira: ninguém entra na recepção enquanto alguém ainda transmite.`,
        warn: ph.n === 1 ? "O Tdb que esta fase lê foi montado por Gimbal::processPlayersOfInterest() na thread de fundo, possivelmente há um ou dois quadros." : null });

      byId.sim.children.filter((p) => p.player && (!p.dynamic || released)).forEach((p) => {
        stack.push({ label: `${p.cls}::updateTC`, node: p.id });
        walk(p, ph, DT / 4, c, push, stack, fr, () => { released = true; });
        stack.pop();
      });
      exec += 1;
    });

    stack = [{ label: "Station::updateTC", node: "station" }];
    push({ kind: "frameEnd", node: "station", counters: ctr, dt: DT, src: "Simulation::frameCount", hl: [1, 8],
      title: `Fim do quadro ${fr}`,
      body: `frame() vai a ${fr + 1}. A 16 quadros incCycle() dispara e frame() volta a zero. É este contador que o idioma frame() % N == 0 usa para agendar lógica em sub-taxa.` });
    simT += DT;
  }
  return steps;
}

function walk(node, ph, dt, ctr, push, stack, fr, doRelease) {
  // AgentTC::updateTC() é invocado em toda fase pela recursão genérica de
  // Component (que não filtra) -- mas não participa do switch(phase) nem
  // repassa BaseClass::updateTC() aos próprios filhos (confirmado no fonte:
  // nem Agent::updateData nem AgentTC::updateTC chamam BaseClass::
  // update*()): controller() é chamado direto, FORA dessa recursão. Por
  // isso este nó é um beco sem saída pra recursão GENÉRICA (não desce pra
  // state/behavior via o mecanismo de sempre) -- mas a decisão em si RODA
  // aqui, de verdade, no pool de tempo crítico, então esta trilha é o lugar
  // certo de mostrá-la (ver o rename desta trilha p/ "Thread de Tempo
  // Crítico" -- é exatamente por isso). Fase 0: sequência completa (a
  // mesma, byte a byte, que existia numa trilha "Decisão (UBF)" à parte,
  // antes de dobrar aqui). Fases 1-3: controller() roda nelas também --
  // sem filtro de fase -- mas o RESULTADO é idêntico (nada no ciclo lê a
  // fase), então repetir os 7 passos integrais 4x por quadro seria só
  // ruído -- resumido, visível com "Ociosos".
  if (node.id === "agent") {
    if (ph.n !== 0) {
      push({ kind: "visit", node: "agent", counters: ctr, dt, runs: false, idle: true, owner: null,
        src: "AgentTC::updateTC", hl: [0, 3],
        title: `AgentTC::updateTC(dt) — fase ${ph.n}, mesma decisão da fase 0`,
        body: "Sem filtro de fase: controller() roda de novo, mas nada no ciclo lê a fase atual -- é a MESMA decisão da fase 0 deste quadro, repetida." });
      return;
    }
    push({ kind: "decision", node: "agent", counters: ctr, dt, src: "AgentTC::updateTC", hl: [0, 3],
      title: "AgentTC::updateTC(dt) → controller(dt)",
      body: "AgentTC (não Agent) roda no pool de TEMPO CRÍTICO, junto do resto do frame -- a mesma escolha da produção (FlightAgentTC): nenhum relógio de fundo, fora de sincronia, decide por fora.",
      warn: "Sem filtro de fase: controller() é chamado em TODA fase (0..3), 4x por quadro -- as 3 seguintes repetem a MESMA decisão (ligue \"Ociosos\" pra ver). Uma subclasse concreta que só queira decidir uma vez por quadro tem que filtrar ela mesma (ex.: if (phase==3))." });

    stack.push({ label: "Agent::controller", node: "agent" });

    stack.push({ label: "AbstractState::updateState", node: "ubfstate" });
    push({ kind: "decision", node: "ubfstate", counters: ctr, dt, src: "AbstractState::updateState", hl: [0, 27],
      title: "state->updateState(actor) — percepção",
      body: "O corpo em AbstractState já é real: por si só só recursa nos filhos (é um estado composto, como o grafo de components também é). Quem de fato LÊ algo do ator é uma subclasse própria -- no tutorial oficial do MIXR (mainUbf1, ver MIXR-PATTERN-CONTEXT.md §10.1), PlaneState." });
    stack.pop();

    stack.push({ label: "Arbiter::genAction", node: "ubfarb" });
    push({ kind: "decision", node: "ubfarb", counters: ctr, dt, src: "Arbiter::genAction", hl: [0, 31],
      title: "Arbiter::genAction() pergunta a cada behavior",
      body: "UbfArbiter é ele mesmo um AbstractBehavior: percorre a lista behaviors, chama genAction() em cada um e junta as respostas num actionSet -- antes de decidir o que fazer com elas." });

    [["ubfbeh1", "1", 10], ["ubfbeh2", "2", 6], ["ubfbeh3", "3", 3]].forEach(([id, n, vote]) => {
      stack.push({ label: "AbstractBehavior::genAction", node: id });
      push({ kind: "decision", node: id, counters: ctr, dt, src: "Arbiter::genAction", hl: [6, 20],
        title: `behavior ${n} recomenda uma ação — voto ${vote}`,
        body: "Exemplo didático (não é um behavior deste repositório)." });
      stack.pop();
    });
    stack.pop(); // Arbiter::genAction

    stack.push({ label: "Arbiter::genComplexAction", node: "ubfarb" });
    push({ kind: "decision", node: "ubfbeh1", counters: ctr, dt, src: "Arbiter::genComplexAction", hl: [9, 18],
      title: "genComplexAction(): 10 > 6 > 3 — vence o voto 10",
      body: "Critério estrito '>' (Arbiter.cpp): maior voto vence a ação INTEIRA. Empate favorece quem foi listado primeiro na lista behaviors (maxVote==0 também cai nesse ramo).",
      warn: "Isso é o Arbiter PADRÃO. Uma subclasse pode sobrescrever genComplexAction() para compor CAMPO A CAMPO em vez de escolher a ação inteira -- é o que PriorityArbiter faz no tutorial oficial do MIXR (mainUbf1): pitch/roll/heading/throttle, cada um do behavior de maior voto NAQUELE campo. É isso que distingue UBF de uma árvore/máquina de estados onde um único ramo vence de uma vez (MIXR-PATTERN-CONTEXT.md §10.1)." });
    stack.pop();

    push({ kind: "decision", node: "agent", counters: ctr, dt, src: "Agent::controller", hl: [11, 14],
      title: "action->execute(actor); action->unref();",
      body: "A ação é efêmera: nasce em genAction(), atua em execute(actor) e é liberada no mesmo ciclo -- nunca fica guardada. AbstractAction::execute() é puro virtual; qual efeito concreto ela produz é da subclasse (fora de escopo aqui)." });

    stack.pop(); // Agent::controller
    return;
  }

  const runs = node.phases.includes(ph.n);
  const owner = runs ? phaseOwner(node.cls, ph.n) : null;
  const isPlayer = !!node.player;

  push({
    kind: "visit", node: node.id, counters: ctr, dt, runs, idle: !runs, owner,
    src: srcFor(node, ph.n), hl: runs && SNIPPETS[`${owner}::${PHASES[ph.n].m}`] ? [0, 3] : isPlayer ? [16, 22] : node.disp ? [21, 38] : [2, 16],
    title: runs
      ? `${node.cls}::${PHASES[ph.n].m}(dt4)${owner !== node.cls ? ` — herdado de ${owner}` : ""}`
      : `${node.cls}::updateTC(dt) — sem trabalho na fase ${ph.n}`,
    body: runs
      ? `dt recebido = ${fmt(dt)}. ${node.disp ? `System::updateTC() recompõe dt4 = dt*4 = ${fmt(dt * 4)} e despacha` : "O Player recompõe dt4 e despacha"} para ${owner}::${PHASES[ph.n].m}(). A divisão na descida e a multiplicação na chegada se cancelam: o método roda uma vez por quadro, com o dt integral.`
      : node.disp
        ? `dt recebido = ${fmt(dt)}. Nenhuma classe da cadeia ${chainOf(node.cls).slice(0, 3).join(" < ")} implementa ${PHASES[ph.n].m}(); System::${PHASES[ph.n].m}() é um corpo vazio. O nó é visitado e repassa dt aos filhos.`
        : `${node.cls} não deriva de System: não há switch(phase) na cadeia. Component::updateTC() apenas percorre os filhos com tcFrame(dt).`,
    warn: node.note && ph.n === 0 ? node.note : null,
  });

  if (node.id === "ac" && ph.n === 0 && fr === 0)
    push({ kind: "note", node: "ac", counters: ctr, dt, src: "Player::updateSystemPointers", hl: [2, 14],
      title: "loadSysPtrs — a varredura por tipo",
      body: "Os DEZ ponteiros de sistema são resolvidos por findByType() — a mesma razão de só poder haver UM de cada tipo primário: um segundo Navigation irmão seria invisível. É por isso que a ordem dos filhos no EDL é irrelevante." });

  if (node.id === "a1" && ph.n === 1)
    push({ kind: "rf", node: "a1", to: "ac2", counters: ctr, dt, src: "Radar::transmit", hl: [10, 24],
      title: "Radar::transmit() → Antenna::rfTransmit() → alvo->event(RF_EMISSION)",
      body: "Note que quem transmite é o Tws (deriva de Radar), não a antena: Antenna não implementa transmit(). O sensor monta a Emission e chama rfTransmit() da antena nomeada em antennaName. A emissão chega ao alvo como evento.",
      warn: "Aresta invisível a qualquer análise estática: nenhum call graph liga Antenna a Aircraft." });

  if (node.id === "ttm" && ph.n === 3)
    push({ kind: "name", node: "ttm", from: "tws", counters: ctr, dt, src: "TrackManager::process", hl: [0, 4],
      title: "TrackManager::process() — herdado por AirTrkMgr",
      body: "AirTrkMgr não sobrescreve process(). O ponteiro do gerente veio do slot trackManagerName, resolvido por string dentro do Tws — e do Stt: os DOIS sensores apontam para o MESMO twsTrkMgr.",
      warn: "Erro de digitação em trackManagerName não produz erro de carga: o sistema simplesmente não faz nada." });

  if (node.id === "sto" && ph.n === 3 && fr === LAUNCH_FRAME) {
    push({ kind: "release", node: "sto", to: "flyout", counters: ctr, dt, src: "Stores::releaseWeapon", hl: [0, 14],
      title: "Stores::releaseWeapon() — o míssil entra na simulação",
      body: "wpn->release() muda o modo para ACTIVE e chama addNewPlayer(). Do próximo quadro em diante o míssil (a estação 2, um Aam) é percorrido nas quatro fases como qualquer outro player.",
      warn: "A árvore de contenção mudou em execução. Nenhum arquivo de configuração descreve este nó." });
    doRelease();
  }

  (node.children || []).forEach((c) => {
    stack.push({ label: `${c.cls}::updateTC`, node: c.id });
    walk(c, ph, dt, ctr, push, stack, fr, doRelease);
    stack.pop();
  });
}

function traceBackground() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length, counters: { cycle: 0, frame: "—", phase: null }, dt: 0.05 });
  p({ kind: "bg", node: "station", src: "Station::updateData", hl: [1, 14], stack: [{ label: "Station::updateData", node: "station" }],
    title: "Station::updateData(dt) — a 20 Hz, não a 50",
    body: "A thread de fundo tem taxa própria (bgRate) e nenhuma relação com as fases. Com bgRate: 0 tudo isto roda sincronamente na thread que chamou updateData().",
    warn: "Não existe dataFrame(): este caminho não passa por invólucro nem é medido." });
  p({ kind: "bg", node: "nav", src: "Navigation::updateData", hl: [0, 6],
    stack: [{ label: "Station::updateData", node: "station" }, { label: "Player::updateData", node: "ac" }, { label: "Navigation::updateData", node: "nav" }],
    title: "Navigation::updateData() — e também Navigation::process()",
    body: "A navegação é dos poucos subsistemas que trabalham nos dois caminhos: updateData() no fundo e process() na fase 3. Os dados de pilotagem que o Autopilot lê podem, portanto, ser de outro quadro." });
  p({ kind: "bg", node: "a1", src: "Gimbal::processPlayersOfInterest", hl: [0, 9],
    stack: [{ label: "Station::updateData", node: "station" }, { label: "RfSystem::updateData", node: "tws" }, { label: "Gimbal::processPlayersOfInterest", node: "a1" }],
    title: "Gimbal::processPlayersOfInterest() monta o Tdb",
    body: "Filtrar centenas de players por alcance, ângulo, tipo e terreno é caro e tolera defasagem, então sai do caminho crítico. Medir a geometria daqueles alvos é barato e precisa ser atual, então fica na fase 1." });
  return st;
}

function traceReset() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length, counters: { cycle: 0, frame: "—", phase: null }, dt: 0 });
  p({ kind: "reset", node: "station", src: "Station::updateTC", hl: [42, 51], stack: [{ label: "Station::updateTC", node: "station" }],
    title: "startupResetTimer expira → event(RESET_EVENT)",
    body: "O reset não é chamada de método: é um evento que desce a árvore inteira. Sem startupResetTimer no EDL, a simulação carrega sem erro e não faz nada." });
  p({ kind: "vanish", node: "flyout", src: "Component::processComponents", hl: [0, 20],
    stack: [{ label: "Station::reset", node: "station" }, { label: "Simulation::reset", node: "sim" }],
    title: "players é reconstruída a partir de origPlayers",
    body: "origPlayers vem do slot players: do EDL e nunca é modificada. O míssil lançado nunca esteve lá — desaparece sem que exista uma linha de código para removê-lo.",
    warn: "Entidades vindas da rede e players destruídos seguem a mesma regra." });
  p({ kind: "reset", node: "ac", src: "Component::processComponents", hl: [20, 40],
    stack: [{ label: "Station::reset", node: "station" }, { label: "Simulation::reset", node: "sim" }, { label: "Player::reset", node: "ac" }],
    title: "A porta de tipo de processComponents()",
    body: "Um filho que não seja Component é descartado em silêncio, e nem o parser nem isValid() acusam. O sintoma aparece depois, como um subsistema que não faz nada." });
  return st;
}

// "Quadro" virou "Thread de Tempo Crítico": a decisão UBF (AgentTC) roda
// nesse MESMO pool -- ela deixou de ser uma trilha à parte ("Decisão
// (UBF)") e passou a aparecer aqui, na fase 0, dentro de walk() (ver o
// bloco "if (node.id === 'agent')" acima). Uma trilha só, sem duplicar a
// mesma sequência em dois lugares.
const TRACES = { tc: { label: "Thread de Tempo Crítico", build: traceFrames }, bg: { label: "Thread de fundo", build: traceBackground }, reset: { label: "Reset", build: traceReset } };

/* ============================== layout ============================== */

// NW subiu de 142 pra 208: o cenário completo tem nomes de classe de até 20
// caracteres (ActionCamouflageType, ActionWeaponRelease) que truncavam em
// elipse mesmo depois do alargamento anterior -- "truncar texto atrapalha"
// (pedido explícito).
//
// COL precisou subir de 280 pra 350 numa passada POSTERIOR -- a conta de
// "NW + ~70px de goteira" (a que gerou 280) não batia: o label "via:" (o
// rótulo do slot EDL na própria aresta, ex. "components:") não usa a
// goteira INTEIRA -- `mid = p.x + NW + 16` já consome 16px pro próprio
// cotovelo da linha antes do texto começar, e o cálculo de `labelW` ainda
// desconta mais 4+6px de margem -- sobravam só 280-208-26=46px reais pro
// texto, não os 70px assumidos. "components:" (11 caracteres) e
// principalmente "irShapeSignature:" (17, o mais longo `via:` deste
// arquivo) não cabiam a 8.5px mono nesse espaço e saíam com elipse (medido
// rodando: cortava em "compone..."). COL=350 deixa ~116px reais pro texto
// (350-208-26), com folga sobre os ~17 caracteres do pior caso.
const NW = 208, NH = 34, ROW = 42, COL = 350;
// Espaçamento equivalente para a árvore VERTICAL (raiz em cima, irmãos lado
// a lado): ROW_V é o passo de profundidade (substitui COL), grande o
// bastante pra caber a caixa (NH) + o cotovelo da aresta + os rótulos
// "via:"/"dt" que antes só cabiam na goteira horizontal. COL_V é o passo
// entre irmãos (substitui ROW), tem de caber a LARGURA da caixa (NW) —
// bem maior que ROW=42, porque agora os irmãos se espalham no eixo que
// antes era só de empilhamento fino.
const ROW_V = 100, COL_V = NW + 46;
// Teto alto de propósito: em zoom baixo os cartões mais lotados (pips de
// visita, badges de thread) ficam ilegíveis -- 10x dá pra ler qualquer
// cartão de perto, inclusive num viewport estreito.
const ZOOM_MIN = 0.4, ZOOM_MAX = 10;

// orientation: "h" (raiz à esquerda, profundidade cresce pra direita -- o
// padrão) ou "v" (raiz em cima, profundidade cresce pra baixo, irmãos lado
// a lado). Mesma árvore, mesmo algoritmo -- só troca qual eixo é
// "profundidade" (a even ROW_V/COL passo por nível) e qual é "espalhamento
// dos irmãos" (o COL_V/ROW passo por folha, na ordem de visita DFS).
function layout(root, orientation) {
  const v = orientation === "v";
  const nodes = []; let i = 0;
  (function place(n, depth) {
    const kids = n.children || [];
    const a = depth * (v ? ROW_V : COL);
    if (!kids.length) {
      const b = i * (v ? COL_V : ROW);
      nodes.push({ ...n, x: v ? b : a, y: v ? a : b, depth });
      i += 1;
    } else {
      kids.forEach((k) => place(k, depth + 1));
      const f = nodes.find((m) => m.id === kids[0].id);
      const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
      const b = v ? (f.x + l.x) / 2 : (f.y + l.y) / 2;
      nodes.push({ ...n, x: v ? b : a, y: v ? a : b, depth });
    }
  })(root, 0);
  return nodes;
}

const THREAD_COLOR = { tc: "var(--ink)", fundo: "var(--bgc)", rede: "var(--ok)", "tc+fundo": "var(--bgc)", "—": "var(--rule)" };

/* ============================ pan/zoom (SVG) ========================= *
 * Extraído depois de confirmado idêntico entre Exec e FlightDecision (a  *
 * mesma lógica -- zoom por roda com fator 1.12 clampado em [ZOOM_MIN,    *
 * ZOOM_MAX], arrasto com deadzone de clique antes de capturar o pointer  *
 * -- só divergia em COMENTÁRIO, não em comportamento). Terceiro          *
 * consumidor: StructDiagram (aba Estrutura). Devolve exatamente os       *
 * mesmos nomes que os três call-sites já usam como variáveis locais.    */
function usePanZoom(initial = { k: 1, x: 0, y: 0 }) {
  const [view, setView] = useState(initial);
  const drag = useRef(null);
  const svgRef = useRef(null);
  // Zoom pela roda do mouse tem de ser um addEventListener NATIVO,
  // {passive:false} -- nao um onWheel de JSX. Desde o React 17, wheel/
  // touchstart/touchmove viram listener PASSIVO por padrao na raiz da
  // árvore React (otimização de scroll, documentada no próprio React —
  // issue #14856); dentro de um listener passivo, e.preventDefault() é
  // NO-OP silencioso (sem erro, sem aviso em produção) -- o navegador
  // rola a JANELA por baixo ao MESMO TEMPO que o zoom acontece. É esse o
  // motivo de "rolar a página" e "dar zoom na árvore" pareciam os dois
  // efeitos do mesmo gesto de scroll: o segundo sempre funcionou (o
  // estado React mudava normalmente), o primeiro nunca foi de fato
  // bloqueado. Um listener anexado via addEventListener direto no nó DOM
  // do <svg> (fora do sistema de eventos sintético do React) escapa desse
  // comportamento passivo e bloqueia o scroll da janela de verdade.
  useEffect(() => {
    const el = svgRef.current;
    if (!el) return undefined;
    const handleWheel = (e) => {
      e.preventDefault();
      const f = e.deltaY < 0 ? 1.12 : 1 / 1.12;
      setView((v) => ({ ...v, k: Math.max(ZOOM_MIN, Math.min(ZOOM_MAX, v.k * f)) }));
    };
    el.addEventListener("wheel", handleWheel, { passive: false });
    return () => el.removeEventListener("wheel", handleWheel);
  }, []);
  // Captura o pointer só quando o arrasto vira REAL (deslocamento >
  // DRAG_CLICK_PX), não no pointerdown cru. Descoberto rodando um clique de
  // verdade (mousedown+mouseup no MESMO lugar) contra um listener de
  // depuração: capturar cedo demais faz o BROWSER decidir, já no
  // pointerdown, retargetar o "click" resultante pro próprio <svg> capturador
  // -- e essa decisão NÃO muda mesmo soltando a captura depois, no
  // pointerup/onUp (confirmado: hasPointerCapture ia de true a false antes do
  // "click" dispersar, e o "click" ainda saía com target=svg). O onClick de
  // um <g class="mx-node"> por baixo do <svg> nunca disparava com mouse de
  // verdade -- só com dispatchEvent sintético (que não passa por pointer
  // capture nenhum), o que escondeu o bug de um teste anterior. Adiar a
  // captura pro primeiro pointermove que de fato deslocar evita o problema na
  // raiz: um clique sem deslocamento nunca chega a capturar o pointer, então
  // o "click" segue o alvo normal (hit-test no elemento sob o cursor).
  const DRAG_CLICK_PX = 4;
  // O <svg> tem viewBox proprio (unidades logicas de layout, ex. W/H de
  // layout()) e e' exibido num retangulo CSS de OUTRO tamanho --
  // preserveAspectRatio="xMidYMid meet" encolhe/estica o conteudo pelo
  // MENOR fator que caiba nos dois eixos (letterbox no outro). Sem
  // converter o delta de PIXEL DE TELA (clientX/Y) para unidade de viewBox
  // por esse mesmo fator, arrastar "desliza" -- a arvore anda mais devagar
  // (ou mais rapido) que o mouse, proporcional a quanto o viewBox difere do
  // tamanho renderizado. Um so fator para os dois eixos (nunca X e Y
  // separados) e' o que mantém a diagonal reta quando os dois eixos tem
  // proporcoes diferentes -- "meet" so usa UMA escala uniforme.
  const dragFactor = (svg) => {
    const vb = svg && svg.viewBox && svg.viewBox.baseVal;
    const rect = svg && svg.getBoundingClientRect ? svg.getBoundingClientRect() : null;
    if (!vb || !rect || !rect.width || !rect.height) return 1;
    const scale = Math.min(rect.width / vb.width, rect.height / vb.height);
    return scale > 0 ? 1 / scale : 1;
  };
  const onDown = (e) => { drag.current = { x: e.clientX, y: e.clientY, vx: view.x, vy: view.y, captured: false, pointerId: e.pointerId, el: e.currentTarget, factor: dragFactor(e.currentTarget) }; };
  // Captura `d`/cx/cy num LOCAL antes de agendar o setView -- a arvore agora
  // e grande o bastante pra arrastar gerar VARIOS pointermove por frame, e o
  // callback de updater do setState so roda depois (as vezes ja no proximo
  // lote). Ler `drag.current` DENTRO do updater (como era antes) reagia ao
  // valor NA HORA em que o updater executa, nao em que o evento chegou -- um
  // pointerup entre um evento e o outro zera drag.current pra null primeiro,
  // e o updater de um pointermove ainda na fila quebrava com "Cannot read
  // properties of null (reading 'vx')", derrubando a arvore React inteira.
  // Medido travando o app com um arrasto real (nao so no teste automatizado).
  const onMove = (e) => {
    const d = drag.current;
    if (!d) return;
    const cx = e.clientX, cy = e.clientY;
    if (!d.captured) {
      if (Math.hypot(cx - d.x, cy - d.y) < DRAG_CLICK_PX) return; // ainda pode ser so um clique -- nao mexe em nada ainda
      d.captured = true;
      d.el.setPointerCapture(d.pointerId);
    }
    setView((v) => ({ ...v, x: d.vx + (cx - d.x) * d.factor, y: d.vy + (cy - d.y) * d.factor }));
  };
  const onUp = (e) => {
    if (e && e.currentTarget.hasPointerCapture && e.currentTarget.hasPointerCapture(e.pointerId)) {
      e.currentTarget.releasePointerCapture(e.pointerId);
    }
    drag.current = null;
  };
  return { view, setView, svgRef, onDown, onMove, onUp, drag };
}

/* =============================== CSS ================================ */

const CSS = `
/* Reset da margem padrão do user-agent (~8px em <body>) -- sem isto sobra   *
 * uma faixa da cor de fundo do PRÓPRIO NAVEGADOR (não de --paper) ao redor  *
 * da página inteira, visível principalmente no tema escuro. */
html, body { margin:0; padding:0; }
.mx { --paper:#E6E9E3; --panel:#DCE0D9; --ink:#16232E; --muted:#6E7A76;
  --rule:#C6CDC3; --hot:#B4661E; --rf:#8C2F3D; --bgc:#3D6C8C; --ok:#4A6B4F;
  --new:#7A5B9B; --code:#1B2730; --codeink:#CFD8CE;
  --py-accent:#3E7C4F; --onnx-accent:#6A4C93;
  --active-bg:#F0EAE2; --never-bg:#E2E5DF; --running-fg:#F0E2D4;
  --sub-muted:#8F9A93; --band-bg:#E0E4DC; --graph-bg:#EAEDE7;
  --phase-inherited:#8B9691; --phase-now-bg:#F5E7D8; --phase-has-running-bg:#E0C9AF;
  --phase-stroke-running:#D9B48C;
  --seg-phase-0:#9AA79F; --seg-phase-1:#8FA0A8; --seg-phase-2:#A8A08F; --seg-phase-3:#9E93A8;
  --edl-bg:#F0F2EC; --edl-muted:#A3ADA4; --edl-hl:#E2DBCE;
  --code-muted:#5E7280; --code-hl:#2E4250;
  --mono: ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace;
  --sans: 'Inter',system-ui,-apple-system,sans-serif;
  background:var(--paper); color:var(--ink); font-family:var(--sans);
  font-size:13.5px; line-height:1.5; min-height:100vh;
  /* body/html não têm cor de fundo própria (o template HTML de compile.js não
     define uma) -- sem min-height aqui, um conteúdo mais baixo que a janela
     deixa a sobra transparente, mostrando o branco padrão do navegador por
     baixo. Inofensivo no claro (quase a mesma cor de --paper), mas MUITO
     visível no escuro -- medido: tira de listra branca embaixo da barra de
     transporte (fixed, não conta pra altura do fluxo normal). */
  /* Paleta clara é a padrão (era fixa antes -- agora "Modo escuro", no canto
     superior direito, alterna pra [data-theme="dark"] abaixo; nenhuma das
     duas depende de @media prefers-color-scheme, então o SO do usuário
     nunca decide por conta própria -- só o toggle). */
  color-scheme: light; }
/* Paleta escura: mesmas 26 variáveis, redefinidas -- todo o resto do CSS (e
   os poucos fill=/stroke= que precisam variar por tema, no JS mais abaixo)
   só lê var(--x), nunca um hex cru, então trocar aqui basta. */
.mx[data-theme="dark"] { --paper:#181C19; --panel:#232722; --ink:#E7EAE4; --muted:#8B968E;
  --rule:#3A413B; --hot:#D98A4A; --rf:#E0808F; --bgc:#7FB3D9; --ok:#7FBE8B;
  --new:#B79BDB; --code:#12171B; --codeink:#C7D0C6;
  --py-accent:#6FCB86; --onnx-accent:#B08FE0;
  --active-bg:#2C2F27; --never-bg:#1F231E; --running-fg:#2A1A0A;
  --sub-muted:#77827A; --band-bg:#20251F; --graph-bg:#1D211C;
  --phase-inherited:#5B655D; --phase-now-bg:#3A2A16; --phase-has-running-bg:#4A3620;
  --phase-stroke-running:#6B4E28;
  --seg-phase-0:#4B534C; --seg-phase-1:#445158; --seg-phase-2:#565040; --seg-phase-3:#524A5C;
  --edl-bg:#20251F; --edl-muted:#647169; --edl-hl:#39331F;
  --code-muted:#7C8A93; --code-hl:#293C49;
  color-scheme: dark; }
.mx *:focus-visible { outline:2px solid var(--hot); outline-offset:2px; }
.mx-bar { position:sticky; top:0; z-index:5; background:var(--paper);
  border-bottom:1px solid var(--rule); padding:10px 18px 8px;
  display:flex; justify-content:space-between; align-items:center; gap:14px; flex-wrap:wrap; }
.mx-h1 { font-size:16px; font-weight:600; margin:0; letter-spacing:-0.01em; }
.mx-sub { font-size:12px; color:var(--muted); margin:2px 0 0; }
.mx-tabs { display:flex; gap:3px; flex-wrap:wrap; }
.mx-tab { font:inherit; font-size:12.5px; padding:5px 12px; cursor:pointer;
  border:1px solid var(--rule); background:transparent; color:var(--muted); border-radius:2px; }
.mx-tab:hover { border-color:var(--ink); color:var(--ink); }
.mx-tab[data-on="1"] { background:var(--ink); border-color:var(--ink); color:var(--paper); }
.mx-body { padding:12px 18px 86px; }
.mx-graph { position:relative; border:1px solid var(--rule); border-radius:2px;
  background:var(--graph-bg); margin-bottom:12px; overflow:hidden; }
.mx-svgwrap { height:clamp(300px, 46vh, 780px); width:100%; transition:height 200ms ease-out; }
/* Com o painel de detalhe oculto (botao "detalhe"), o grafo cresce pra usar   *
 * a altura que o painel deixou de ocupar -- e o proposito do toggle. */
.mx-svgwrap[data-expanded="1"] { height:clamp(500px, 82vh, 1500px); }
.mx-svgwrap svg { width:100%; height:100%; display:block; touch-action:none; cursor:grab; }
.mx-svgwrap svg:active { cursor:grabbing; }
.mx-zoom { position:absolute; top:8px; right:8px; display:flex; gap:3px; z-index:2; }
.mx-zbtn { font:inherit; font-size:12px; width:26px; height:26px; padding:0; cursor:pointer;
  border:1px solid var(--rule); background:var(--paper); color:var(--ink); border-radius:2px; }
.mx-zbtn:hover { border-color:var(--ink); }
.mx-zbtn[data-w="1"] { width:auto; padding:0 8px; }
.mx-zoomslider { display:flex; align-items:center; gap:7px; height:26px; padding:0 9px;
  background:var(--paper); border:1px solid var(--rule); border-radius:2px; }
.mx-zoomslider input[type="range"] { width:88px; accent-color:var(--ink); cursor:pointer; }
.mx-zoomslider span { font-size:11px; color:var(--muted); min-width:36px; text-align:right; }

/* --- popup flutuante do clique num no do grafo (fabrica/registro/slots) -- *
 * position:absolute relativo a .mx-graph (que ja e position:relative);     *
 * left/top vem JA CLAMPADOS em JS pra caber dentro de .mx-graph, que corta *
 * overflow -- por isso nenhum tamanho aqui e "auto", os dois lados         *
 * concordam com a mesma largura/altura estimada. z-index acima de         *
 * .mx-zoom (2), acima tambem dos nos do proprio svg. */
.mx-nodepopup { position:absolute; z-index:6; background:var(--paper); border:1px solid var(--ink);
  border-radius:3px; padding:9px 11px 10px; box-shadow:0 3px 10px rgba(0,0,0,0.22);
  animation:mx-fadein 140ms ease-out; }
.mx-nodepopup-x { font:inherit; font-size:14px; line-height:1; padding:0 2px; cursor:pointer;
  border:none; background:none; color:var(--muted); }
.mx-nodepopup-x:hover { color:var(--ink); }
.mx-nodepopup-link { display:block; width:100%; margin-top:8px; font:inherit; font-size:11px;
  padding:5px 6px; cursor:pointer; border:1px solid var(--rule); border-radius:2px;
  background:var(--panel); color:var(--ink); text-align:left; }
.mx-nodepopup-link:hover { border-color:var(--ink); }

.mx-pane { min-width:0; display:flex; flex-direction:column; }
.mx-card { background:var(--panel); padding:11px 13px; border-radius:2px; }
.mx-lbl { font-size:11.5px; color:var(--muted); margin-bottom:5px; display:flex;
  justify-content:space-between; gap:8px; align-items:baseline; }
.mx-mono { font-family:var(--mono); }

/* --- detalhe com abas: cada aba ocupa a largura toda, ninguem disputa espaco --- */
.mx-dtabs { display:flex; gap:3px; flex-wrap:wrap; margin-bottom:10px; }
.mx-dtab { font:inherit; font-size:12.5px; padding:6px 13px; cursor:pointer;
  border:1px solid var(--rule); background:var(--paper); color:var(--muted); border-radius:2px 2px 0 0;
  border-bottom:2px solid transparent; display:flex; align-items:center; gap:6px; }
.mx-dtab:hover { color:var(--ink); }
.mx-dtab[data-on="1"] { color:var(--ink); border-bottom-color:var(--hot); background:var(--panel); font-weight:600; }
.mx-dtab-dot { width:6px; height:6px; border-radius:50%; background:var(--hot); flex-shrink:0; }
.mx-detailbody { animation:mx-fadein 200ms ease-out; }

/* --- codigo/EDL: SEM caixa de rolagem -- a janela de linhas ja vem cortada do   *
 * JS (windowLines()), entao a caixa so cresce ate o que de fato existe.        */
.mx-code { background:var(--code); color:var(--codeink); font-family:var(--mono);
  font-size:11.5px; line-height:18px; padding:10px 0; border-radius:2px; }
.mx-edl { background:var(--edl-bg); border:1px solid var(--rule); font-family:var(--mono);
  font-size:11.5px; line-height:18px; padding:10px 0; border-radius:2px; }
.mx-codecut { text-align:center; font-size:10.5px; color:var(--code-muted); padding:3px 0; letter-spacing:0.06em; }
.mx-edl .mx-codecut { color:var(--edl-muted); }
.mx-cl { display:flex; transition:background 200ms; }
.mx-cl[data-on="1"] { background:var(--code-hl); }
.mx-edl .mx-cl[data-on="1"] { background:var(--edl-hl); }
.mx-num { width:42px; text-align:right; padding-right:9px; color:var(--code-muted); flex-shrink:0; }
.mx-edl .mx-num { width:30px; color:var(--edl-muted); }
.mx-src { white-space:pre; border-left:2px solid transparent; padding-left:8px; }
.mx-cl[data-on="1"] .mx-src { border-left-color:var(--hot); }
.mx-node { cursor:pointer; }
.mx-node rect { transition:fill 130ms, stroke 130ms, opacity 220ms, stroke-width 130ms; }
.mx-node[data-pop="1"] { animation:mx-popin 320ms cubic-bezier(.2,.9,.3,1.3); }

/* --- rotulo do no: foreignObject com ellipsis, NUNCA passa da caixa (era        *
 * <text> puro, que nao quebra nem corta -- em nomes longos o texto vazava por   *
 * cima do proximo elemento; era exatamente o "texto sobreposto" a corrigir). --- */
.mx-fo { pointer-events:none; }
.mx-fo-row { width:100%; height:100%; overflow:hidden; display:flex; align-items:center; }
.mx-fo-cls { font-family:var(--mono); font-weight:600; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
.mx-fo-sub { font-family:var(--mono); white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }

/* --- animacoes de fluxo: aresta "andando" (marcha de formigas) para o caminho   *
 * ativo, halo pulsando no no em execucao, tudo so com CSS/SMIL -- sem lib nova   *
 * (mantem docs/manual/index.html sem nenhuma requisicao de rede pra abrir). --- */
@keyframes mx-dash { to { stroke-dashoffset:-20; } }
@keyframes mx-dashfast { to { stroke-dashoffset:-24; } }
@keyframes mx-halo { 0% { transform:scale(0.55); opacity:0.65; } 100% { transform:scale(2.1); opacity:0; } }
@keyframes mx-popin { from { transform:scale(0.82); opacity:0.3; } to { transform:scale(1); opacity:1; } }
@keyframes mx-fadein { from { opacity:0; transform:translateY(3px); } to { opacity:1; transform:translateY(0); } }
@keyframes mx-glow { 0%,100% { opacity:1; } 50% { opacity:0.45; } }
.mx-edge-onpath { stroke-dasharray:7 5; animation:mx-dash 900ms linear infinite; }
.mx-edge-current { stroke-dasharray:5 4; animation:mx-dashfast 480ms linear infinite; }
.mx-edge-evt { animation:mx-dashfast 420ms linear infinite; }
.mx-halo { fill:none; pointer-events:none; transform-box:fill-box; transform-origin:center;
  animation:mx-halo 1200ms ease-out infinite; }
.mx-phase-now { animation:mx-glow 1000ms ease-in-out infinite; }
.mx-btn { font:inherit; font-size:12.5px; padding:5px 12px; border:1px solid var(--ink);
  background:transparent; color:var(--ink); border-radius:2px; cursor:pointer; }
.mx-btn:hover { background:var(--rule); }
.mx-btn[data-primary="1"] { background:var(--ink); color:var(--paper); }
.mx-transport { position:fixed; left:0; right:0; bottom:0; background:var(--paper);
  border-top:1px solid var(--rule); padding:8px 18px; display:flex; align-items:center;
  gap:8px; flex-wrap:wrap; z-index:6; }
.mx-tl { display:flex; height:24px; gap:1px; flex:1; min-width:170px; cursor:pointer; align-items:flex-end; }
.mx-seg { flex:1; min-width:1px; border-radius:1px 1px 0 0; transition:height 160ms ease-out, opacity 160ms; }
.mx-input { font:inherit; font-size:12.5px; padding:5px 8px; background:var(--paper);
  border:1px solid var(--rule); color:var(--ink); border-radius:2px; }
.mx-chip { display:inline-block; font-family:var(--mono); font-size:11px; padding:1px 6px;
  border-radius:2px; border:1px solid var(--rule); color:var(--muted); }
.mx-cls { display:inline-flex; align-items:center; gap:5px; padding:2px 7px; border-radius:2px;
  font-family:var(--mono); font-size:11.5px; border:1px solid transparent; cursor:pointer; }
.mx-cls:hover { background:var(--panel); }
.mx-cls[data-scn="1"] { border-color:var(--ink); background:var(--panel); }
.mx-cls[data-div="1"] { border-style:dashed; border-color:var(--rf); }
.mx-cls[data-reg="0"] { opacity:0.55; }
.mx-wrap { display:flex; flex-wrap:wrap; gap:4px; }
.mx-mod { border-top:1px solid var(--rule); padding-top:12px; margin-top:16px; }
.mx-warn { margin:8px 0 0; padding-left:8px; border-left:2px solid var(--rf);
  font-size:12px; line-height:1.45; color:var(--rf); }
.mx-leg { display:flex; gap:14px; flex-wrap:wrap; font-size:11.5px; color:var(--muted);
  border-top:1px solid var(--rule); padding:6px 10px; background:var(--paper); align-items:center; }
.mx-leg-toggle { font:inherit; font-size:11.5px; font-weight:600; color:var(--ink); background:var(--panel);
  border:1px solid var(--rule); border-radius:2px; padding:2px 8px; cursor:pointer; }
.mx-leg-toggle:hover { border-color:var(--ink); }
.mx-cardleg { padding:10px 14px 12px; background:var(--paper); border-top:1px solid var(--rule);
  animation:mx-fadein 200ms ease-out; }
.mx-stats { display:flex; gap:16px; flex-wrap:wrap; font-size:12px; color:var(--muted);
  margin-bottom:10px; }
.mx-stats b { color:var(--ink); font-family:var(--mono); font-weight:600; }
.mx-slot { display:flex; gap:8px; font-family:var(--mono); font-size:11px; padding:1px 0; }
.mx-slot span:first-child { min-width:132px; color:var(--ink); }
.mx-slot span:last-child { color:var(--muted); }
/* Grade de colunas em vez de lista alta com scroll: uma cadeia com 40+ slots
 * ainda cabe sem caixa de rolagem, so ficando mais larga que alta. */
.mx-slotgrid { columns:230px; column-gap:18px; }
.mx-slotgrid .mx-slot { break-inside:avoid; }

@media (prefers-reduced-motion:reduce) { .mx * { transition:none !important; animation:none !important; } }
`;

/* =============================== app ================================ */

export default function App() {
  const [mode, setMode] = useState("exec");
  const [focus, setFocus] = useState(null);
  // Espelha `focus` (Catálogo -> Execução), na direção oposta: o popup de nó
  // do grafo (Exec) pede "ver esta classe no Catálogo" e este estado carrega
  // QUAL classe até lá -- consumido (voltando a null) pelo próprio Catalog,
  // mesmo padrão do useEffect de `focus` dentro de Exec.
  const [catalogFocus, setCatalogFocus] = useState(null);
  // Espelha `catalogFocus`, mesma razão -- permite deep-link futuro de
  // outra aba para a aba Estrutura (nenhum ponto do app ainda dispara
  // isto, mas StructDiagram já consome via o mesmo useEffect padrão).
  const [structFocus, setStructFocus] = useState(null);
  // Lido uma vez, no mount -- nunca via @media prefers-color-scheme (o CSS
  // acima é explícito sobre isso: só o toggle decide, não o SO). localStorage
  // é só conveniência entre visitas; falha em silêncio (ex.: file:// em
  // navegadores que bloqueiam storage nesse esquema) e cai pro claro.
  const [theme, setTheme] = useState(() => {
    try { return localStorage.getItem("mx-theme") === "dark" ? "dark" : "light"; } catch { return "light"; }
  });
  useEffect(() => { try { localStorage.setItem("mx-theme", theme); } catch { /* sem storage disponível -- ok, só não persiste */ } }, [theme]);
  return (
    <div className="mx" data-theme={theme}>
      <style>{CSS}</style>
      <div className="mx-bar">
        <div>
          <h1 className="mx-h1">MIXR — execução, EDL e {STATS.classes} classes built-in</h1>
          <p className="mx-sub">Extraído da árvore de fontes: {STATS.cpp} arquivos .cpp, {STATS.registered} classes registradas, {STATS.slotsTotal} slots</p>
        </div>
        <div style={{ display: "flex", gap: 8, alignItems: "center" }}>
          <div className="mx-tabs">
            <button className="mx-tab" data-on={mode === "exec" ? 1 : 0} onClick={() => setMode("exec")}>Simulação</button>
            <button className="mx-tab" data-on={mode === "dec" ? 1 : 0} onClick={() => setMode("dec")}>Comportamento</button>
            <button className="mx-tab" data-on={mode === "struct" ? 1 : 0} onClick={() => setMode("struct")}>Estrutura</button>
            <button className="mx-tab" data-on={mode === "cat" ? 1 : 0} onClick={() => setMode("cat")}>Catálogo</button>
          </div>
          <button className="mx-zbtn" data-w="1" onClick={() => setTheme((t) => (t === "light" ? "dark" : "light"))} title="Alternar modo claro/escuro">
            {theme === "light" ? "☾ escuro" : "☀ claro"}
          </button>
        </div>
      </div>
      {mode === "exec" && (
        <Exec focus={focus} setFocus={setFocus}
              onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }} />
      )}
      {mode === "dec" && (
        <FlightDecision onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }} />
      )}
      {mode === "struct" && (
        <StructDiagram onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }}
                       focus={structFocus} setFocus={setStructFocus} />
      )}
      {mode === "cat" && (
        <Catalog onOpen={(c) => { setFocus(c); setMode("exec"); }}
                 openClass={catalogFocus} setOpenClass={setCatalogFocus} />
      )}
    </div>
  );
}

/* ---------------------------- execução ----------------------------- */

function Exec({ focus, setFocus, onOpenCatalog }) {
  const [traceKey, setTraceKey] = useState("tc");
  const [showIdle, setShowIdle] = useState(false);
  const [showNames, setShowNames] = useState(false);
  const [i, setI] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(650);
  const [pinned, setPinned] = useState(null);
  // Popup flutuante no clique de um nó: nome de fábrica (e se diverge do nome
  // da classe C++), registrada ou não, contagem de slots -- fatos JÁ
  // extraídos em MODEL/FACTORIES mas que a aba Execução nunca mostrava (só o
  // Catálogo, e só depois de buscar a classe à mão). `x`/`y` são relativos ao
  // canto de .mx-graph (graphRef), capturados uma vez no clique -- não
  // recalculados a cada pan/zoom (o popup é uma anotação efêmera, não parte
  // do grafo; ela some ao trocar de nó/trilha/orientação, ver os efeitos
  // abaixo).
  const [popup, setPopup] = useState(null);
  const graphRef = useRef(null);
  // Zoom inicial 3.5x (pedido explícito) -- diferente do 1x default das
  // outras duas abas que também usam usePanZoom(). Um só literal, reusado
  // por TODO lugar que "reseta a view pro padrão" (o próprio hook, o efeito
  // de troca de orientação e o botão "ajustar" logo abaixo) -- sem isso, o
  // efeito de troca de orientação (que já existia, resetando pra k:1) e o
  // botão "ajustar" continuariam levando de volta a 1x, mascarando o 3.5x
  // inicial assim que qualquer um dos dois rodasse (medido: o efeito roda
  // também no primeiro render, então o 3.5x nunca chegava a aparecer).
  const EXEC_DEFAULT_VIEW = { k: 3.5, x: 0, y: 0 };
  const { view, setView, svgRef, onDown, onMove, onUp, drag } = usePanZoom(EXEC_DEFAULT_VIEW);
  const [detailTab, setDetailTab] = useState("step");
  // Oculta o painel de detalhe (Passo/Código/EDL/Classe) pra dar mais altura
  // ao grafo -- pedido explícito, depois que o cenário cresceu pra cobrir
  // todos os modelos built-in possíveis e passou a precisar de mais área de
  // desenho pra caber sem espremer.
  const [showDetail, setShowDetail] = useState(true);
  // Legenda visual do cartão (nome/subtítulo/pips/badge/cor de thread) --
  // fechada por padrão, mesmo raciocínio do "detalhe": explicar sem competir
  // por espaço com o grafo o tempo todo.
  const [showCardLegend, setShowCardLegend] = useState(false);
  // "Seguir ramo": zoom/pan passam a acompanhar sozinhos o caminho raiz->nó
  // ativo (o mesmo caminho que as arestas tracejadas já destacam) a cada
  // passo -- inclusive passo a passo, não só durante "Reproduzir". Ver
  // followViewFor() mais abaixo (onde W/H/topMargin/pos já existem) e a
  // transição condicional no <g> do grafo.
  const [autoFollow, setAutoFollow] = useState(false);
  // "v" gira a árvore pra raiz-em-cima/irmãos-lado-a-lado (ver layout()) --
  // reseta o pan/zoom ao trocar (view.x/y/k de uma orientação não fazem
  // sentido nenhum na outra: os nós inteiros mudam de posição).
  const [orientation, setOrientation] = useState("h");
  // Suprime a transição suave ENQUANTO a barra de zoom está sendo arrastada
  // (mesmo motivo do "!drag.current" para o pan: um <input type="range">
  // dispara onChange a cada tique do arrasto -- animar 420ms a cada tique
  // vira elástico. Some ref, não state: não precisa re-render por si só.
  const sliderActive = useRef(false);
  const transportRef = useRef(null);
  // Altura MEDIDA da barra de transporte (fixed, bottom:0) -- nao um numero fixo.
  // Em viewport estreito ela quebra em 2+ linhas (flex-wrap) e uma folga fixa
  // (o antigo `padding-bottom:86px` do .mx-body) passa a ser MENOR que a barra
  // real, escondendo as ultimas linhas do painel de detalhe atras dela (achado
  // rodando em 650px: 2 linhas do aviso do passo 11/78 sumiam por baixo da
  // barra). ResizeObserver cobre tanto resize de janela quanto qualquer mudanca
  // de conteudo da propria barra (rotulo Reproduzir/Pausar, etc.).
  const [transportH, setTransportH] = useState(0);
  useLayoutEffect(() => {
    const el = transportRef.current;
    if (!el) return undefined;
    const update = () => setTransportH(el.getBoundingClientRect().height);
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    window.addEventListener("resize", update);
    return () => { ro.disconnect(); window.removeEventListener("resize", update); };
  }, []);

  useEffect(() => {
    if (!focus) return;
    const n = ALL.find((x) => x.cls === focus);
    if (n) setPinned(n.id);
    setPopup(null); // chegada por navegação, não por clique -- sem coordenada pra ancorar
    setFocus(null);
  }, [focus, setFocus]);

  const raw = useMemo(() => TRACES[traceKey].build(), [traceKey]);
  const trace = useMemo(() => (showIdle ? raw : raw.filter((s) => !s.idle)), [raw, showIdle]);
  const orientV = orientation === "v";
  const nodes = useMemo(() => layout(SCENARIO, orientation), [orientation]);
  useEffect(() => { setView(EXEC_DEFAULT_VIEW); setPopup(null); }, [orientation]);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  const idx = Math.min(i, trace.length - 1);
  const step = trace[idx] || {};

  useEffect(() => { setI(0); setPopup(null); }, [traceKey, showIdle]);

  // Clicar num QUADRADINHO DE FASE (dentro do cartão do nó, não o cartão
  // inteiro) pula a reprodução direto pro passo em que ESTE nó roda NAQUELA
  // fase -- "quero ver o agent na fase 3" sem procurar manualmente na
  // timeline. Só faz sentido na trilha "tc" (a única com fase de verdade);
  // clicar num pip com outra trilha selecionada troca pra "tc" primeiro e
  // resolve o salto só quando o trace novo estiver pronto -- o efeito
  // abaixo roda DEPOIS do "zera i pra 0" logo acima (mesma ordem de
  // declaração = mesma ordem de execução no commit), então o índice certo
  // vence por último, sem um "pulo visual" passando por 0.
  const [phaseJumpRequest, setPhaseJumpRequest] = useState(null);
  const performPhaseJump = (nodeId, phaseN, traceArr, fromIdx) => {
    // Busca A PARTIR do passo seguinte ao atual, em ciclo -- clicar de novo
    // no MESMO pip avança pro próximo quadro em vez de ficar preso no
    // primeiro achado; fromIdx=-1 (vindo de outra trilha) começa do zero.
    for (let k = 0; k < traceArr.length; k++) {
      const c = (fromIdx + 1 + k) % traceArr.length;
      const s = traceArr[c];
      if (s.node === nodeId && s.counters && s.counters.phase === phaseN) { setI(c); return; }
    }
  };
  const jumpToPhase = (nodeId, phaseN) => {
    setPlaying(false);
    if (traceKey !== "tc") { setPhaseJumpRequest({ nodeId, phase: phaseN }); setTraceKey("tc"); return; }
    performPhaseJump(nodeId, phaseN, trace, idx);
  };
  useEffect(() => {
    if (!phaseJumpRequest || traceKey !== "tc") return;
    performPhaseJump(phaseJumpRequest.nodeId, phaseJumpRequest.phase, trace, -1);
    setPhaseJumpRequest(null);
    // performPhaseJump não entra nas deps: é recriada a cada render (barata,
    // sem estado próprio), e incluí-la quebraria o "só uma vez quando o
    // trace novo chegar" -- rodaria de novo a cada render à toa.
  }, [phaseJumpRequest, traceKey, trace]);

  useEffect(() => {
    if (!playing) return;
    const t = setTimeout(() => setI((p) => (p + 1 >= trace.length ? (setPlaying(false), p) : p + 1)), speed);
    return () => clearTimeout(t);
  }, [playing, i, speed, trace.length]);

  const move = useCallback((d) => { setPlaying(false); setI((p) => Math.max(0, Math.min(trace.length - 1, p + d))); }, [trace.length]);
  useEffect(() => {
    const h = (e) => {
      if (e.target.tagName === "INPUT" && e.target.type === "text") return;
      if (e.key === "ArrowRight") move(1);
      else if (e.key === "ArrowLeft") move(-1);
      else if (e.key === " ") { e.preventDefault(); setPlaying((p) => !p); }
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [move]);

  const seen = trace.slice(0, idx + 1);
  const launched = traceKey === "tc" && seen.some((s) => s.kind === "release");
  const vanished = traceKey === "reset" && step.kind === "vanish";
  const visits = useMemo(() => {
    const v = {}; seen.forEach((s) => { if (s.kind === "visit") v[s.node] = (v[s.node] || 0) + 1; }); return v;
  }, [idx, traceKey, showIdle]);

  const detail = pinned ? byId[pinned] : byId[step.node] || byId.station;
  const dm = cls(detail.cls) || {};
  const pathEdges = useMemo(() => new Set(ancestors(step.node || "station").map(([a, b]) => a + ">" + b)), [step.node]);

  const snip = SNIPPETS[step.src];
  // Os novos nós de decisão UBF (agent/ubfstate/ubfarb/ubfbeh*) não têm EDL de
  // produção real (ver a nota em UBF_EDL_TEXT) — checa a tabela ilustrativa
  // primeiro, cai para o EDL real dos outros 72 nós senão.
  const ubfEdlRange = UBF_EDL_RANGE[detail.id];
  const edlSrc = ubfEdlRange ? UBF_EDL_TEXT : EDL_TEXT;
  const edlRange = ubfEdlRange || EDL_RANGE[detail.id] || EDL_RANGE.station;
  // Janela de linhas em vez de scroll: ver o "porque" no cabecalho de windowLines().
  const codeWin = useMemo(() => (snip ? windowLines(snip.lines, step.hl, 22) : null), [snip, step.hl]);
  const edlWin = useMemo(() => windowLines(edlSrc, edlRange, 22), [edlSrc, edlRange]);
  // Aba "Codigo"/"EDL" so faz sentido tendo o que mostrar -- se o usuario
  // estava nela e o passo/no atual deixou de ter trecho de fonte (ex.: um no
  // sem override de fase nenhum), cai para "Passo" em vez de mostrar vazio.
  useEffect(() => {
    if (detailTab === "code" && !snip) setDetailTab("step");
  }, [detailTab, snip]);

  const edges = [];
  ALL.forEach((n) => (n.children || []).forEach((c) => edges.push([n.id, c.id])));
  // W/H generalizados pela extensão REAL dos nós, não por maxDepth*COL: na
  // horizontal isso dá exatamente o mesmo valor (x cresce estritamente com a
  // profundidade), mas na vertical quem estica a largura é o número de
  // FOLHAS (x = índice*COL_V), não a profundidade -- uma fórmula só que
  // funciona pras duas sem precisar de um "if" aqui.
  const W = Math.max(...nodes.map((n) => n.x)) + NW + 30;
  const H = Math.max(...nodes.map((n) => n.y)) + NH + 30;
  const ctr = step.counters || {};
  const curPhase = ctr.phase;

  // Horizontal: topo de cada COLUNA (rótulo "raiz"/"players"/... folga do nó
  // mais alto DAQUELA coluna). Vertical: esquerda de cada LINHA (rótulo
  // folga do nó mais à esquerda DAQUELA linha -- mesma ideia, eixo trocado).
  // Nunca um valor fixo: um nó visitado cedo na travessia DFS pode acabar no
  // extremo absoluto (0) -- caso medido de "IoHandler" sob "executivo e E/S"
  // na horizontal, com o rótulo caindo atrás do próprio topo da caixa.
  const depthHeaderPos = useMemo(() => {
    const top = {};
    nodes.forEach((n) => {
      const edge = orientV ? n.x : n.y - NH / 2;
      if (top[n.depth] === undefined || edge < top[n.depth]) top[n.depth] = edge;
    });
    return top;
  }, [nodes, orientV]);
  // Margem de topo (horizontal) ou de esquerda (vertical) do viewBox: tem de
  // acomodar o rótulo mais próximo do extremo -- 12px de folga do nó +
  // ~10px de ascendente da fonte na horizontal; na vertical o rótulo cresce
  // PRA ESQUERDA a partir do nó (texto ancorado à direita, ver o render),
  // então a margem tem de caber a largura do rótulo mais longo
  // ("sistemas primários" ≈ 130px em mono 10px), não só sua altura.
  const topMargin = orientV ? 14 : Math.max(30, -Math.min(...Object.values(depthHeaderPos), 0) + 12 + 10);
  const leftMargin = orientV ? Math.max(150, -Math.min(...Object.values(depthHeaderPos), 0) + 150) : 14;

  // Centraliza o elemento de COMPONENTE DE ATUAÇÃO -- o nó ativo do passo
  // (o mesmo que ganha o halo "executando") -- no zoom ATUAL, sem recalculá-
  // lo. O zoom é escolha do usuário (a barra deslizante); entre passos, ele
  // PERSISTE -- só o enquadramento (pan) acompanha. Antes disto, cada passo
  // recomputava um k próprio (ajustando a caixa do caminho inteiro), e o
  // zoom "pulava" a cada passo -- o oposto de uma barra que o usuário ajusta
  // uma vez e espera que fique.
  const followViewFor = (nodeId, k) => {
    const n = pos[nodeId];
    if (!n) return null;
    // O <g> do grafo tem style={transformOrigin:"center"} -- a ANCORA do
    // transform CSS não é a origem (0,0) do conteúdo, é o CENTRO do viewBox
    // (view-box é o transform-box padrão em SVG). A composição real é
    // origin + k*(p-origin) + (tx,ty); pra centralizar p em `origin`,
    // (tx,ty) = k*(origin-p) -- NÃO (origin-p)*k (o que dava um deslocamento
    // a mais de origin*(k-1), crescendo com o zoom -- medido botando
    // "station" fora da tela com k=1.73 antes deste ajuste).
    // O alvo vertical não é o centro geométrico puro: os botões flutuantes
    // (zoom/detalhe/ajustar, absolutos no canto superior direito) ficam POR
    // CIMA do canvas -- centralizar exatamente no meio deixava o nó, em zoom
    // alto, bater embaixo deles (medido em viewport estreito, com "Station"
    // perto do topo do caminho). Um viés de 6% da altura do viewBox empurra
    // o enquadramento pra baixo, sem custar quase nada do outro lado (o
    // rodapé não tem overlay nenhum).
    const Ox = orientV ? (W - leftMargin) / 2 : (-leftMargin + W / 2);
    const Oy = (H - topMargin) / 2 + (H + topMargin) * 0.06;
    return { k, x: k * (Ox - n.x), y: k * (Oy - n.y) };
  };

  useEffect(() => {
    if (!autoFollow || !step.node) return;
    const v = followViewFor(step.node, view.k);
    if (v) setView(v);
    // view.k entra de propósito: se o usuário reajustar o zoom (a barra)
    // enquanto "Seguir ramo" está ligado, o enquadramento recalcula o pan
    // pro MESMO nó no zoom novo, em vez de deixar o nó fugir do centro.
  }, [autoFollow, idx, step.node, view.k]);

  const bandFor = (rootId) => {
    const ids = flat(byId[rootId]).map((n) => n.id);
    const ys = ids.map((id) => pos[id].y), xs = ids.map((id) => pos[id].x);
    return { y0: Math.min(...ys) - NH / 2 - 5, y1: Math.max(...ys) + NH / 2 + 5, x0: Math.min(...xs) - 6, x1: Math.max(...xs) + NW + 6 };
  };

  const segColor = (s) =>
    s.kind === "rf" ? "var(--rf)" : s.kind === "release" ? "var(--new)" :
    s.kind === "name" ? "var(--ok)" : s.kind === "phase" ? "var(--ink)" :
    s.counters && s.counters.phase != null ? ["var(--seg-phase-0)", "var(--seg-phase-1)", "var(--seg-phase-2)", "var(--seg-phase-3)"][s.counters.phase] : "var(--rule)";

  const slots = allSlots(detail.cls);

  return (
    <>
      <div className="mx-body" style={{ paddingBottom: Math.max(transportH, 86) + 14 }}>
        <div style={{ display: "flex", gap: 14, alignItems: "center", flexWrap: "wrap", marginBottom: 10 }}>
          <div className="mx-tabs">
            {Object.entries(TRACES).map(([k, t]) => (
              <button key={k} className="mx-tab" data-on={traceKey === k ? 1 : 0} onClick={() => setTraceKey(k)}>{t.label}</button>
            ))}
          </div>
          <div className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", display: "flex", gap: 12, flexWrap: "wrap" }}>
            <span>cycle {ctr.cycle ?? 0}</span><span>frame {ctr.frame ?? "—"}</span>
            <span style={{ color: curPhase != null ? "var(--hot)" : "inherit" }}>phase {curPhase ?? "—"}</span>
            <span>exec {ctr.exec ?? 0}</span><span>t_sim {((ctr.simT ?? 0) * 1000).toFixed(0)} ms</span>
          </div>
          {traceKey === "tc" && (
            <div style={{ display: "flex", gap: 4, marginLeft: "auto", flexWrap: "wrap" }}>
              {PHASES.map((p) => {
                const on = curPhase === p.n;
                return (
                  <div key={p.n} className={on ? "mx-phase-now" : ""} style={{ padding: "3px 9px", borderRadius: 2, fontSize: 11.5, background: on ? "var(--ink)" : "var(--panel)", color: on ? "var(--paper)" : "var(--muted)", transition: "background 160ms, color 160ms" }}>
                    <span className="mx-mono">{p.n}</span> {p.label}
                  </div>
                );
              })}
            </div>
          )}
        </div>

        <div className="mx-graph" ref={graphRef}>
          <div className="mx-zoom">
            <div className="mx-zoomslider" title="Zoom -- também funciona com a roda do mouse, e continua valendo com 'Seguir ramo' ligado (o próprio acompanhamento move esta barra a cada passo).">
              <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={0.01} value={view.k} aria-label="Zoom"
                onPointerDown={() => { sliderActive.current = true; }}
                onPointerUp={() => { sliderActive.current = false; }}
                onChange={(e) => setView((v) => ({ ...v, k: Number(e.target.value) }))} />
              <span className="mx-mono">{view.k.toFixed(2)}×</span>
            </div>
            <button className="mx-zbtn" data-w="1" onClick={() => setShowDetail((s) => !s)} title="Oculta o painel de detalhe abaixo, dando mais área ao grafo">
              {showDetail ? "▾ detalhe" : "▸ detalhe"}
            </button>
            <button className="mx-zbtn" data-w="1" onClick={() => setView(EXEC_DEFAULT_VIEW)}>ajustar</button>
          </div>
          <div className="mx-svgwrap" data-expanded={showDetail ? 0 : 1}>
            <svg ref={svgRef} viewBox={`${-leftMargin} ${-topMargin} ${W + (orientV ? leftMargin : 0)} ${H + topMargin}`} preserveAspectRatio="xMidYMid meet"
                 onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
              <g transform={`translate(${view.x},${view.y}) scale(${view.k})`}
                 style={{ transformOrigin: "center", transition: autoFollow && !drag.current && !sliderActive.current ? "transform 420ms cubic-bezier(.22,.61,.36,1)" : "none" }}>
                {["ac", "ac2"].map((pid) => {
                  const b = bandFor(pid);
                  return <rect key={pid} x={b.x0} y={b.y0} width={b.x1 - b.x0} height={b.y1 - b.y0} rx="3" fill="var(--band-bg)" />;
                })}
                {DEPTH_LABELS.map((l, d) => {
                  if (orientV) {
                    // Vertical: rótulo à ESQUERDA da linha (nunca um x fixo -- a
                    // mesma ideia de depthHeaderPos, só que ancorado à direita
                    // do texto, que cresce pra longe do nó). Divisória
                    // horizontal entre linhas, não mais vertical entre colunas.
                    const rowY = d * ROW_V;
                    const labelX = (depthHeaderPos[d] ?? 0) - 14;
                    return (
                      <g key={d}>
                        <text x={labelX} y={rowY + 3} textAnchor="end" className="mx-mono" style={{ fontSize: 10, fill: "var(--muted)" }}>{l}</text>
                        {d > 0 && <line x1={-leftMargin + 10} y1={rowY - ROW_V / 2} x2={W - 20} y2={rowY - ROW_V / 2} stroke="var(--rule)" strokeWidth="1" strokeDasharray="2 4" />}
                      </g>
                    );
                  }
                  // 12px de folga acima do no mais alto DESTA coluna -- nunca um y
                  // fixo (ver o comentario de depthHeaderPos() acima). Renderizado
                  // DEPOIS da faixa de destaque (banda ac/ac2, acima) de proposito --
                  // a banda cobre toda a subarvore de components: do Aircraft e, em
                  // ordem de pintura SVG, um elemento desenhado depois fica POR CIMA;
                  // com a ordem invertida a banda escondia por completo os rotulos de
                  // coluna que caem dentro da faixa (medido: "subsistemas"/"detalhe"/
                  // "acoes" ficavam com 100% de sobreposicao vertical, texto invisivel).
                  const labelY = (depthHeaderPos[d] ?? 0) - 12;
                  return (
                    <g key={d}>
                      <text x={d * COL} y={labelY} className="mx-mono" style={{ fontSize: 10, fill: "var(--muted)" }}>{l}</text>
                      {d > 0 && <line x1={d * COL - 20} y1={labelY - 10} x2={d * COL - 20} y2={H - 26} stroke="var(--rule)" strokeWidth="1" strokeDasharray="2 4" />}
                    </g>
                  );
                })}
                {edges.map(([a, b]) => {
                  const p = pos[a], q = pos[b], child = byId[b];
                  const hidden = child.dynamic && (!launched || vanished);
                  const onPath = pathEdges.has(a + ">" + b);
                  // A aresta que acabou de ser atravessada (termina no no ATIVO) ganha
                  // a "marcha de formigas" mais rapida, e um pulso de chegada no destino
                  // -- as demais do caminho ficam com a mesma animacao, so mais lenta.
                  const isCurrent = onPath && b === step.node;
                  // Horizontal: cotovelo direita-do-pai -> baixo/cima -> esquerda-
                  // do-filho, rótulo na goteira (largura COL-NW, sempre a mesma
                  // porque y varia por IRMÃO -- cada aresta já tem sua própria
                  // faixa vertical).
                  // Vertical: MEDIDO quebrando -- ancorar o rótulo entre pai e
                  // filho (largura = distância em x entre os dois) sobrepõe
                  // agressivamente o rótulo de QUALQUER outro filho do MESMO
                  // pai, porque todas as arestas de um fan-out largo compartilham
                  // a mesma faixa de y (o "meio" entre duas linhas de profundi-
                  // dade é igual pra todo mundo) e frequentemente a mesma faixa
                  // de x também (a do meio do pai até o proprio meio). A correção:
                  // ancorar o rótulo na largura do PRÓPRIO FILHO (mesma largura e
                  // x da caixa dele) -- como caixas de filhos nunca se sobrepõem
                  // (mesma garantia do layout()), os rótulos herdam essa garantia
                  // de graça. Cabe: o vão entre duas linhas (ROW_V-NH)/2 = 33px
                  // por lado comporta as duas linhas (via+dt, 12px cada).
                  let dPath, labelX, labelW, viaY, dtY, haloX, haloY;
                  if (orientV) {
                    const midY = (p.y + NH / 2 + (q.y - NH / 2)) / 2;
                    dPath = `M ${p.x + NW / 2} ${p.y + NH / 2} V ${midY} H ${q.x + NW / 2} V ${q.y - NH / 2}`;
                    labelX = q.x; labelW = NW;
                    viaY = midY + 2; dtY = midY + 16;
                    haloX = q.x + NW / 2; haloY = q.y - NH / 2;
                  } else {
                    const mid = p.x + NW + 16;
                    dPath = `M ${p.x + NW} ${p.y} H ${mid} V ${q.y} H ${q.x}`;
                    labelX = mid + 4; labelW = Math.max(10, q.x - (mid + 4) - 6);
                    viaY = q.y - 13; dtY = q.y + 3;
                    haloX = q.x; haloY = q.y;
                  }
                  return (
                    <g key={a + b} opacity={hidden ? 0.22 : 1}>
                      <path d={dPath} fill="none"
                        stroke={onPath ? "var(--hot)" : "var(--rule)"} strokeWidth={onPath ? 2.2 : 1}
                        className={isCurrent ? "mx-edge-current" : onPath ? "mx-edge-onpath" : ""} />
                      {isCurrent && (
                        <circle cx={haloX} cy={haloY} r="5" className="mx-halo" stroke="var(--hot)" strokeWidth="2" />
                      )}
                      {/* foreignObject+ellipsis, nao <text> livre: a goteira entre
                         * colunas (COL-NW) e finita, e rotulos de slot EDL como
                         * "dataRecorder:"/"addNewPlayer()" sao mais largos que ela --
                         * um <text> sem largura maxima cresce PRA DENTRO da caixa do
                         * no filho (pintada DEPOIS, por cima) e o excesso some sem
                         * aviso nenhum. Medido rodando: "components:" perto de
                         * Radar/AirTrkMgr aparecia cortado em "comp", com o resto
                         * escondido atras da caixa -- mesma familia de bug de #2
                         * acima, so que em texto SVG cru em vez de foreignObject. */}
                      {child.via && (
                        <foreignObject x={labelX} y={viaY} width={labelW} height="12" className="mx-fo">
                          <div className="mx-fo-row" title={child.via}>
                            <span className="mx-fo-sub" style={{ fontSize: 8.5, color: onPath ? "var(--hot)" : "var(--sub-muted)" }}>{child.via}</span>
                          </div>
                        </foreignObject>
                      )}
                      {onPath && step.dt != null && (
                        <foreignObject x={labelX} y={dtY} width={labelW} height="12" className="mx-fo">
                          <div className="mx-fo-row" title={`dt ${fmt(step.dt)}`}>
                            <span className="mx-fo-sub" style={{ fontSize: 8.5, color: "var(--hot)" }}>dt {fmt(step.dt)}</span>
                          </div>
                        </foreignObject>
                      )}
                    </g>
                  );
                })}
                {showNames && !orientV && NAME_LINKS.map((l) => {
                  const a = pos[l.from], b = pos[l.to];
                  // `l.dy` evita a MESMA faixa vertical que o "via:" do proprio no
                  // (agora sempre visivel e largo -- ver o comentario da secao via/dt
                  // acima) E evita dois links do MESMO no colidirem entre si -- ver o
                  // comentario junto de NAME_LINKS.
                  const ly = (a.y + b.y) / 2 + (l.dy || 0);
                  const lx = Math.min(a.x, b.x) - 28;
                  return (
                    <g key={l.slot} opacity="0.8">
                      <path d={`M ${a.x + NW / 2} ${a.y + NH / 2} C ${a.x - 26} ${a.y + 30}, ${b.x - 26} ${b.y - 30}, ${b.x + NW / 2} ${b.y - NH / 2}`} fill="none" stroke="var(--ok)" strokeWidth="1.3" strokeDasharray="2 3" />
                      <foreignObject x={lx - 100} y={ly - 6} width="100" height="12" className="mx-fo">
                        <div className="mx-fo-row" style={{ justifyContent: "flex-end" }} title={l.slot}>
                          <span className="mx-fo-sub" style={{ fontSize: 8.5, color: "var(--ok)" }}>{l.slot}</span>
                        </div>
                      </foreignObject>
                    </g>
                  );
                })}
                {step.kind === "rf" && (
                  <g>
                    <path d={`M ${pos.a1.x + NW / 2} ${pos.a1.y - NH / 2} C ${pos.a1.x} ${pos.a1.y - 80}, ${pos.ac2.x + NW} ${pos.ac2.y - 80}, ${pos.ac2.x + NW / 2} ${pos.ac2.y - NH / 2}`}
                      fill="none" stroke="var(--rf)" strokeWidth="1.8" strokeDasharray="5 3" className="mx-edge-evt" />
                    <circle cx={pos.ac2.x + NW / 2} cy={pos.ac2.y - NH / 2} r="5" className="mx-halo" stroke="var(--rf)" strokeWidth="2" />
                    <text x={(pos.a1.x + pos.ac2.x) / 2 + NW / 2} y={pos.a1.y - 66} textAnchor="middle" className="mx-mono" style={{ fontSize: 10, fill: "var(--rf)" }}>event(RF_EMISSION)</text>
                  </g>
                )}
                {step.kind === "release" && (
                  <g>
                    <path d={`M ${pos.sto.x + NW / 2} ${pos.sto.y + NH / 2} C ${pos.sto.x} ${pos.sto.y + 70}, ${pos.flyout.x + 30} ${pos.flyout.y - 50}, ${pos.flyout.x + NW / 2} ${pos.flyout.y - NH / 2}`}
                      fill="none" stroke="var(--new)" strokeWidth="2" strokeDasharray="4 3" className="mx-edge-evt" />
                    <circle cx={pos.flyout.x + NW / 2} cy={pos.flyout.y - NH / 2} r="5" className="mx-halo" stroke="var(--new)" strokeWidth="2" />
                  </g>
                )}
                {nodes.map((n) => {
                  const active = step.node === n.id;
                  const inStack = (step.stack || []).some((s) => s.node === n.id);
                  const running = active && step.runs;
                  const ghost = n.dynamic && (!launched || vanished);
                  const never = !n.phases.length;
                  const v = visits[n.id] || 0;
                  // Key composta SO nos nos dinamicos (o missil): forca remontagem quando
                  // ghost vira real (e vice-versa), o que faz a animacao de entrada
                  // (mx-popin) tocar de novo -- sem isso, o React so atualiza atributos
                  // do MESMO elemento e a keyframe nunca reinicia.
                  const key = n.dynamic ? `${n.id}-${ghost ? "g" : "r"}` : n.id;
                  return (
                    <g key={key} className="mx-node" data-pop={n.dynamic && !ghost ? 1 : 0}
                       transform={`translate(${n.x},${n.y - NH / 2})`}
                       onClick={(e) => {
                         setPlaying(false);
                         const willPin = pinned !== n.id;
                         setPinned(willPin ? n.id : null);
                         if (willPin && graphRef.current) {
                           const r = graphRef.current.getBoundingClientRect();
                           setPopup({ nodeId: n.id, x: e.clientX - r.left, y: e.clientY - r.top });
                         } else {
                           setPopup(null);
                         }
                       }}>
                      {running && <circle cx={NW / 2} cy={NH / 2} r={NH / 2} className="mx-halo" stroke="var(--hot)" strokeWidth="2.5" />}
                      <rect x="0" y="0" width={NW} height={NH} rx="2"
                        fill={running ? "var(--hot)" : active ? "var(--active-bg)" : never ? "var(--never-bg)" : "var(--paper)"}
                        stroke={pinned === n.id ? "var(--ink)" : running ? "var(--hot)" : active ? "var(--ink)" : inStack ? "var(--muted)" : "var(--rule)"}
                        strokeWidth={active || pinned === n.id ? 1.6 : 1}
                        strokeDasharray={ghost ? "3 2" : "0"} opacity={ghost ? 0.45 : 1} />
                      <rect x="0" y="0" width="3" height={NH} fill={THREAD_COLOR[n.thread] || "var(--rule)"} opacity={ghost ? 0.4 : 0.9} />
                      <foreignObject x="9" y="2" width={NW - 58} height="16" className="mx-fo" style={{ opacity: ghost ? 0.55 : 1 }}>
                        <div className="mx-fo-row" title={n.cls}><span className="mx-fo-cls" style={{ fontSize: 11, color: running ? "var(--paper)" : never ? "var(--muted)" : "var(--ink)" }}>{n.cls}</span></div>
                      </foreignObject>
                      {/* largura igual a do nome (NW-58): a faixa dos pips de fase   *
                         * (comeca em NW-48, y=21) cai bem NESSA linha -- um subtitulo *
                         * mais largo que isso ficava por baixo dos pips (medido     *
                         * rodando em zoom alto: "ownship .player .s/System" cobria   *
                         * os 3 primeiros quadradinhos de fase). */}
                      <foreignObject x="9" y="18" width={NW - 58} height="12" className="mx-fo" style={{ opacity: ghost ? 0.5 : 1 }}>
                        <div className="mx-fo-row" title={`${n.edl}${n.player ? " ·player" : ""}${n.disp ? "" : " ·s/System"}`}><span className="mx-fo-sub" style={{ fontSize: 9, color: running ? "var(--running-fg)" : "var(--sub-muted)" }}>
                          {n.edl}{n.player ? " ·player" : ""}{n.disp ? "" : " ·s/System"}
                        </span></div>
                      </foreignObject>
                      <g transform={`translate(${NW - 48}, 21)`} opacity={ghost ? 0.5 : 1}>
                        {PHASES.map((p) => {
                          const has = n.phases.includes(p.n);
                          const own = has && phaseOwner(n.cls, p.n) === n.cls;
                          const now = has && curPhase === p.n;
                          return (
                            <rect key={p.n} x={p.n * 10} y="0" width="7" height="7" rx="1" className={now ? "mx-phase-now" : ""}
                              fill={now ? (running ? "var(--phase-now-bg)" : "var(--hot)") : has ? (running ? "var(--phase-has-running-bg)" : own ? "var(--ink)" : "var(--phase-inherited)") : "none"}
                              stroke={has ? "none" : running ? "var(--phase-stroke-running)" : "var(--rule)"} strokeWidth="1"
                              style={{ cursor: has ? "pointer" : "default" }}
                              onClick={has ? (e) => { e.stopPropagation(); jumpToPhase(n.id, p.n); } : undefined}>
                              {has && <title>ir ao passo da fase {p.n} ({p.label}) para {n.cls}</title>}
                            </rect>
                          );
                        })}
                      </g>
                      {v > 0 && <text x={NW - 7} y="14" textAnchor="end" className="mx-mono" style={{ fontSize: 9, fill: running ? "var(--running-fg)" : "var(--muted)" }}>×{v}</text>}
                    </g>
                  );
                })}
              </g>
            </svg>
          </div>
          {popup && byId[popup.nodeId] && (() => {
            const n = byId[popup.nodeId];
            const e = MODEL[n.cls] || {};
            const fname = factoryOf(n.cls);
            const diverges = fname !== n.cls;
            const gw = graphRef.current ? graphRef.current.clientWidth : 800;
            const gh = graphRef.current ? graphRef.current.clientHeight : 500;
            const PW = 250, PH = 172;
            const left = Math.min(Math.max(8, popup.x + 14), Math.max(8, gw - PW - 8));
            const top = Math.min(Math.max(40, popup.y - 12), Math.max(40, gh - PH - 8));
            return (
              <div className="mx-nodepopup" style={{ left, top, width: PW }}>
                <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 6 }}>
                  <span className="mx-mono" style={{ fontWeight: 600, fontSize: 12.5 }}>{n.cls}</span>
                  <button className="mx-nodepopup-x" onClick={() => setPopup(null)} aria-label="Fechar" title="Fechar">×</button>
                </div>
                <div style={{ fontSize: 11.5, lineHeight: 1.6, marginTop: 5 }}>
                  <div>nome de fábrica: <b className="mx-mono">{fname}</b>{diverges && (
                    <span style={{ color: "var(--hot)" }} title="Nome de fábrica diferente do nome da classe C++ -- é ESTE que o EDL espera dentro de ( ... )">
                      {" "}≠ classe C++
                    </span>
                  )}</div>
                  <div>registrada em fábrica: {e.r
                    ? "sim"
                    : <b style={{ color: "var(--rf)" }} title="Sem IMPLEMENT_*SUBCLASS -- um ( ... ) com este nome no EDL falha com 'unknown factory name'">não</b>}</div>
                  <div>slots próprios: {e.sl ? e.sl.length : 0}{e.b && <> · deriva de <span className="mx-mono">{e.b}</span></>}</div>
                </div>
                {onOpenCatalog && (
                  <button className="mx-nodepopup-link" onClick={() => onOpenCatalog(n.cls)}>Ver classe completa no Catálogo →</button>
                )}
              </div>
            );
          })()}
          <div className="mx-leg">
            <button className="mx-leg-toggle" onClick={() => setShowCardLegend((s) => !s)}>
              {showCardLegend ? "▾" : "▸"} como ler um cartão
            </button>
            <span><b style={{ color: "var(--hot)" }}>■</b> executando</span>
            <span>pips: <b>■</b> implementa · <b style={{ color: "var(--phase-inherited)" }}>■</b> herda de ancestral · ▫ ninguém na cadeia</span>
            <span>×n visitas</span>
            <span>thread: <b style={{ color: "var(--ink)" }}>TC</b> · <b style={{ color: "var(--bgc)" }}>fundo</b> · <b style={{ color: "var(--ok)" }}>rede</b></span>
            <span style={{ color: "var(--rf)" }}>--- evento</span>
            <span style={{ color: "var(--ok)" }}>··· por nome</span>
            <span>roda = zoom · arrastar = mover</span>
          </div>
          {/* Legenda visual: pedido explicito ("talvez uma legenda ajude
             * muito") -- a linha de texto acima diz O QUE cada marca significa,
             * mas nao ONDE ela fica no cartao. Um cartao de exemplo, anotado com
             * setas, responde "rápido entendimento" melhor que prosa -- fechado
             * por padrao pra nao competir por altura com o grafo (o mesmo motivo
             * do toggle "detalhe"). */}
          {showCardLegend && (
            <div className="mx-cardleg">
              {/* viewBox bem mais largo que o cartão em si: cada rótulo precisa
                 * de ~190px, e a goteira esquerda/direita do cartão de exemplo
                 * tem de caber isso INTEIRO -- a mesma lição do achado sobre os
                 * rótulos "via:"/"dt:" do grafo principal (goteira estreita
                 * corta texto por baixo da caixa). Aqui não há caixa vizinha
                 * pra esconder o corte, então o risco é pior: o texto simplesmente
                 * sai do viewBox e desaparece, sem nem um "..." de aviso. */}
              <svg viewBox="0 0 900 220" width="900" height="220">
                <g transform="translate(350,30)">
                  <rect x="0" y="0" width={NW} height={NH} rx="2" fill="var(--paper)" stroke="var(--ink)" strokeWidth="1.6" />
                  <rect x="0" y="0" width="3" height={NH} fill="var(--bgc)" />
                  <text x="9" y="13" className="mx-mono" style={{ fontSize: 11, fontWeight: 600, fill: "var(--ink)" }}>Radar</text>
                  <text x="9" y="27" className="mx-mono" style={{ fontSize: 9, fill: "var(--sub-muted)" }}>radar ·tc+fundo</text>
                  <g transform={`translate(${NW - 48}, 21)`}>
                    {[0, 1, 2, 3].map((p) => (
                      <rect key={p} x={p * 10} y="0" width="7" height="7" rx="1"
                        fill={p < 2 ? "var(--ink)" : "none"} stroke={p < 2 ? "none" : "var(--rule)"} strokeWidth="1" />
                    ))}
                  </g>
                  <text x={NW - 7} y="14" textAnchor="end" className="mx-mono" style={{ fontSize: 9, fill: "var(--muted)" }}>×3</text>
                </g>
                {[
                  // Ancora sempre na BORDA do cartao (nunca em cima de um glifo
                  // especifico) -- version anterior colocava o ponto exatamente
                  // sobre o "3" de "x3" e sobre um pip, competindo visualmente
                  // com o proprio conteudo que a legenda tenta explicar.
                  { x: 350, y: 39, lx: 210, ly: 18, w: 190, align: "right", label: "nome da classe C++ (nunca o nome de fábrica — veja a aba Classe)" },
                  { x: 350, y: 53, lx: 210, ly: 112, w: 190, align: "right", label: "identificador no EDL — mais \"·player\" ou \"·s/System\" quando aplicável" },
                  { x: 351, y: 47, lx: 210, ly: 178, w: 190, align: "right", label: "cor = onde a classe roda: preta TC, azul fundo, verde rede" },
                  { x: 558, y: 44, lx: 700, ly: 130, w: 190, align: "left", label: "4 quadrados = as 4 fases do frame; preenchido = implementa esta fase (herdada ou própria)" },
                  { x: 551, y: 30, lx: 700, ly: 20, w: 190, align: "left", label: "×N — quantas vezes este nó já foi visitado até o passo atual" },
                ].map((c, k) => (
                  <g key={k}>
                    <line x1={c.x} y1={c.y} x2={c.lx} y2={c.ly} stroke="var(--muted)" strokeWidth="1" strokeDasharray="2 2" />
                    <circle cx={c.x} cy={c.y} r="2.5" fill="var(--muted)" />
                    <foreignObject x={c.align === "right" ? c.lx - c.w : c.lx} y={c.ly - 9} width={c.w} height="46" className="mx-fo">
                      <div style={{ fontSize: 10.5, lineHeight: 1.3, color: "var(--muted)", textAlign: c.align }}>{c.label}</div>
                    </foreignObject>
                  </g>
                ))}
              </svg>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "4px 0 0", maxWidth: 720 }}>
                A borda fica <b style={{ color: "var(--ink)" }}>tracejada</b> quando o nó ainda não existe (um míssil na
                estação, antes do lançamento) e some o tracejado assim que ele nasce como player de verdade. O nó
                fica <b style={{ color: "var(--hot)" }}>laranja</b>, com um halo pulsando, exatamente no passo em que ele está EXECUTANDO
                agora — os demais na pilha de chamadas (acima dele) ficam só com a borda mais escura.
              </p>
            </div>
          )}
        </div>

        {showDetail && (
        <div className="mx-pane">
          {/* Clicar num cartão do grafo pausa (ver onClick de .mx-node) e fixa   *
             * este nó -- o painel abaixo (EDL/Classe/Código) já segue o fixado, *
             * não o passo atual; este bloco só torna isso EXPLÍCITO, com um    *
             * dado que não muda ao avançar/voltar o passo (ao contrário do     *
             * "×n" no próprio cartão, que conta só até aqui). */}
          {pinned && byId[pinned] && (
            <div className="mx-card" style={{ marginBottom: 10, borderLeft: "3px solid var(--hot)" }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 8, flexWrap: "wrap" }}>
                <span className="mx-mono" style={{ fontWeight: 600, fontSize: 12.5 }}>📌 fixado — {byId[pinned].cls}</span>
                <button className="mx-btn" style={{ fontSize: 11, padding: "2px 8px" }} onClick={() => { setPinned(null); setPopup(null); }}>soltar</button>
              </div>
              <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 4 }}>
                Não muda ao avançar/voltar o passo — thread <b>{byId[pinned].thread}</b>.{" "}
                {(() => {
                  const n = trace.filter((s) => s.node === pinned).length;
                  return n > 0
                    ? <>Aparece em <b className="mx-mono">{n}</b> de <b className="mx-mono">{trace.length}</b> passos da trilha "{TRACES[traceKey].label}".</>
                    : <>Não é visitado pela trilha "{TRACES[traceKey].label}" — ver Classe/EDL abaixo para os dados estáticos.</>;
                })()}
              </div>
            </div>
          )}
          <div className="mx-dtabs" role="tablist" aria-label="Detalhe do passo">
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "step"} data-on={detailTab === "step" ? 1 : 0} onClick={() => setDetailTab("step")}>Passo</button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "code"} data-on={detailTab === "code" ? 1 : 0} disabled={!snip} onClick={() => snip && setDetailTab("code")}>
              Código{snip && step.hl ? <span className="mx-dtab-dot" /> : null}
            </button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "edl"} data-on={detailTab === "edl" ? 1 : 0} onClick={() => setDetailTab("edl")}>EDL do cenário</button>
            <button className="mx-dtab" role="tab" aria-selected={detailTab === "class"} data-on={detailTab === "class" ? 1 : 0} onClick={() => setDetailTab("class")}>Classe</button>
          </div>

          <div className="mx-detailbody" key={detailTab}>
            {detailTab === "step" && (
              <>
                <div className="mx-card">
                  <div className="mx-mono" style={{ fontSize: 12.5, fontWeight: 600, marginBottom: 5 }}>{step.title}</div>
                  <p style={{ margin: 0, fontSize: 13, lineHeight: 1.5 }}>{step.body}</p>
                  {step.warn && <p className="mx-warn">{step.warn}</p>}
                </div>
                <div className="mx-lbl" style={{ marginTop: 12 }}><span>Pilha de chamadas</span></div>
                {(step.stack || []).map((s, k) => (
                  <div key={k} className="mx-mono" style={{ fontSize: 11.5, padding: "2px 0 2px 9px", marginLeft: k * 7, borderLeft: `2px solid ${k === step.stack.length - 1 ? "var(--hot)" : "var(--rule)"}` }}>{s.label}</div>
                ))}
              </>
            )}

            {detailTab === "code" && snip && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono">{snip.file}:{snip.line + (step.hl ? step.hl[0] : 0)}</span>
                  <span>{snip.trunc ? "corpo truncado na extração" : "C++"}</span>
                </div>
                {/* SEM caixa de rolagem: codeWin ja e a janela de linhas (no maximo   *
                   * 22) centrada no trecho destacado -- ver windowLines(). */}
                <div className="mx-code">
                  {codeWin.cutBefore && <div className="mx-codecut">⋯ {codeWin.offset} linha{codeWin.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {codeWin.lines.map((ln, k) => {
                    const abs = k + codeWin.offset;
                    const on = step.hl && abs >= step.hl[0] && abs <= step.hl[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{ln || " "}</span></div>;
                  })}
                  {codeWin.cutAfter && <div className="mx-codecut">⋯ {snip.lines.length - codeWin.offset - codeWin.lines.length} linhas abaixo ⋯</div>}
                </div>
              </>
            )}

            {detailTab === "edl" && (
              <>
                <div className="mx-lbl"><span className="mx-mono">{ubfEdlRange ? "ilustrativo (não é o EDL de produção)" : "cenario.edl"}</span><span>{detail.cls} · {detail.edl}</span></div>
                <div className="mx-edl">
                  {edlWin.cutBefore && <div className="mx-codecut">⋯ {edlWin.offset} linha{edlWin.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {edlWin.lines.map((ln, k) => {
                    const abs = k + edlWin.offset;
                    const on = abs >= edlRange[0] && abs <= edlRange[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{abs + 1}</span><span className="mx-src">{ln || " "}</span></div>;
                  })}
                  {edlWin.cutAfter && <div className="mx-codecut">⋯ {edlSrc.length - edlWin.offset - edlWin.lines.length} linhas abaixo ⋯</div>}
                </div>
              </>
            )}

            {detailTab === "class" && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono" style={{ color: "var(--ink)", fontWeight: 600 }}>{detail.cls}</span>
                  <span>{pinned ? "fixado" : "segue a execução"}</span>
                </div>
                <div style={{ fontSize: 11.5, color: "var(--muted)", marginBottom: 7 }}>
                  módulo <b className="mx-mono">{dm.m}</b> · EDL <b className="mx-mono">( {factoryOf(detail.cls)} )</b>
                  {dm.f ? <span style={{ color: "var(--rf)" }}> · nome divergente</span> : null}
                  {dm.r === false ? <span style={{ color: "var(--rf)" }}> · NÃO registrada</span> : null}
                  <br />{dm.src || dm.hd}
                </div>
                {chainOf(detail.cls).map((c, k) => {
                  const ph = (MODEL[c] && MODEL[c].sl ? MODEL[c].sl.length : 0);
                  const own = PHASES.filter((p) => (MODEL[c] ? MODEL[c].wp : []).length && phaseOwner(detail.cls, p.n) === c);
                  return (
                    <div key={c} style={{ padding: "3px 8px", marginLeft: k * 6, borderLeft: `2px solid ${own.length ? "var(--hot)" : "var(--rule)"}`, background: own.length ? "var(--panel)" : "transparent" }}>
                      <span className="mx-mono" style={{ fontSize: 11.5, fontWeight: own.length ? 600 : 400 }}>{c}</span>
                      <span style={{ fontSize: 11, color: "var(--muted)" }}>
                        {own.length ? ` — ${own.map((p) => p.m + "()").join(", ")}` : ""}
                        {ph ? ` · ${ph} slots` : ""}
                      </span>
                    </div>
                  );
                })}
                <div className="mx-lbl" style={{ marginTop: 12 }}>
                  <span>Slots ({slots.length} na cadeia)</span><span>{dm.own || 0} próprios</span>
                </div>
                <div className="mx-slotgrid">
                  {slots.map(([s, from], k) => (
                    <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>
                  ))}
                  {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>Nenhum slot em toda a cadeia.</div>}
                </div>
                {detail.note && <p className="mx-warn">{detail.note}</p>}
              </>
            )}
          </div>
        </div>
        )}
      </div>

      <div className="mx-transport" ref={transportRef}>
        <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>
          {playing && <span className="mx-dtab-dot" style={{ marginRight: 6, animation: "mx-glow 900ms ease-in-out infinite" }} />}
          {playing ? "Pausar" : "Reproduzir"}
        </button>
        <button className="mx-btn" onClick={() => move(-1)}>←</button>
        <button className="mx-btn" onClick={() => move(1)}>→</button>
        <button className="mx-btn" onClick={() => { setPlaying(false); setI(0); }}>Início</button>
        <div className="mx-tl" role="slider" aria-label="Linha do tempo" aria-valuenow={idx} aria-valuemin={0} aria-valuemax={trace.length - 1} tabIndex={0}
             onKeyDown={(e) => { if (e.key === "ArrowRight") move(1); if (e.key === "ArrowLeft") move(-1); }}>
          {trace.map((s, k) => {
            const evt = ["rf", "release", "name", "phase"].includes(s.kind);
            return <div key={k} className="mx-seg" onClick={() => { setPlaying(false); setI(k); }} title={s.title}
              style={{ background: k === idx ? "var(--hot)" : segColor(s), height: k === idx ? "100%" : evt ? "70%" : "40%", opacity: k <= idx ? 1 : 0.4 }} />;
          })}
        </div>
        <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", minWidth: 52 }}>{idx + 1}/{trace.length}</span>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }}>
          <input type="checkbox" checked={showIdle} onChange={(e) => setShowIdle(e.target.checked)} /> Ociosos
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }}
               title={orientV ? "Indisponível na árvore vertical (as setas pontilhadas ainda não têm posição calibrada nesse layout)" : ""}>
          <input type="checkbox" checked={showNames && !orientV} disabled={orientV} onChange={(e) => setShowNames(e.target.checked)} /> Ligações por nome
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }} title="Zoom/pan acompanham sozinhos o ramo em destaque a cada passo">
          <input type="checkbox" checked={autoFollow} onChange={(e) => setAutoFollow(e.target.checked)} /> Seguir ramo
        </label>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }} title="Raiz em cima, irmãos lado a lado, em vez de raiz à esquerda">
          <input type="checkbox" checked={orientV} onChange={(e) => setOrientation(e.target.checked ? "v" : "h")} /> Árvore vertical
        </label>
        <select className="mx-input" value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Velocidade">
          <option value={1100}>Lento</option><option value={650}>Normal</option><option value={240}>Rápido</option>
        </select>
      </div>
    </>
  );
}

/* ==================================================================== *
 * DECISAO DE VOO (producao) -- do updateTC() do agente ate o Autopilot
 *
 * Ao contrario da trilha "Thread de Tempo Critico" (framework puro,
 * escopo deliberado so em mixr::base::ubf -- ver a nota em walk() e no
 * README.md), esta aba mostra a cadeia REAL do MODELO de voo deste
 * repositorio (models/players/A-4/): FlightAgentTC, FlightState, BtBehavior,
 * os 4 ramos do Fallback de producao (flight_tree.xml) e FlightAction
 * escrevendo no Autopilot nativo -- UM EXEMPLO concreto do padrao
 * generico Agent/AgentTC + AbstractState/AbstractBehavior/AbstractAction,
 * que se aplica a QUALQUER player (ver a faixa 'escopo: framework UBF' e
 * o cartao 'Agent vs. AgentTC' dentro do componente).
 *
 * DUAS trilhas a mais espelham as pocs irmas que trocam so a FOLHA de
 * decisao, mantendo o mesmo agente/estado/acao/autopilot:
 * src/poc/python-flight (flight_tree_python.xml, folhas PyDecideAction,
 * delegando para libs/xpyembed) e src/poc/onnx-policy (flight_tree_onnx.xml,
 * folha OnnxPolicyAction, delegando para libs/xinfer). Cada uma troca a
 * ARVORE (FLIGHT_TREE_PYTHON/FLIGHT_TREE_ONNX) via a entrada de
 * FLIGHT_TRACES correspondente -- ver buildFlightIndex()/flightSkeleton()
 * logo abaixo, e activeTree/activeIndex dentro de FlightDecision().
 *
 * tools/extract_execution_chain.py so cobre contexts/src/mixr/ (ver
 * docs/manual/README.md) -- FLIGHT_SNIPPETS/FLIGHT_MODEL abaixo foram
 * conferidos a mao, direto do fonte de models/players/A-4/, arquivo e linha
 * reais, mesma pratica ja usada por EDL_TEXT/SCENARIO (curadoria manual
 * sobre dado real, nao invencao). Agent/AgentTC/AbstractState/
 * AbstractBehavior/AbstractAction JA estao no MODEL global (extraidos de
 * verdade) -- FLIGHT_MODEL so preenche o que o extrator nao cobre, e as
 * duas fontes se emendam na mesma cadeia (flightChainOf).
 * ==================================================================== */

const FLIGHT_SNIPPETS = {
  "FlightAgentTC::controller": {
    "file": "models/players/A-4/src/xnative/FlightAgentTC.cpp",
    "line": 72,
    "lines": [
      "void FlightAgentTC::controller(const double dt)",
      "{",
      "   if (dt <= 0.0) return;",
      "",
      "   if (getActor() == nullptr) initActor();",
      "",
      "   const auto player = dynamic_cast<models::Player*>(getActor());",
      "   if (player == nullptr) return;",
      "",
      "   const models::WorldModel* const world{player->getWorldModel()};",
      "   if (world == nullptr) return;",
      "",
      "   // 4 passagens por frame, uma por fase, cada uma com dt/4. A decisao",
      "   // pertence a fase 3 (\"logica e controle\"), com o dt do frame inteiro.",
      "   if (world->phase() != 3) return;",
      "",
      "   const int tag{xboard::threadTag()};",
      "   lastThreadTag.store(tag, std::memory_order_relaxed);",
      "",
      "   // Publica no quadro de leitura: e por ele que a linha de status mostra em",
      "   // que thread do pool T/C este aviao decidiu. O host nao alcanca mais esta",
      "   // classe -- ela mora no plugin do modelo.",
      "   xboard::setThreadTag(player->getID(), tag);",
      "",
      "   BaseClass::controller(dt * 4.0);",
      "",
      "   decisions.fetch_add(1, std::memory_order_relaxed);",
      "}"
    ],
    "trunc": false
  },
  "FlightState::updateState": {
    "file": "models/players/A-4/src/ubf/FlightState.cpp",
    "line": 49,
    "lines": [
      "void FlightState::updateState(const base::Component* const actor)",
      "{",
      "   BaseClass::updateState(actor);",
      "",
      "   const auto air = dynamic_cast<const models::AirVehicle*>(actor);",
      "   if (air == nullptr) {",
      "      snap = Snapshot{};",
      "      return;",
      "   }",
      "",
      "   Snapshot s;",
      "   s.valid = true;",
      "",
      "   const base::Vec3d& pos{air->getPosition()};",
      "   s.northM = pos[models::Player::INORTH];",
      "   s.eastM = pos[models::Player::IEAST];",
      "   s.altitudeM = air->getAltitudeM();",
      "   s.headingDeg = air->getHeadingD();",
      "   s.speedKts = air->getTotalVelocityKts();",
      "   s.rollDeg = air->getRollD();",
      "   s.pitchDeg = air->getPitchD();",
      "",
      "   // Referencia de solo, tambem pela pilha nativa: quem consulta o banco de",
      "   // elevacao do WorldModel e o proprio Player::updateElevation(), na fase de",
      "   // BACKGROUND (Player.cpp:630, dentro de updateData()) -- nao numa das",
      "   // quatro fases do frame de tempo critico. Consequencia pratica: onde a",
      "   // decisao roda na fase 3 a 50 Hz contra um background de 10 Hz, este",
      "   // valor pode estar ate 100 ms velho (~8 m percorridos), o que",
      "   // e irrelevante para um piso com centenas de metros de folga. E continua",
      "   // deterministico: em -deterministic o laco faz tcFrame() e updateData()",
      "   // em sequencia no mesmo passo, com qualquer numero de threads T/C.",
      "   //",
      "   // ARMADILHA: terrainValid NAO garante cobertura. O updateElevation()",
      "   // nativo ignora o retorno de getElevation(), entao uma aeronave fora da",
      "   // celula do tile recebe elevacao 0.0 com o flag LIGADO. Quem trata isso e",
      "   // o piso absoluto de domain/TerrainFloor.hpp, nao este campo.",
      "   s.terrainValid = air->isTerrainElevationValid();",
      "   s.terrainElevM = air->getTerrainElevationM();",
      "   s.altitudeAglM = air->getAltitudeAglM();",
      "",
      "   // Telemetria do 6-DOF -- tudo via AirVehicle, que repassa ao JSBSimModel",
      "   const double fuelMax{air->getFuelWtMax()};",
      "   s.fuelFraction = (fuelMax > 0.0) ? (air->getFuelWt() / fuelMax) : 1.0;",
      "   s.mach = air->getMach();",
      "   s.gLoad = air->getGload();",
      "   s.alphaDeg = air->getAngleOfAttack() * RAD2DEG;",
      "",
      "   // --- contato: pista do radar NATIVO (Antenna/Tws -> AirTrkMgr) ---",
      "   const xtrack::TrackInfo track{xtrack::nearestHostileTrack(air)};",
      "   if (track.found) {",
      "      s.hasContact = true;",
      "      s.contactName = track.name;",
      "      s.contactRangeM = track.rangeM;",
      "      s.contactRelBearingDeg = track.relBearingDeg;",
      "      s.contactDeltaAltM = track.deltaAltM;",
      "",
      "      // A pista vem RELATIVA ao ownship; somando a nossa posicao sai a",
      "      // posicao absoluta, que e o que vai no alerta para os outros avioes.",
      "      s.contactNorthM = s.northM + track.relNorthM;",
      "      s.contactEastM = s.eastM + track.relEastM;",
      "      s.contactAltitudeM = s.altitudeM + track.deltaAltM;",
      "   }",
      "",
      "   // --- alerta recebido pelo datalink NATIVO ---",
      "   const auto datalink = dynamic_cast<const AlertDatalink*>(air->getDatalink());",
      "   if (datalink != nullptr) {",
      "      const auto alert = datalink->getAlert();",
      "      s.hasAlert = alert.valid;",
      "      if (alert.valid) {",
      "         s.alertSender = alert.senderName;",
      "         s.alertContactName = alert.contactName;",
      "         s.alertNorthM = alert.northM;",
      "         s.alertEastM = alert.eastM;",
      "         s.alertAltitudeM = alert.altitudeM;",
      "         s.alertRangeM = alert.rangeM;",
      "      }",
      "   }",
      "",
      "   // --- arma: StoresMgr e opcional (nenhum aviao de producao declara",
      "   // 'stores:'). available() e o numero de armas disponiveis para",
      "   // LIBERACAO -- ao contrario de isWeaponAvailable(), nao depende de uma",
      "   // estacao ter sido SELECIONADA primeiro (getStoresManagement() ja",
      "   // devolve StoresMgr*, tipado -- ver Player.hpp).",
      "   const auto storesMgr = air->getStoresManagement();",
      "   s.weaponReady = (storesMgr != nullptr) && (storesMgr->available() > 0);",
      "",
      "   // --- navegacao NATIVA: Route/Steerpoint, so leitura ---",
      "   //",
      "   // mixr::models::Route::updateData() (chamado todo frame de BACKGROUND,",
      "   // incondicionalmente -- Route.cpp) atualiza os dados de guiagem de CADA",
      "   // steerpoint e sequencia a rota por DISTANCIA, independente do navMode",
      "   // do Autopilot. Navigation::updateNavSteering() copia o rumo/alcance do",
      "   // steerpoint \"to\" para os proprios campos da Navigation -- e o MESMO",
      "   // dado que Autopilot::processModeNavigation() consulta quando navMode",
      "   // esta ligado (ver o comentario do slot 'pilot:' em qualquer cenario que",
      "   // use o no ( Navigate )). Aqui a leitura e identica, so que por FORA do",
      "   // Autopilot -- quem decide o que fazer com ela e a arvore.",
      "   const auto nav = air->getNavigation();",
      "   if (nav != nullptr) {",
      "      s.hasNavSteering = nav->isNavSteeringValid();",
      "      s.navTrueBrgDeg = nav->getTrueBrgDeg();",
      "      const auto route = nav->getPriRoute();",
      "      const auto steerpoint = (route != nullptr) ? route->getSteerpoint() : nullptr;",
      "      if (steerpoint != nullptr) {",
      "         s.hasNavCmdAlt = steerpoint->isCmdAltValid();",
      "         s.navCmdAltM = steerpoint->getCmdAltitudeM();",
      "         s.hasNavCmdSpeed = steerpoint->isCmdAirspeedValid();",
      "         s.navCmdSpeedKts = steerpoint->getCmdAirspeedKts();",
      "      }",
      "   }",
      "",
      "   snap = s;",
      "",
      "   // Para onde a antena esta apontando AGORA -- publicado no quadro de leitura",
      "   // para o host empurrar ao Tacview.",
      "   //",
      "   // Isto e do MODELO e nao do host: quem sabe o que a aeronave esta",
      "   // enxergando e quem percebe. O host so relaia o que o quadro disser, e se",
      "   // um modelo nunca publicar, ele simplesmente nao desenha varredura -- um",
      "   // modelo sem radar e legitimo.",
      "   //",
      "   // Nao entra no dump deterministico: a varredura so alimenta o caminho de",
      "   // tempo real (ver app/RealTimeRun.cpp).",
      "   const RadarScanInfo scan{radarScanOf(air)};",
      "   xboard::setRadarScan(air->getID(), scan.found, scan.azimuthDeg, scan.elevationDeg,",
      "                        scan.rangeM, scan.horizontalBeamwidthDeg, scan.verticalBeamwidthDeg);",
      "}"
    ],
    "trunc": false
  },
  "BtBehavior::genAction": {
    "file": "models/players/A-4/src/ubf/BtBehavior.cpp",
    "line": 175,
    "lines": [
      "base::ubf::AbstractAction* BtBehavior::genAction(const base::ubf::AbstractState* const state,",
      "                                                 const double dt)",
      "{",
      "   const auto flightState = dynamic_cast<const FlightState*>(state);",
      "   if (flightState == nullptr) return nullptr;",
      "",
      "   // Ver a armadilha no cabecalho: reset() pode nunca chegar a um",
      "   // comportamento aninhado no Arbiter, entao a configuracao vinda dos",
      "   // slots e aplicada aqui, na primeira decisao.",
      "   if (!plansReady) {",
      "      configurePlans();",
      "      patrol.reset();",
      "      plansReady = true;",
      "   }",
      "",
      "   snap = flightState->snapshot();",
      "   if (!snap.valid) return nullptr;",
      "",
      "   frameDt = dt;",
      "   feedThreatPolicy(dt);",
      "",
      "   if (!treeBuilt) buildTree();",
      "   if (!treeValid) return nullptr;",
      "",
      "   currentDecision.reset();",
      "   tree.tickRoot();",
      "   if (!currentDecision.taken) return nullptr;",
      "",
      "   // Acao PRE-REF'd (o Agent chama unref() depois de executar) -- contrato",
      "   // do UBF: \"returns a pre-ref'd Action\".",
      "   const auto action = new FlightAction();",
      "   action->setCommand(currentDecision.command);",
      "   action->setLabel(currentDecision.label);",
      "   if (currentDecision.broadcastAlert) {",
      "      action->setAlertBroadcast(currentDecision.alertContactName,",
      "                                currentDecision.alertNorthM, currentDecision.alertEastM,",
      "                                currentDecision.alertAltitudeM, currentDecision.alertRangeM);",
      "   }",
      "   if (currentDecision.launchRequested) {",
      "      action->setLaunchRequest(currentDecision.launchTargetName);",
      "   }",
      "",
      "   // O voto do comportamento vai junto: e por ele que o UbfArbiter escolhe",
      "   // entre esta acao e a de outro comportamento no mesmo frame.",
      "   action->setVote(getVote());",
      "   return action;",
      "}"
    ],
    "trunc": false
  },
  "ContactDetectedCondition::tick": {
    "file": "models/players/A-4/src/bt/nodes/ContactDetectedCondition.cpp",
    "line": 14,
    "lines": [
      "//------------------------------------------------------------------------------",
      "// A condicao NAO e \"estou vendo o intruso agora\", e sim \"a manobra de evasao",
      "// esta valendo\" -- que continua true por alguns segundos depois de a pista",
      "// sumir (domain::ThreatPolicy::engaged()).",
      "//",
      "// E essa diferenca que impede a alternancia com o ramo de apoio: a propria",
      "// quebra tira o intruso do setor do radar (+-30 graus, contra uma quebra de",
      "// 110), entao \"vendo agora\" pisca -- e o ramo de baixo assumia, trazia a",
      "// aeronave de volta e ela reaquisitava. Resultado observado no Tacview:",
      "// aeronaves oscilando +-25 graus de banco, com periodo de ~24 s.",
      "//------------------------------------------------------------------------------",
      "BT::NodeStatus ContactDetectedCondition::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   return context_.behavior->threatPolicy().engaged() ? BT::NodeStatus::SUCCESS",
      "                                                      : BT::NodeStatus::FAILURE;",
      "}"
    ],
    "trunc": false
  },
  "PatrolAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/PatrolAction.cpp",
    "line": 13,
    "lines": [
      "BT::NodeStatus PatrolAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   auto& plan = context_.behavior->patrolPlan();",
      "   plan.advance(context_.behavior->getFrameDt());",
      "",
      "   context_.behavior->decision().take(plan.command(), \"PATROL\");",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  },
  "ReportAndEvadeAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/ReportAndEvadeAction.cpp",
    "line": 15,
    "lines": [
      "//------------------------------------------------------------------------------",
      "// O no NAO calcula a manobra: ele so entrega o comando que a politica fixou",
      "// na entrada da evasao (ver domain/ThreatPolicy.hpp -- o alvo e calculado uma",
      "// vez e mantido, para o piloto automatico ter para onde convergir).",
      "//",
      "// Dois rotulos, porque sao dois estados diferentes e vale ve-los no status:",
      "//    EVADE  -- quebrando COM o intruso na tela",
      "//    BREAK  -- terminando a quebra no arrasto da histerese, ja sem pista",
      "//------------------------------------------------------------------------------",
      "BT::NodeStatus ReportAndEvadeAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const domain::ThreatPolicy& policy{context_.behavior->threatPolicy()};",
      "   if (!policy.engaged()) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& snap = context_.behavior->snapshot();",
      "   FlightDecision& decision{context_.behavior->decision()};",
      "",
      "   decision.take(policy.command(), policy.contactLive() ? \"EVADE\" : \"BREAK\");",
      "",
      "   // O \"influencia os demais\": este no NAO alcanca outro player -- ele so",
      "   // marca o pedido. Quem transmite e o AlertDatalink, na fase 1 do frame",
      "   // seguinte, com a mensagem chegando aos outros como evento nativo.",
      "   //",
      "   // So se avisa o que se esta VENDO: no arrasto da histerese a posicao do",
      "   // contato ja e velha, e retransmiti-la manteria os outros convergindo",
      "   // para um ponto que nao vale mais.",
      "   if (policy.contactLive()) {",
      "      decision.broadcastAlert = true;",
      "      decision.alertContactName = snap.contactName;",
      "      decision.alertNorthM = snap.contactNorthM;",
      "      decision.alertEastM = snap.contactEastM;",
      "      decision.alertAltitudeM = snap.contactAltitudeM;",
      "      decision.alertRangeM = snap.contactRangeM;",
      "   }",
      "",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  },
  "ReturnToBaseAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/ReturnToBaseAction.cpp",
    "line": 13,
    "lines": [
      "BT::NodeStatus ReturnToBaseAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& snap = context_.behavior->snapshot();",
      "   auto& plan = context_.behavior->rtbPlan();",
      "",
      "   const domain::FlightCommand cmd{plan.command(snap.northM, snap.eastM, snap.headingDeg)};",
      "   const bool home{plan.arrived(snap.northM, snap.eastM)};",
      "",
      "   context_.behavior->decision().take(cmd, home ? \"HOME\" : \"RTB\");",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  },
  "SupportAlertAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/SupportAlertAction.cpp",
    "line": 14,
    "lines": [
      "BT::NodeStatus SupportAlertAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& snap = context_.behavior->snapshot();",
      "   if (!snap.hasAlert) return BT::NodeStatus::FAILURE;",
      "",
      "   // Reacao ao evento de OUTRO player: voa para a posicao que o alerta",
      "   // trouxe. O alerta nao mandou fazer isso -- ele so disse onde esta o",
      "   // intruso; a decisao de apoiar e desta aeronave.",
      "   domain::FlightCommand cmd;",
      "   cmd.headingDeg = domain::headingToDeg(snap.northM, snap.eastM,",
      "                                         snap.alertNorthM, snap.alertEastM);",
      "   cmd.altitudeM = snap.alertAltitudeM;",
      "   cmd.speedKts = context_.behavior->getSupportSpeedKts();",
      "",
      "   context_.behavior->decision().take(cmd, \"SUPPORT\");",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  },
  "FlightAction::execute": {
    "file": "models/players/A-4/src/ubf/FlightAction.cpp",
    "line": 129,
    "lines": [
      "bool FlightAction::execute(base::Component* actor)",
      "{",
      "   const auto player = dynamic_cast<models::Player*>(actor);",
      "   if (player == nullptr) return false;",
      "",
      "   base::Pair* const pilotPair{player->getPilotByType(typeid(models::Autopilot))};",
      "   const auto autopilot = (pilotPair != nullptr)",
      "                           ? dynamic_cast<models::Autopilot*>(pilotPair->object())",
      "                           : nullptr;",
      "   if (autopilot == nullptr) {",
      "      // Ate aqui esta falha era MUDA: o comportamento decidia, o arbitro",
      "      // escolhia, e a atuacao voltava 'false' sem nada em lugar nenhum --",
      "      // a aeronave simplesmente nao obedecia. Uma vez por player (ver",
      "      // firstTimeFor()).",
      "      static std::map<int, std::string> reported;",
      "      if (changedFor(reported, player->getID(), \"sem-autopilot\")) {",
      "         LOG(ERROR) << \"[FlightAction] \" << player->getName()->getString()",
      "                    << \": sem Autopilot -- decisao '\" << label << \"' nao pode ser atuada\";",
      "      }",
      "      return false;",
      "   }",
      "",
      "   // Estado ANTERIOR do quadro, lido antes de sobrescrever logo abaixo --",
      "   // e o que permite logar a TRANSICAO de comportamento (evento raro) em",
      "   // vez do comportamento corrente (50 Hz por aeronave).",
      "   const xboard::Readout before{xboard::get(player->getID())};",
      "",
      "   autopilot->setHeadingHoldMode(true);",
      "   autopilot->setAltitudeHoldMode(true);",
      "   autopilot->setVelocityHoldMode(true);",
      "",
      "   autopilot->setCommandedHeadingD(command.headingDeg);",
      "   autopilot->setCommandedAltitudeFt(command.altitudeM * base::distance::M2FT);",
      "   autopilot->setCommandedVelocityKts(command.speedKts);",
      "",
      "   // O quadro de leitura (libs/xboard) e a UNICA coisa que este modelo e o",
      "   // host compartilham: escrevemos aqui, o dump e a linha de status leem la.",
      "   // Ele mora numa .so de verdade justamente porque este codigo passou a rodar",
      "   // dentro de um plugin -- ver o cabecalho de libs/xboard/Board.hpp.",
      "   //",
      "   // Conta DECISAO, nao candidatura: estamos depois de o UbfArbiter ter",
      "   // escolhido o vencedor.",
      "   xboard::setBehaviorLabel(player->getID(), label);",
      "   xboard::bumpDecisionCount(player->getID());",
      "",
      "   // Transicao de comportamento -- o evento que conta a historia da missao",
      "   // (\"falcon1: PATROL -> EVADE\"). A primeira decisao de cada aeronave",
      "   // aparece como \"-- -> PATROL\", que e o valor inicial do quadro.",
      "   if (before.label != label) {",
      "      LOG(INFO) << \"[FlightAction] \" << player->getName()->getString()",
      "                << \": \" << before.label << \" -> \" << label",
      "                << \"  (hdg=\" << command.headingDeg",
      "                << \"deg alt=\" << command.altitudeM",
      "                << \"m vel=\" << command.speedKts << \"kt)\";",
      "   }",
      "",
      "   // Batimento: prova que a aeronave continua decidindo mesmo sem trocar",
      "   // de comportamento, e da a cadencia real de decisao. Cadenciado pela",
      "   // contagem do proprio quadro (ver kHeartbeatEveryDecisions).",
      "   if (before.decisions > 0 && (before.decisions % kHeartbeatEveryDecisions) == 0) {",
      "      LOG(DEBUG) << \"[FlightAction] \" << player->getName()->getString()",
      "                 << \": \" << before.decisions << \" decisoes atuadas, em '\" << label",
      "                 << \"' (thread \" << xboard::threadTag() << \")\";",
      "   }",
      "",
      "   // Qual thread decidiu -- unico ponto de atuacao comum aos DOIS agentes",
      "   // (o SimAgent nativo, background, e o FlightAgentTC, pool T/C), entao e",
      "   // aqui que o quadro fica correto pros dois: FlightAgentTC::controller()",
      "   // ja escreve o mesmo valor antes de chegar aqui (redundante, inofensivo,",
      "   // mesma tag); o SimAgent nunca escrevia nada -- o campo ficava preso em",
      "   // -1 (\"-\") pra sempre, nao porque a decisao nao tivesse thread, mas",
      "   // porque ninguem contava qual. threadTag() e por-thread (cache",
      "   // thread_local), entao aqui sai sempre a MESMA tag pras 4 aeronaves --",
      "   // resposta honesta: elas decidem, de fato, todas na mesma thread de",
      "   // background.",
      "   xboard::setThreadTag(player->getID(), xboard::threadTag());",
      "",
      "   // O pedido de broadcast fica LIGADO enquanto a aeronave evade -- e",
      "   // estado, nao evento. A linha de log sai so na BORDA: quando comeca a",
      "   // alertar, ou quando troca de contato (ver changedFor()). Sair do",
      "   // alerta zera a chave, entao um episodio novo volta a logar.",
      "   static std::map<int, std::string> lastAlertContact;",
      "   if (broadcast) {",
      "      const auto datalink = dynamic_cast<AlertDatalink*>(player->getDatalink());",
      "      if (datalink != nullptr) {",
      "         datalink->broadcastAlert(alertContactName, alertNorthM, alertEastM,",
      "                                  alertAltitudeM, alertRangeM);",
      "         // WARNING e nivel OPERACIONAL aqui, nao \"defeito de software\": e",
      "         // literalmente um alerta tatico saindo pro resto da esquadrilha, e",
      "         // e o que se quer enxergar destacado no meio das transicoes.",
      "         if (changedFor(lastAlertContact, player->getID(), alertContactName)) {",
      "            LOG(WARNING) << \"[FlightAction] \" << player->getName()->getString()",
      "                         << \": alerta tatico -- contato '\" << alertContactName",
      "                         << \"' a \" << (alertRangeM * base::distance::M2NM) << \" NM\";",
      "         }",
      "      }",
      "   } else {",
      "      changedFor(lastAlertContact, player->getID(), std::string{});",
      "   }",
      "",
      "   // --- lancamento de missil -------------------------------------------",
      "   //",
      "   // O UNICO ponto deste modelo que toca um objeto MIXR de arma. StoresMgr e",
      "   // opcional (getStoresManagement() devolve nullptr sem 'stores:' no EDL) --",
      "   // inerte em qualquer aviao de producao.",
      "   //",
      "   // releaseOneMissile() ja faz tudo que o framework nativo oferece: clona o",
      "   // 'missile' do EDL num flyout e o enfileira em Simulation::addNewPlayer()",
      "   // (materializado no proximo updatePlayerList(), no laco de background) --",
      "   // e assim, sem nenhum codigo nosso, que um player novo entra na simulacao",
      "   // EM EXECUCAO. Devolve pre-ref()'d (ver StoresMgr.hpp) -- por isso o",
      "   // unref() no fim.",
      "   if (launch) {",
      "      models::StoresMgr* const storesMgr{player->getStoresManagement()};",
      "      models::WorldModel* const world{player->getWorldModel()};",
      "      if (storesMgr != nullptr && world != nullptr) {",
      "         const auto target = dynamic_cast<models::Player*>(",
      "            world->findPlayerByName(launchTargetName.c_str()));",
      "         if (target != nullptr) {",
      "            models::AbstractWeapon* const flyout{storesMgr->releaseOneMissile()};",
      "            if (flyout != nullptr) {",
      "               flyout->setTargetPlayer(target, true);",
      "               LOG(INFO) << \"[FlightAction] \" << player->getName()->getString()",
      "                         << \": missil lancado contra '\" << launchTargetName",
      "                         << \"' (flyout '\" << flyout->getName()->getString() << \"')\";",
      "               flyout->unref();",
      "            } else {",
      "               // Pediu-se lancamento e o cabide esta vazio -- a arvore",
      "               // continuaria pedindo a cada frame sem nada acontecer.",
      "               LOG(WARNING) << \"[FlightAction] \" << player->getName()->getString()",
      "                            << \": lancamento pedido, mas releaseOneMissile() nao devolveu arma\";",
      "            }",
      "         } else {",
      "            LOG(WARNING) << \"[FlightAction] \" << player->getName()->getString()",
      "                         << \": lancamento pedido contra '\" << launchTargetName",
      "                         << \"', que nao existe na simulacao\";",
      "         }",
      "      }",
      "   }",
      "",
      "   return true;",
      "}"
    ],
    "trunc": false
  },
  "PyDecideAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/PyDecideAction.cpp",
    "line": 36,
    "lines": [
      "BT::NodeStatus PyDecideAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   if (!tentouCarregar_) {",
      "      tentouCarregar_ = true;",
      "      const BT::Optional<std::string> caminho{getInput<std::string>(\"script\")};",
      "      if (!caminho || caminho.value().empty()) {",
      "         LOG(ERROR) << \"[PyDecide] porta 'script' ausente ou vazia no XML da arvore\";",
      "      } else if (!mixr::xpyembed::isAvailable()) {",
      "         LOG(WARNING) << \"[PyDecide] sem interpretador Python -- o no fica inerte\";",
      "      } else {",
      "         scriptId_ = mixr::xpyembed::loadScript(caminho.value());",
      "      }",
      "   }",
      "   if (scriptId_ == 0) return BT::NodeStatus::FAILURE;",
      "",
      "   // A observacao na ordem canonica -- a MESMA macro do .onnx e do treino.",
      "   const domain::WorldView& snap{context_.behavior->snapshot()};",
      "   std::array<double, XRLBRIDGE_OBSERVATION_SIZE> entrada{};",
      "   {",
      "      int i{};",
      "#define XRLBRIDGE_F(nome) entrada[i++] = static_cast<double>(snap.nome);",
      "#define XRLBRIDGE_B(nome) entrada[i++] = snap.nome ? 1.0 : 0.0;",
      "      XRLBRIDGE_OBSERVATION_FIELDS",
      "#undef XRLBRIDGE_F",
      "#undef XRLBRIDGE_B",
      "   }",
      "",
      "   std::array<double, XRLBRIDGE_ACTION_SIZE> saida{};",
      "   if (!mixr::xpyembed::decide(scriptId_, instanciaId_,",
      "                               entrada.data(), static_cast<int>(entrada.size()),",
      "                               saida.data(), static_cast<int>(saida.size()))) {",
      "      return BT::NodeStatus::FAILURE;",
      "   }",
      "",
      "   domain::FlightCommand cmd;",
      "   cmd.headingDeg = saida[0];",
      "   cmd.altitudeM = saida[1];",
      "   cmd.speedKts = saida[2];",
      "",
      "   std::string rotulo{\"PY\"};",
      "   if (const BT::Optional<std::string> in{getInput<std::string>(\"label\")}) rotulo = in.value();",
      "",
      "   context_.behavior->decision().take(cmd, rotulo);",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  },
  "OnnxPolicyAction::tick": {
    "file": "models/players/A-4/src/bt/nodes/OnnxPolicyAction.cpp",
    "line": 30,
    "lines": [
      "BT::NodeStatus OnnxPolicyAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   if (!tentouAbrir_) {",
      "      tentouAbrir_ = true;",
      "      const BT::Optional<std::string> caminho{getInput<std::string>(\"model\")};",
      "      if (!caminho || caminho.value().empty()) {",
      "         LOG(ERROR) << \"[OnnxPolicy] porta 'model' ausente ou vazia no XML da arvore\";",
      "      } else {",
      "         modelId_ = mixr::xinfer::open(caminho.value());",
      "         if (modelId_ != 0) {",
      "            int nIn{}, nOut{};",
      "            if (mixr::xinfer::shape(modelId_, nIn, nOut)) {",
      "               // A forma e contrato, nao sugestao: 28 entrada, 3 saida. Um",
      "               // .onnx com outra forma foi treinado contra outra observacao",
      "               // ou outra acao, e comandar com ele seria pior que nao",
      "               // comandar.",
      "               if (nIn != XRLBRIDGE_OBSERVATION_SIZE || nOut != XRLBRIDGE_ACTION_SIZE) {",
      "                  LOG(ERROR) << \"[OnnxPolicy] '\" << caminho.value() << \"' tem forma \"",
      "                             << nIn << \"->\" << nOut << \", mas o contrato e \"",
      "                             << XRLBRIDGE_OBSERVATION_SIZE << \"->\" << XRLBRIDGE_ACTION_SIZE",
      "                             << \" (ver xrlbridge/ObservationFields.hpp)\";",
      "                  modelId_ = 0;",
      "               }",
      "            }",
      "         }",
      "      }",
      "   }",
      "   if (modelId_ == 0) return BT::NodeStatus::FAILURE;",
      "",
      "   // A observacao na ordem canonica -- a MESMA macro do treino.",
      "   const domain::WorldView& snap{context_.behavior->snapshot()};",
      "   std::array<float, XRLBRIDGE_OBSERVATION_SIZE> entrada{};",
      "   {",
      "      int i{};",
      "#define XRLBRIDGE_F(nome) entrada[i++] = static_cast<float>(snap.nome);",
      "#define XRLBRIDGE_B(nome) entrada[i++] = snap.nome ? 1.0F : 0.0F;",
      "      XRLBRIDGE_OBSERVATION_FIELDS",
      "#undef XRLBRIDGE_F",
      "#undef XRLBRIDGE_B",
      "   }",
      "",
      "   std::array<float, XRLBRIDGE_ACTION_SIZE> saida{};",
      "   const int escritos{mixr::xinfer::run(modelId_, entrada.data(),",
      "                                        static_cast<int>(entrada.size()),",
      "                                        saida.data(), static_cast<int>(saida.size()))};",
      "   if (escritos != XRLBRIDGE_ACTION_SIZE) return BT::NodeStatus::FAILURE;",
      "",
      "   bool normalizada{true};",
      "   if (const BT::Optional<bool> in{getInput<bool>(\"normalized\")}) normalizada = in.value();",
      "",
      "   domain::FlightCommand cmd;",
      "   if (normalizada) {",
      "      // Uma unica implementacao da desnormalizacao, em libs/xrlbridge, com",
      "      // os mesmos limites que o lado Python usa para montar o action_space.",
      "      const mixr::xrlbridge::Command c{mixr::xrlbridge::unscaleCommand(saida.data())};",
      "      cmd.headingDeg = c.headingDeg;",
      "      cmd.altitudeM = c.altitudeM;",
      "      cmd.speedKts = c.speedKts;",
      "   } else {",
      "      cmd.headingDeg = static_cast<double>(saida[0]);",
      "      cmd.altitudeM = static_cast<double>(saida[1]);",
      "      cmd.speedKts = static_cast<double>(saida[2]);",
      "   }",
      "",
      "   std::string rotulo{\"ONNX\"};",
      "   if (const BT::Optional<std::string> in{getInput<std::string>(\"label\")}) rotulo = in.value();",
      "",
      "   context_.behavior->decision().take(cmd, rotulo);",
      "   return BT::NodeStatus::SUCCESS;",
      "}"
    ],
    "trunc": false
  }
};
const flightSnip = (key) => (key ? (FLIGHT_SNIPPETS[key] || SNIPPETS[key] || null) : null);

/* Constantes de layout PRÓPRIAS desta aba -- deliberadamente maiores que    *
 * NW/NH/ROW/COL do resto da página: os cartões aqui carregam legendas mais *
 * longas (percepção/decisão/ação, "aciona Fulano") e um rótulo de CHAMADA  *
 * embaixo do nó ativo -- caber isso sem abreviar exigiu mais espaço.       */
const FNW = 250, FNH = 58, FROW = 78, FCOL = 300;

// Mesmo algoritmo de layout() (DFS, folha empilha, pai centraliza entre o    *
// primeiro e o último filho) -- não reaproveitado porque layout() fecha     *
// sobre as constantes GLOBAIS (NW/NH/ROW/COL); aqui as constantes são       *
// outras. Só orientação horizontal (a vertical não se aplica: sem "árvore   *
// vertical" nesta aba).
function flightLayout(root) {
  const nodes = []; let i = 0;
  (function place(n, depth) {
    const kids = n.children || [];
    const x = depth * FCOL;
    if (!kids.length) {
      const y = i * FROW;
      nodes.push({ ...n, x, y, depth });
      i += 1;
    } else {
      kids.forEach((k) => place(k, depth + 1));
      const f = nodes.find((m) => m.id === kids[0].id);
      const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
      const y = (f.y + l.y) / 2;
      nodes.push({ ...n, x, y, depth });
    }
  })(root, 0);
  return nodes;
}

// Miolo comum às 3 árvores desta aba (produção + as 2 variantes Python/ONNX    *
// mais abaixo) -- SÓ as folhas de "behavior" mudam entre elas; agente,        *
// estado, ação e autopilot são o MESMO nas 3 pocs (flight,                    *
// python-flight, onnx-policy só trocam o treeFile: do BtBehavior).
function flightSkeleton(behaviorChildren) {
  return N("agent", "FlightAgentTC", {
    edl: "agent:", sub: "extends AgentTC (decide na fase 3) -- um EXEMPLO de player concreto",
    note: "Único override necessário sobre AgentTC: filtrar a fase 3 e reescalar o dt para o do frame inteiro. O ciclo em si (controller()) é herdado, genérico -- qualquer player poderia estender AgentTC (ou Agent) do mesmo jeito.",
    children: [
      N("state", "FlightState", {
        edl: "state:", sub: "percepção -- implementa AbstractState (domain::WorldView, sem MIXR)",
        note: "AbstractState é a INTERFACE genérica de percepção do UBF -- FlightState é só UMA implementação possível, específica de aeronave. Qualquer player pode ter a sua própria.",
      }),
      N("behavior", "BtBehavior", {
        edl: "behavior:", sub: "decisão -- implementa AbstractBehavior (tick de uma árvore BehaviorTree.CPP)",
        note: "AbstractBehavior é a INTERFACE genérica de decisão do UBF -- BtBehavior é só UMA implementação possível (delega a uma árvore). Outra classe poderia decidir por tabela, por rede neural, por regra fixa, etc., sem mudar Agent/AgentTC.",
        children: behaviorChildren,
      }),
      N("action", "FlightAction", {
        edl: "-- efêmero --", sub: "ação -- implementa AbstractAction (nasce em genAction(), liberada em unref())",
        note: "AbstractAction é a INTERFACE genérica de atuação do UBF -- FlightAction é só UMA implementação possível (comanda um Autopilot). Um player sem piloto automático teria uma AbstractAction totalmente diferente.",
        children: [
          N("autopilot", "Autopilot", {
            edl: "pilot:", sub: "nativo mixr::models -- alvo comum a qualquer player com piloto automático",
            note: "setCommandedHeadingD/AltitudeFt/VelocityKts -- os três comandos que de fato chegam ao JSBSimModel via ap/heading_hold, ap/altitude_hold, ap/airspeed_hold.",
          }),
        ],
      }),
    ],
  });
}

// Deriva {all, byId, edges, ancestorsOf} de uma árvore -- extraído (em vez de   *
// ficar solto no escopo do módulo, como antes) porque agora HÁ TRÊS árvores:   *
// FLIGHT_TREE (produção, default) e as duas variantes logo abaixo. Cada        *
// entrada de FLIGHT_TRACES que declarar "tree:" faz FlightDecision() chamar    *
// isto de novo (activeIndex, via useMemo) em cima da árvore escolhida.
function buildFlightIndex(tree) {
  normalize(tree, null);
  const all = flat(tree);
  const byId = Object.fromEntries(all.map((n) => [n.id, n]));
  const edges = [];
  all.forEach((n) => (n.children || []).forEach((c) => edges.push([n.id, c.id])));
  const ancestorsOf = (id) => {
    const o = []; let c = byId[id];
    while (c && c.parent) { o.push([c.parent, c.id]); c = byId[c.parent]; }
    return o;
  };
  return { all, byId, edges, ancestorsOf };
}

const FLIGHT_TREE = flightSkeleton([
  N("fuelLow", "FuelLow", { edl: "flight_tree.xml", sub: "ramo 1 -- combustível abaixo de 5% aciona ReturnToBase" }),
  N("contact", "ContactDetected", { edl: "flight_tree.xml", sub: "ramo 2 -- evasão em curso (com histerese) aciona ReportAndEvade" }),
  N("alert", "AlertReceived", { edl: "flight_tree.xml", sub: "ramo 3 -- alerta recebido aciona SupportAlert" }),
  N("patrol", "Patrol", { edl: "flight_tree.xml", sub: "ramo 4 -- fallback incondicional" }),
]);
// Índice default (o de produção) -- os 3 nomes seguem existindo no escopo do   *
// módulo, do jeito que o resto do arquivo (fora de FlightDecision()) já        *
// espera; dentro do componente, quem lê é activeIndex (ver mais abaixo).
const FLIGHT_INDEX = buildFlightIndex(FLIGHT_TREE);
const flightById = FLIGHT_INDEX.byId;
const flightEdges = FLIGHT_INDEX.edges;
const flightAncestors = FLIGHT_INDEX.ancestorsOf;

// Duas variantes que trocam SÓ as folhas de "behavior", reaproveitando o        *
// MESMO flightSkeleton() (mesmo agente/estado/ação/autopilot) -- espelham       *
// src/poc/python-flight/configs/flight_tree_python.xml e                       *
// src/poc/onnx-policy/configs/flight_tree_onnx.xml, lidas por inteiro. Cada     *
// caixa funde Sequence(condição, ação) numa única folha, mesma convenção já    *
// usada acima para o FLIGHT_TREE de produção (ver o comentário perto de        *
// FLIGHT_BT_LEAVES, mais abaixo).
const FLIGHT_TREE_PYTHON = flightSkeleton([
  N("pyRtb", "PyDecideAction", { edl: "flight_tree_python.xml", sub: "ramo 1 -- combustível baixo (FuelLow) aciona policy/rtb.py via libs/xpyembed", runtime: "python" }),
  N("pyEvade", "PyDecideAction", { edl: "flight_tree_python.xml", sub: "ramo 2 -- evasão valendo (ContactDetected + ReportAndEvade) aciona policy/evade.py via libs/xpyembed", runtime: "python" }),
  N("pySupport", "PyDecideAction", { edl: "flight_tree_python.xml", sub: "ramo 3 -- alerta recebido (AlertReceived) aciona policy/support.py via libs/xpyembed", runtime: "python" }),
  N("pyPatrol", "PyDecideAction", { edl: "flight_tree_python.xml", sub: "ramo 4 -- nada acontecendo, sem condição -- aciona policy/patrol.py via libs/xpyembed", runtime: "python" }),
  N("patrolFallbackPy", "Patrol", { edl: "flight_tree_python.xml", sub: "degradação -- sem Python no sistema, script ausente/sem decide()/exceção: cai no Patrol nativo" }),
]);
const FLIGHT_TREE_ONNX = flightSkeleton([
  N("onnxPolicy", "OnnxPolicyAction", { edl: "flight_tree_onnx.xml", sub: "único ramo condicional -- infere policy_barrier.onnx via libs/xinfer (28 entradas -> 3 saídas)", runtime: "onnx" }),
  N("patrolFallbackOnnx", "Patrol", { edl: "flight_tree_onnx.xml", sub: "rede de segurança -- só roda se a inferência FALHAR (modelo ausente, forma errada, erro)" }),
]);

// Escopo da BehaviorTree.CPP (o "board" pedido): os ramos são, cada um,      *
// uma subclasse de BT::ConditionNode/BT::SyncActionNode -- código de         *
// TERCEIRO, não deste modelo. "behavior" (BtBehavior) fica DE FORA da faixa: *
// é dele o tree.tickRoot() que cruza a fronteira, mas a classe em si é do    *
// plugin (models/players/A-4), não da lib. Uma lista por árvore -- os ids    *
// das folhas mudam entre produção/Python/ONNX.
const FLIGHT_BTCPP_SCOPE = ["fuelLow", "contact", "alert", "patrol"];
const FLIGHT_BTCPP_SCOPE_PYTHON = ["pyRtb", "pyEvade", "pySupport", "pyPatrol", "patrolFallbackPy"];
const FLIGHT_BTCPP_SCOPE_ONNX = ["onnxPolicy", "patrolFallbackOnnx"];
// Escopo do framework UBF (mixr::base::ubf) -- o segundo "board" pedido.
// Deliberadamente SEM as folhas: a BehaviorTree.CPP é código de TERCEIRO
// (nem deste modelo, nem do MIXR) -- fica ao LADO do escopo UBF, não dentro
// dele, ainda que "behavior" (BtBehavior, papel AbstractBehavior) seja quem
// cruza a fronteira ao chamar tree.tickRoot(). "autopilot" também fica DE
// FORA: é o alvo nativo que a ação alcança, não parte do ciclo UBF em si.
// Os 4 ids (agent/state/behavior/action) são os MESMOS nas 3 árvores -- só
// as folhas sob "behavior" mudam -- então um escopo só serve para as 3.
const FLIGHT_UBF_SCOPE = ["agent", "state", "behavior", "action"];

/* ---------- Classe: dados que o extrator NÃO cobre (models/players/A-4/) ---------- *
 * tools/extract_execution_chain.py só varre contexts/src/mixr/, então as      *
 * classes deste modelo não têm entrada em MODEL. Preenchido à mão, uma vez,   *
 * a partir do PRÓPRIO header (DECLARE_SUBCLASS + BEGIN_SLOTTABLE) -- mesma    *
 * curadoria manual-sobre-dado-real já usada para EDL_TEXT/FLIGHT_SNIPPETS.    *
 * O "ch" de cada uma já EMENDA no "ch" real de MODEL (AgentTC/AbstractState/  *
 * AbstractBehavior/AbstractAction, todos de fato extraídos, com slots reais   *
 * como state/behavior em Agent e vote em AbstractBehavior) -- então andar a   *
 * cadeia inteira (flightChainOf) mistura dado curado e dado extraído sem      *
 * costura visível. */
const FLIGHT_MODEL = {
  FlightAgentTC: { b: "AgentTC", ch: ["FlightAgentTC", "AgentTC", "Agent", "Component"], sl: [], own: 0,
    f: "FlightAgentTC", r: true, m: "models/players/A-4",
    hd: "models/players/A-4/include/xnative/FlightAgentTC.hpp", src: "models/players/A-4/src/xnative/FlightAgentTC.cpp" },
  FlightState: { b: "AbstractState", ch: ["FlightState", "AbstractState", "Component"], sl: [], own: 0,
    f: "FlightState", r: true, m: "models/players/A-4",
    hd: "models/players/A-4/include/ubf/FlightState.hpp", src: "models/players/A-4/src/ubf/FlightState.cpp" },
  BtBehavior: { b: "AbstractBehavior", ch: ["BtBehavior", "AbstractBehavior", "Component"],
    sl: ["treeFile", "patrolHeading", "legTime", "legTurn", "patrolAltitude", "patrolSpeed", "rtbAltitude", "rtbSpeed", "arrivalRadius", "fuelReserve", "breakTurn", "evadeClimb", "evadeSpeed", "evadeHold", "supportSpeed", "terrainClearance", "launchMinRange", "launchMaxRange", "launchCone", "patrolJitterHeading", "patrolMasterSeed", "patrolSeedOverride"], own: 22,
    f: "BtBehavior", r: true, m: "models/players/A-4",
    hd: "models/players/A-4/include/ubf/BtBehavior.hpp", src: "models/players/A-4/src/ubf/BtBehavior.cpp" },
  FlightAction: { b: "AbstractAction", ch: ["FlightAction", "AbstractAction"], sl: [], own: 0,
    f: "FlightAction", r: true, m: "models/players/A-4",
    hd: "models/players/A-4/include/ubf/FlightAction.hpp", src: "models/players/A-4/src/ubf/FlightAction.cpp" },
};
// Tipo de cada slot -- só BtBehavior tem slots próprios nesta curadoria;      *
// o resto do catálogo (MODEL) não anota tipo, então esta é uma camada extra  *
// só aqui, opcional na hora de renderizar.
const FLIGHT_SLOT_TYPES = { BtBehavior: {"treeFile": "String", "patrolHeading": "Angle", "legTime": "Time", "legTurn": "Angle", "patrolAltitude": "Distance", "patrolSpeed": "Number", "rtbAltitude": "Distance", "rtbSpeed": "Number", "arrivalRadius": "Distance", "fuelReserve": "Number", "breakTurn": "Angle", "evadeClimb": "Distance", "evadeSpeed": "Number", "evadeHold": "Time", "supportSpeed": "Number", "terrainClearance": "Distance", "launchMinRange": "Distance", "launchMaxRange": "Distance", "launchCone": "Angle", "patrolJitterHeading": "Angle", "patrolMasterSeed": "Number", "patrolSeedOverride": "Number"} };

const flightEntry = (cls) => FLIGHT_MODEL[cls] || MODEL[cls] || null;
const flightChainOf = (cls) => { const e = flightEntry(cls); return e ? e.ch : [cls]; };
const flightAllSlotsOf = (cls) => {
  const out = [];
  flightChainOf(cls).forEach((a) => { const e = flightEntry(a); (e ? e.sl : []).forEach((s) => out.push([s, a])); });
  return out;
};
const flightSlotType = (fromClass, slotName) => (FLIGHT_SLOT_TYPES[fromClass] || {})[slotName] || null;
const flightModuleOf = (cls) => { const e = flightEntry(cls); return e ? (e.m || "base") : "?"; };
const flightFactoryOf = (cls) => (FLIGHT_MODEL[cls] ? (FLIGHT_MODEL[cls].f || cls) : factoryOf(cls));

// As 4 folhas da árvore não são objetos MIXR (não têm "slot" nenhum) --      *
// cada uma mistura uma CONDIÇÃO e uma AÇÃO da BehaviorTree.CPP (a árvore     *
// real usa Sequence[condição, ação]; esta aba funde os dois numa única       *
// caixa por simplicidade visual). O mecanismo próprio da lib é "port"        *
// (par chave/valor lido do atributo XML do nó, via providedPorts()) -- só    *
// FuelLowCondition declara um de verdade (margin).
const FLIGHT_BT_LEAVES = {
  fuelLow: {
    cond: { cls: "FuelLowCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/FuelLowCondition.hpp", ports: [["margin", "double"]] },
    act: { cls: "ReturnToBaseAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/ReturnToBaseAction.hpp", ports: [] },
  },
  contact: {
    cond: { cls: "ContactDetectedCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/ContactDetectedCondition.hpp", ports: [] },
    act: { cls: "ReportAndEvadeAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/ReportAndEvadeAction.hpp", ports: [] },
  },
  alert: {
    cond: { cls: "AlertReceivedCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/AlertReceivedCondition.hpp", ports: [] },
    act: { cls: "SupportAlertAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/SupportAlertAction.hpp", ports: [] },
  },
  patrol: {
    cond: null,
    act: { cls: "PatrolAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PatrolAction.hpp", ports: [] },
  },
  // -------- src/poc/python-flight (flight_tree_python.xml) --------
  pyRtb: {
    cond: { cls: "FuelLowCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/FuelLowCondition.hpp", ports: [["margin", "double"]] },
    act: { cls: "PyDecideAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PyDecideAction.hpp", ports: [["script", "std::string"], ["label", "std::string"]],
      delegate: { lib: "libs/xpyembed", to: "mixr::xpyembed::decide()", cost: "~42 µs/decisão (medido, ver CLAUDE.md)", fail: "sem interpretador Python, script ausente/sem decide()/exceção -> FAILURE, o Fallback cai no próximo ramo" } },
  },
  pyEvade: {
    cond: { cls: "ContactDetectedCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/ContactDetectedCondition.hpp", ports: [] },
    act: { cls: "PyDecideAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PyDecideAction.hpp", ports: [["script", "std::string"], ["label", "std::string"]],
      delegate: { lib: "libs/xpyembed", to: "mixr::xpyembed::decide()", cost: "~42 µs/decisão (medido, ver CLAUDE.md)", fail: "sem interpretador Python, script ausente/sem decide()/exceção -> FAILURE, o Fallback cai no próximo ramo" } },
  },
  pySupport: {
    cond: { cls: "AlertReceivedCondition", base: "BT::ConditionNode", hd: "models/players/A-4/include/bt/nodes/AlertReceivedCondition.hpp", ports: [] },
    act: { cls: "PyDecideAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PyDecideAction.hpp", ports: [["script", "std::string"], ["label", "std::string"]],
      delegate: { lib: "libs/xpyembed", to: "mixr::xpyembed::decide()", cost: "~42 µs/decisão (medido, ver CLAUDE.md)", fail: "sem interpretador Python, script ausente/sem decide()/exceção -> FAILURE, o Fallback cai no próximo ramo" } },
  },
  pyPatrol: {
    cond: null,
    act: { cls: "PyDecideAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PyDecideAction.hpp", ports: [["script", "std::string"], ["label", "std::string"]],
      delegate: { lib: "libs/xpyembed", to: "mixr::xpyembed::decide()", cost: "~42 µs/decisão (medido, ver CLAUDE.md)", fail: "sem interpretador Python, script ausente/sem decide()/exceção -> FAILURE, o Fallback cai no Patrol nativo (degradação)" } },
  },
  patrolFallbackPy: {
    cond: null,
    act: { cls: "PatrolAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PatrolAction.hpp", ports: [] },
  },
  // -------- src/poc/onnx-policy (flight_tree_onnx.xml) --------
  onnxPolicy: {
    cond: null,
    act: { cls: "OnnxPolicyAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/OnnxPolicyAction.hpp", ports: [["model", "std::string"], ["normalized", "bool"], ["label", "std::string"]],
      delegate: { lib: "libs/xinfer", to: "mixr::xinfer::run()", cost: "~50,1 µs/inferência (medido, ver CLAUDE.md)", fail: "modelo ausente, forma diferente de 28->3 ou erro de inferência -> FAILURE, o Fallback cai no Patrol nativo (a \"rede de segurança\")" } },
  },
  patrolFallbackOnnx: {
    cond: null,
    act: { cls: "PatrolAction", base: "BT::SyncActionNode", hd: "models/players/A-4/include/bt/nodes/PatrolAction.hpp", ports: [] },
  },
};

/* EDL real (condensado) de src/poc/dis/flight/configs/scenario.edl.in --      *
 * dois trechos do MESMO falcon1 (pilot:/agent: não são vizinhos no arquivo real,   *
 * há dezenas de linhas de outros sistemas entre os dois -- omitidas e marcadas     *
 * abaixo, mesma prática já usada por EDL_TEXT/SCENARIO). */
const FLIGHT_EDL_TEXT = `falcon1: ( Aircraft
   side: blue   type: "A4"   id: 101
   components: {
      // ... dynamics/navegacao/datalink/radar/track manager omitidos ...

      pilot: ( Autopilot
         navMode: false
         headingHoldMode:  true
         altitudeHoldMode: true
         velocityHoldMode: true
         maxRateOfTurnDps: 6.0
         maxBankAngle:    45.0
         maxPitchAngle:   20.0
         maxClimbRateMps:  40.0
         maxAcceleration:  6.0
      )

      // ... stores/colisao omitidos ...

      agent: ( FlightAgentTC
         state: ( FlightState )
         behavior: ( BtBehavior
            treeFile: "./dist/share/mixr-plugins/flight/flight_tree.xml"
            patrolHeading:  ( Degrees 90 )
            legTime:        ( Seconds 60 )
            legTurn:        ( Degrees 90 )
            patrolAltitude: ( Meters 1750 )
            patrolSpeed:    350.0
            rtbAltitude:    ( Meters 2050 )
            rtbSpeed:       380.0
            arrivalRadius:  ( NauticalMiles 2.0 )
            fuelReserve:    0.35
            breakTurn:      ( Degrees 110 )
            evadeClimb:     ( Meters 700 )
            evadeSpeed:     420.0
            supportSpeed:   400.0
            evadeHold:      ( Seconds 30 )
            terrainClearance: ( Meters 800 )
            patrolJitterHeading: ( Degrees 6 )
            patrolMasterSeed:    20260903
         )
      )
   }
)`.split("\n");
const FLIGHT_EDL_RANGE = {
  agent: [20, 42], state: [21, 21], behavior: [22, 41], autopilot: [6, 16],
};
// Um segundo parâmetro opcional (default = a de produção) -- as trilhas
// Python/ONNX passam FLIGHT_EDL_RANGE_PYTHON/_ONNX, cujos números de linha
// divergem só porque o excerto delas não tem patrolJitterHeading/
// patrolMasterSeed (a produção tem; python-flight/onnx-policy não).
const flightEdlRangeFor = (id, rangeMap = FLIGHT_EDL_RANGE) => rangeMap[id] || rangeMap.agent;

/* EDL real (condensado) de src/poc/python-flight/configs/scenario.edl.in --   *
 * mesmo falcon1, mesmo pilot:/agent: da produção -- só o treeFile: muda      *
 * (flight_tree_python.xml) e faltam patrolJitterHeading/patrolMasterSeed     *
 * (esta poc não os declara). */
const FLIGHT_EDL_TEXT_PYTHON = `falcon1: ( Aircraft
   side: blue   type: "A4"   id: 101
   components: {
      // ... dynamics/navegacao/datalink/radar/track manager omitidos ...

      pilot: ( Autopilot
         navMode: false
         headingHoldMode:  true
         altitudeHoldMode: true
         velocityHoldMode: true
         maxRateOfTurnDps: 6.0
         maxBankAngle:    45.0
         maxPitchAngle:   20.0
         maxClimbRateMps:  40.0
         maxAcceleration:  6.0
      )

      // ... stores/colisao omitidos ...

      agent: ( FlightAgentTC
         state: ( FlightState )
         behavior: ( BtBehavior
            treeFile: "./src/poc/python-flight/configs/flight_tree_python.xml"
            patrolHeading:  ( Degrees 90 )
            legTime:        ( Seconds 60 )
            legTurn:        ( Degrees 90 )
            patrolAltitude: ( Meters 1750 )
            patrolSpeed:    350.0
            rtbAltitude:    ( Meters 2050 )
            rtbSpeed:       380.0
            arrivalRadius:  ( NauticalMiles 2.0 )
            fuelReserve:    0.35
            breakTurn:      ( Degrees 110 )
            evadeClimb:     ( Meters 700 )
            evadeSpeed:     420.0
            supportSpeed:   400.0
            evadeHold:      ( Seconds 30 )
            terrainClearance: ( Meters 800 )
         )
      )
   }
)`.split("\n");
const FLIGHT_EDL_RANGE_PYTHON = {
  agent: [20, 40], state: [21, 21], behavior: [22, 39], autopilot: [6, 16],
};

/* EDL real (condensado) de src/poc/onnx-policy/configs/scenario.edl.in --     *
 * mesmíssima forma da de python-flight -- só o treeFile: muda                *
 * (flight_tree_onnx.xml). */
const FLIGHT_EDL_TEXT_ONNX = `falcon1: ( Aircraft
   side: blue   type: "A4"   id: 101
   components: {
      // ... dynamics/navegacao/datalink/radar/track manager omitidos ...

      pilot: ( Autopilot
         navMode: false
         headingHoldMode:  true
         altitudeHoldMode: true
         velocityHoldMode: true
         maxRateOfTurnDps: 6.0
         maxBankAngle:    45.0
         maxPitchAngle:   20.0
         maxClimbRateMps:  40.0
         maxAcceleration:  6.0
      )

      // ... stores/colisao omitidos ...

      agent: ( FlightAgentTC
         state: ( FlightState )
         behavior: ( BtBehavior
            treeFile: "./src/poc/onnx-policy/configs/flight_tree_onnx.xml"
            patrolHeading:  ( Degrees 90 )
            legTime:        ( Seconds 60 )
            legTurn:        ( Degrees 90 )
            patrolAltitude: ( Meters 1750 )
            patrolSpeed:    350.0
            rtbAltitude:    ( Meters 2050 )
            rtbSpeed:       380.0
            arrivalRadius:  ( NauticalMiles 2.0 )
            fuelReserve:    0.35
            breakTurn:      ( Degrees 110 )
            evadeClimb:     ( Meters 700 )
            evadeSpeed:     420.0
            supportSpeed:   400.0
            evadeHold:      ( Seconds 30 )
            terrainClearance: ( Meters 800 )
         )
      )
   }
)`.split("\n");
const FLIGHT_EDL_RANGE_ONNX = {
  agent: [20, 40], state: [21, 21], behavior: [22, 39], autopilot: [6, 16],
};

const FLIGHT_STAGES = [
  { n: 0, label: "Percepção" },
  { n: 1, label: "Decisão" },
  { n: 2, label: "Ação" },
];

/* ---------- as duas trilhas: mesmo esqueleto de passo do resto da página ---------- */

function traceFlightPatrol() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length });
  p({ node: "agent", src: "AgentTC::updateTC", hl: [2, 2], stage: null, call: "controller(dt)",
    title: "AgentTC::updateTC(dt) → controller(dt)",
    body: "Chamado pelo passeio de componentes da fase 3 (Component::updateTC() desce até aqui, dentro do próprio ( Aircraft )). O método do framework é trivial -- só repassa para o controller() virtual, que por polimorfismo cai na versão que FlightAgentTC sobrescreve." });
  p({ node: "agent", src: "FlightAgentTC::controller", hl: [12, 24], stage: null, call: "BaseClass::controller(dt * 4.0)",
    title: "FlightAgentTC::controller(dt) -- o gate de fase",
    body: "Este componente é chamado 4x por frame (uma por fase, dt/4 cada) -- só age quando world->phase()==3. Antes de repassar, grava qual thread do pool decidiu (só para a coluna de status). BaseClass::controller(dt*4.0) reconstrói o dt do FRAME INTEIRO antes de entrar no ciclo genérico: histerese e planos de patrulha precisam dele por completo, não de uma fatia de fase.",
    warn: "Sem esse filtro de fase, o mesmo ciclo rodaria 4x por frame -- a MESMA decisão repetida." });
  p({ node: "agent", src: "Agent::controller", hl: [7, 7], stage: 0, call: "state->updateState(actor)",
    title: "Agent::controller(dt) -- state->updateState(actor)",
    body: "A partir daqui é o ciclo GENÉRICO do framework (mixr::base::ubf::Agent), o mesmo usado por QUALQUER agente UBF -- SimAgent ou AgentTC, deste modelo ou de outro qualquer. Primeiro perceber, só depois decidir." });
  p({ node: "state", src: "FlightState::updateState", hl: [13, 20], stage: 0, call: "updateState(actor)",
    title: "FlightState::updateState() -- percepção",
    body: "Lê só acessores nativos do Player/AirVehicle (posição, atitude, velocidade) e preenche um domain::WorldView puro, sem nenhum tipo do MIXR dentro -- é o que permite testar a árvore de comportamento isolada de qualquer Station." });
  p({ node: "agent", src: "Agent::controller", hl: [10, 10], stage: 1, call: "behavior->genAction(state, dt)",
    title: "Agent::controller(dt) -- behavior->genAction(state, dt)",
    body: "Com o estado atualizado, a vez é do comportamento plugado -- aqui, uma árvore de comportamento (BtBehavior)." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [15, 26], stage: 1, call: "tree.tickRoot()",
    title: "BtBehavior::genAction() -- tree.tickRoot()",
    body: "Copia o snapshot, envelhece a histerese de evasão (feedThreatPolicy) e dispara o tick real da BehaviorTree.CPP sobre um Fallback de 4 ramos -- o primeiro que suceder vence." });
  p({ node: "fuelLow", src: null, stage: 1, call: "FuelLow::tick()",
    title: "FuelLow (margin=0.05) -- FAILURE",
    body: "Primeiro ramo do Fallback: combustível acima da reserva de 5%. Condição falha, tickRoot() tenta o próximo ramo." });
  p({ node: "contact", src: "ContactDetectedCondition::tick", hl: [11, 17], stage: 1, call: "tick()",
    title: "ContactDetected -- FAILURE (sem contato ativo)",
    body: "threatPolicy().engaged() == false: não há alvo nem histerese de evasão pendente. Segundo ramo também falha." });
  p({ node: "alert", src: null, stage: 1, call: "AlertReceived::tick()",
    title: "AlertReceived -- FAILURE (sem alerta pendente)",
    body: "hasAlert == false: nenhum outro caça da esquadrilha avisou nada neste frame. Terceiro ramo falha." });
  p({ node: "patrol", src: "PatrolAction::tick", hl: [0, 9], stage: 1, call: "tick()",
    title: "Patrol -- SUCCESS (fallback incondicional)",
    body: "Sem condição alguma: SEMPRE sucede. decision().take(plan.command(), 'PATROL') grava o comando desta vez -- e dá o rótulo bt=PATROL no dump/status." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [30, 32], stage: 1, call: "new FlightAction(); action->setCommand(...)",
    title: "BtBehavior::genAction() -- new FlightAction() (pré-referenciada)",
    body: "tickRoot() retornou com currentDecision.taken=true. genAction() cria a FlightAction, copia comando e rótulo, e devolve pré-referenciada -- contrato do UBF." });
  p({ node: "agent", src: "Agent::controller", hl: [12, 13], stage: 2, call: "action->execute(actor)",
    title: "Agent::controller(dt) -- action->execute(actor)",
    body: "A FlightAction retornada por genAction() é executada AQUI -- e só aqui que ela de fato toca o Autopilot." });
  p({ node: "action", src: "FlightAction::execute", hl: [2, 9], stage: 2, call: "player->getPilotByType(typeid(Autopilot))",
    title: "FlightAction::execute() -- resolve o Autopilot",
    body: "O ator chega por PARÂMETRO -- a ação nunca guarda ponteiro pra aeronave. getPilotByType(typeid(Autopilot)) acha o piloto automático; sem ele, a decisão não pode ser atuada (LOG(ERROR), uma vez por player, via um detector de borda)." });
  p({ node: "autopilot", src: "FlightAction::execute", hl: [27, 33], stage: 2, call: "setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    title: "autopilot->setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    body: "Os três comandos que de fato chegam ao JSBSimModel (via ap/heading_hold, ap/altitude_hold, ap/airspeed_hold). Gotcha de unidade: setCommandedAltitudeFt() é em PÉS -- domain::FlightCommand.altitudeM fica em METROS até esta linha; a conversão (M2FT) acontece exatamente aqui, na fronteira.",
    warn: "É o ÚNICO ponto desta cadeia inteira que é de fato código NATIVO do framework sendo comandado -- tudo antes disso é código deste modelo (models/players/A-4)." });
  p({ node: "action", src: "FlightAction::execute", hl: [42, 54], stage: 2, call: "xboard::setBehaviorLabel(...); LOG(INFO)",
    title: "FlightAction::execute() -- xboard + log de transição",
    body: "xboard::setBehaviorLabel()/bumpDecisionCount() publicam o rótulo pro dashboard/dump (dec=). O log só registra a TRANSIÇÃO (before.label != label) -- não a cada tick, senão seria uma linha a 50 Hz por avião." });
  p({ node: "agent", src: "Agent::controller", hl: [13, 13], stage: 2, call: "action->unref()",
    title: "action->unref() -- a ação é liberada",
    body: "A FlightAction é efêmera: nasceu em genAction(), atuou em execute(actor), e é liberada AQUI, no mesmo ciclo -- nunca fica guardada em lugar nenhum." });
  return st;
}

function traceFlightEvade() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length });
  p({ node: "agent", src: "AgentTC::updateTC", hl: [2, 2], stage: null, call: "controller(dt)",
    title: "AgentTC::updateTC(dt) → controller(dt)",
    body: "Chamado pelo passeio de componentes da fase 3 (Component::updateTC() desce até aqui, dentro do próprio ( Aircraft )). O método do framework é trivial -- só repassa para o controller() virtual, que por polimorfismo cai na versão que FlightAgentTC sobrescreve." });
  p({ node: "agent", src: "FlightAgentTC::controller", hl: [12, 24], stage: null, call: "BaseClass::controller(dt * 4.0)",
    title: "FlightAgentTC::controller(dt) -- o gate de fase",
    body: "Este componente é chamado 4x por frame (uma por fase, dt/4 cada) -- só age quando world->phase()==3. Antes de repassar, grava qual thread do pool decidiu (só para a coluna de status). BaseClass::controller(dt*4.0) reconstrói o dt do FRAME INTEIRO antes de entrar no ciclo genérico.",
    warn: "Sem esse filtro de fase, o mesmo ciclo rodaria 4x por frame -- a MESMA decisão repetida." });
  p({ node: "agent", src: "Agent::controller", hl: [7, 7], stage: 0, call: "state->updateState(actor)",
    title: "Agent::controller(dt) -- state->updateState(actor)",
    body: "Ciclo GENÉRICO do framework, o mesmo usado por QUALQUER agente UBF -- SimAgent ou AgentTC, deste modelo ou de outro qualquer." });
  p({ node: "state", src: "FlightState::updateState", hl: [47, 60], stage: 0, call: "updateState(actor)",
    title: "FlightState::updateState() -- desta vez com um contato",
    body: "xtrack::nearestHostileTrack() achou uma pista hostil: hasContact=true, com alcance/marcação relativa e a posição absoluta do contato já calculada (soma da própria posição com o relativo da pista)." });
  p({ node: "agent", src: "Agent::controller", hl: [10, 10], stage: 1, call: "behavior->genAction(state, dt)",
    title: "Agent::controller(dt) -- behavior->genAction(state, dt)",
    body: "Com o estado atualizado (agora com contato), a vez é da árvore de comportamento." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [15, 26], stage: 1, call: "tree.tickRoot()",
    title: "BtBehavior::genAction() -- tree.tickRoot()",
    body: "Mesmo tick de sempre -- é o CONTEÚDO do snapshot que muda o resultado, não o código." });
  p({ node: "fuelLow", src: null, stage: 1, call: "FuelLow::tick()",
    title: "FuelLow (margin=0.05) -- FAILURE",
    body: "Combustível acima da reserva. Primeiro ramo falha, tickRoot() tenta o próximo." });
  p({ node: "contact", src: "ContactDetectedCondition::tick", hl: [11, 17], stage: 1, call: "tick()",
    title: "ContactDetected -- SUCCESS (threatPolicy().engaged())",
    body: "Não é 'estou vendo o intruso agora' -- é 'a manobra de evasão está valendo', que continua true por evadeHold segundos DEPOIS de a pista sumir. É essa histerese que evita a alternância com o ramo de apoio (a própria quebra tira o intruso do setor do radar)." });
  p({ node: "contact", src: "ReportAndEvadeAction::tick", hl: [9, 19], stage: 1, call: "decision.take(cmd, label)",
    title: "ReportAndEvade -- decision.take(cmd, 'EVADE'/'BREAK')",
    body: "O nó NÃO calcula a manobra -- só entrega o comando que domain::ThreatPolicy já fixou na entrada da evasão. O rótulo (EVADE/BREAK) vem de policy.contactLive().",
    warn: "AlertReceived e Patrol (ramos 3 e 4) NUNCA são avaliados neste ciclo -- o Fallback é curto-circuito: o primeiro ramo que suceder vence, os demais nem chegam a tickar." });
  p({ node: "contact", src: "ReportAndEvadeAction::tick", hl: [28, 35], stage: 1, call: "decision.broadcastAlert = true",
    title: "ReportAndEvade -- decision.broadcastAlert = true",
    body: "Se o contato ainda está VIVO (não só no arrasto da histerese), marca o pedido de alerta tático -- separado de take(), que nunca limpa essa flag. É o que FlightAction::execute() vai ler mais adiante." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [30, 37], stage: 1, call: "action->setAlertBroadcast(...)",
    title: "BtBehavior::genAction() -- new FlightAction() + setAlertBroadcast(...)",
    body: "Além de comando e rótulo, desta vez a ação também carrega o pedido de alerta -- setAlertBroadcast() com a posição do contato." });
  p({ node: "agent", src: "Agent::controller", hl: [12, 13], stage: 2, call: "action->execute(actor)",
    title: "Agent::controller(dt) -- action->execute(actor)",
    body: "A FlightAction (agora com o pedido de alerta junto) é executada AQUI." });
  p({ node: "action", src: "FlightAction::execute", hl: [2, 9], stage: 2, call: "player->getPilotByType(typeid(Autopilot))",
    title: "FlightAction::execute() -- resolve o Autopilot",
    body: "Mesmo caminho de sempre: getPilotByType(typeid(Autopilot)) acha o piloto automático do ator recebido por parâmetro." });
  p({ node: "autopilot", src: "FlightAction::execute", hl: [27, 33], stage: 2, call: "setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    title: "autopilot->setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    body: "O MESMO código do caminho de patrulha -- o Autopilot não sabe (nem precisa saber) que o comando agora vem da manobra de evasão. Quem calculou o rumo de fuga foi domain::ThreatPolicy, mais atrás." });
  p({ node: "action", src: "FlightAction::execute", hl: [81, 98], stage: 2, call: "datalink->broadcastAlert(...)",
    title: "FlightAction::execute() -- broadcastAlert() enfileira a transmissão",
    body: "Isto só ENFILEIRA: a transmissão de verdade sai na fase 1 do PRÓXIMO frame, via AlertDatalink, chegando aos outros caças como evento nativo do MIXR.",
    warn: "O LOG(WARNING) aqui só dispara na BORDA (mudança de contato) -- não a cada tick enquanto a evasão continua." });
  p({ node: "agent", src: "Agent::controller", hl: [13, 13], stage: 2, call: "action->unref()",
    title: "action->unref() -- a ação é liberada",
    body: "Efêmera: nasceu em genAction(), atuou em execute(actor), e é liberada AQUI, no mesmo ciclo." });
  return st;
}

/* Duas trilhas a mais, sobre FLIGHT_TREE_PYTHON/FLIGHT_TREE_ONNX (não mais    *
 * FLIGHT_TREE) -- espelham src/poc/python-flight e src/poc/onnx-policy: o     *
 * MESMO agente/estado/ação/autopilot de sempre, só a folha de decisão sob     *
 * "behavior" delega para fora do plugin (libs/xpyembed / libs/xinfer). */

function traceFlightPython() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length });
  p({ node: "agent", src: "AgentTC::updateTC", hl: [2, 2], stage: null, call: "controller(dt)",
    title: "AgentTC::updateTC(dt) → controller(dt)",
    body: "Chamado pelo passeio de componentes da fase 3 (Component::updateTC() desce até aqui, dentro do próprio ( Aircraft )). O método do framework é trivial -- só repassa para o controller() virtual, que por polimorfismo cai na versão que FlightAgentTC sobrescreve." });
  p({ node: "agent", src: "FlightAgentTC::controller", hl: [12, 24], stage: null, call: "BaseClass::controller(dt * 4.0)",
    title: "FlightAgentTC::controller(dt) -- o gate de fase",
    body: "Este componente é chamado 4x por frame (uma por fase, dt/4 cada) -- só age quando world->phase()==3. BaseClass::controller(dt*4.0) reconstrói o dt do FRAME INTEIRO antes de entrar no ciclo genérico -- o MESMO código de sempre, não importa se a folha da árvore é C++, Python ou ONNX.",
    warn: "Sem esse filtro de fase, o mesmo ciclo rodaria 4x por frame -- a MESMA decisão repetida." });
  p({ node: "agent", src: "Agent::controller", hl: [7, 7], stage: 0, call: "state->updateState(actor)",
    title: "Agent::controller(dt) -- state->updateState(actor)",
    body: "Ciclo GENÉRICO do framework (mixr::base::ubf::Agent) -- idêntico nas 3 pocs (produção, Python, ONNX); só a folha sob 'behavior' muda entre elas." });
  p({ node: "state", src: "FlightState::updateState", hl: [13, 20], stage: 0, call: "updateState(actor)",
    title: "FlightState::updateState() -- percepção",
    body: "Lê só acessores nativos do Player/AirVehicle e preenche um domain::WorldView puro -- os MESMOS 28 campos, na MESMA ordem canônica de xrlbridge/ObservationFields.hpp, que PyDecideAction empacota para o script logo mais." });
  p({ node: "agent", src: "Agent::controller", hl: [10, 10], stage: 1, call: "behavior->genAction(state, dt)",
    title: "Agent::controller(dt) -- behavior->genAction(state, dt)",
    body: "Com o estado atualizado, a vez é do comportamento plugado -- a MESMA classe BtBehavior de sempre; treeFile: aqui aponta para flight_tree_python.xml." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [15, 26], stage: 1, call: "tree.tickRoot()",
    title: "BtBehavior::genAction() -- tree.tickRoot()",
    body: "Dispara o tick real da BehaviorTree.CPP sobre um Fallback de 5 ramos: 3 Sequence(condição, PyDecide), um PyDecide incondicional (PY-PATROL) e um Patrol nativo de degradação -- o primeiro que suceder vence." });
  p({ node: "pyRtb", src: null, stage: 1, call: "FuelLow::tick()",
    title: "Sequence(FuelLow, PyDecide \"PY-RTB\") -- FAILURE",
    body: "Primeiro ramo: combustível acima da reserva de 5%. A CONDIÇÃO falha e a Sequence curto-circuita -- o PyDecide deste ramo (policy/rtb.py) nem chega a tickar; tickRoot() tenta o próximo ramo." });
  p({ node: "pyEvade", src: "ContactDetectedCondition::tick", hl: [11, 17], stage: 1, call: "tick()",
    title: "Sequence(ContactDetected, ReportAndEvade, PyDecide \"PY-EVADE\") -- FAILURE",
    body: "threatPolicy().engaged() == false: sem contato nem histerese de evasão pendente. Segundo ramo falha antes de chegar no PyDecide (policy/evade.py)." });
  p({ node: "pySupport", src: null, stage: 1, call: "AlertReceived::tick()",
    title: "Sequence(AlertReceived, PyDecide \"PY-SUPPORT\") -- FAILURE",
    body: "hasAlert == false: nenhum outro caça avisou nada neste frame. Terceiro ramo falha antes de chegar no PyDecide (policy/support.py)." });
  p({ node: "pyPatrol", src: "PyDecideAction::tick", hl: [30, 32], stage: 1, call: "mixr::xpyembed::decide(scriptId_, ...)",
    title: "PyDecide (script=policy/patrol.py, label=PY-PATROL) -- SUCCESS",
    body: "Quarto ramo -- sem condição, mesmo papel do Patrol nativo de produção. Empacota os 28 floats de domain::WorldView na ordem canônica e chama mixr::xpyembed::decide(), que executa decide(obs) dentro do interpretador Python embarcado (libs/xpyembed) e devolve (heading_deg, altitude_m, speed_kts) em UNIDADES FÍSICAS diretas -- ao contrário de OnnxPolicyAction, que por padrão devolve [-1,1] e desnormaliza depois.",
    warn: "Este nó NÃO é C++ do plugin: quem calcula o comando é o arquivo ./src/poc/python-flight/configs/policy/patrol.py apontado pela porta 'script' -- editar a política deixa de ser recompilar. Sem interpretador Python, script ausente, sem decide() ou exceção: FAILURE, e o Fallback cai no Patrol NATIVO (o último ramo desta árvore)." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [30, 32], stage: 1, call: "new FlightAction(); action->setCommand(...)",
    title: "BtBehavior::genAction() -- new FlightAction() (pré-referenciada)",
    body: "tickRoot() retornou com currentDecision.taken=true -- o CONTEÚDO veio de Python, o código de BtBehavior::genAction() não mudou. Copia comando e rótulo ('PY-PATROL'), devolve pré-referenciada -- mesmo contrato do UBF de sempre." });
  p({ node: "agent", src: "Agent::controller", hl: [12, 13], stage: 2, call: "action->execute(actor)",
    title: "Agent::controller(dt) -- action->execute(actor)",
    body: "A FlightAction retornada por genAction() é executada AQUI -- e só aqui que ela de fato toca o Autopilot." });
  p({ node: "action", src: "FlightAction::execute", hl: [2, 9], stage: 2, call: "player->getPilotByType(typeid(Autopilot))",
    title: "FlightAction::execute() -- resolve o Autopilot",
    body: "O ator chega por PARÂMETRO -- a ação nunca guarda ponteiro pra aeronave. getPilotByType(typeid(Autopilot)) acha o piloto automático." });
  p({ node: "autopilot", src: "FlightAction::execute", hl: [27, 33], stage: 2, call: "setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    title: "autopilot->setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    body: "Os três comandos que de fato chegam ao JSBSimModel. O Autopilot não sabe (nem precisa saber) que o comando veio de um script Python -- recebe os MESMOS três números que receberia de qualquer outro ramo.",
    warn: "É o ÚNICO ponto desta cadeia inteira que é de fato código NATIVO do framework sendo comandado -- tudo antes disso, inclusive a chamada ao interpretador, é código deste modelo/SDK (models/players/A-4, libs/xpyembed)." });
  p({ node: "action", src: "FlightAction::execute", hl: [42, 54], stage: 2, call: "xboard::setBehaviorLabel(...); LOG(INFO)",
    title: "FlightAction::execute() -- xboard + log de transição",
    body: "xboard::setBehaviorLabel()/bumpDecisionCount() publicam o rótulo ('PY-PATROL') pro dashboard/dump (dec=) -- o MESMO ponto de atuação, comum aos dois agentes UBF, que a seção 'libs/xlog' do CLAUDE.md documenta como fonte das transições logadas." });
  p({ node: "agent", src: "Agent::controller", hl: [13, 13], stage: 2, call: "action->unref()",
    title: "action->unref() -- a ação é liberada",
    body: "A FlightAction é efêmera: nasceu em genAction(), atuou em execute(actor), e é liberada AQUI, no mesmo ciclo -- nunca fica guardada em lugar nenhum." });
  return st;
}

function traceFlightOnnx() {
  const st = [];
  const p = (s) => st.push({ ...s, i: st.length });
  p({ node: "agent", src: "AgentTC::updateTC", hl: [2, 2], stage: null, call: "controller(dt)",
    title: "AgentTC::updateTC(dt) → controller(dt)",
    body: "Chamado pelo passeio de componentes da fase 3 (Component::updateTC() desce até aqui, dentro do próprio ( Aircraft )). O método do framework é trivial -- só repassa para o controller() virtual, que por polimorfismo cai na versão que FlightAgentTC sobrescreve." });
  p({ node: "agent", src: "FlightAgentTC::controller", hl: [12, 24], stage: null, call: "BaseClass::controller(dt * 4.0)",
    title: "FlightAgentTC::controller(dt) -- o gate de fase",
    body: "Este componente é chamado 4x por frame (uma por fase, dt/4 cada) -- só age quando world->phase()==3. BaseClass::controller(dt*4.0) reconstrói o dt do FRAME INTEIRO antes de entrar no ciclo genérico.",
    warn: "Sem esse filtro de fase, o mesmo ciclo rodaria 4x por frame -- a MESMA decisão repetida." });
  p({ node: "agent", src: "Agent::controller", hl: [7, 7], stage: 0, call: "state->updateState(actor)",
    title: "Agent::controller(dt) -- state->updateState(actor)",
    body: "Ciclo GENÉRICO do framework (mixr::base::ubf::Agent) -- o MESMO nas 3 pocs (produção, Python, ONNX): só a folha sob 'behavior' muda." });
  p({ node: "state", src: "FlightState::updateState", hl: [13, 20], stage: 0, call: "updateState(actor)",
    title: "FlightState::updateState() -- percepção",
    body: "Lê só acessores nativos do Player/AirVehicle e preenche um domain::WorldView puro -- a MESMA ordem canônica de 28 campos que xrlbridge/ObservationFields.hpp define, e que o .onnx foi TREINADO para receber (src/rl/tools/export_onnx.py usa a mesma ordem, nunca uma lista escrita à mão)." });
  p({ node: "agent", src: "Agent::controller", hl: [10, 10], stage: 1, call: "behavior->genAction(state, dt)",
    title: "Agent::controller(dt) -- behavior->genAction(state, dt)",
    body: "Com o estado atualizado, a vez é do comportamento plugado -- a MESMA classe BtBehavior de sempre; treeFile: aqui aponta para flight_tree_onnx.xml." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [15, 26], stage: 1, call: "tree.tickRoot()",
    title: "BtBehavior::genAction() -- tree.tickRoot()",
    body: "Dispara o tick real da BehaviorTree.CPP -- aqui sobre um Fallback de só 2 ramos: OnnxPolicy (a rede) e Patrol (rede de segurança)." });
  p({ node: "onnxPolicy", src: "OnnxPolicyAction::tick", hl: [44, 46], stage: 1, call: "mixr::xinfer::run(modelId_, ...)",
    title: "OnnxPolicy (model=policy_barrier.onnx, label=ONNX) -- SUCCESS",
    body: "Único ramo condicional desta árvore -- tenta SEMPRE primeiro. Empacota os 28 floats na mesma ordem canônica, chama mixr::xinfer::run() -- uma sessão do ONNX Runtime, cacheada por CAMINHO em libs/xinfer e compartilhada pelas 4 aeronaves -- e, com normalized=true (o default), desnormaliza a saída [-1,1] via xrlbridge::unscaleCommand().",
    warn: "Este nó NÃO tem árvore de decisão nenhuma por trás: a política treinada É o mapa observação→ação inteiro, inclusive o 'quando' agir. Modelo ausente, forma diferente de 28→3 ou falha de inferência: FAILURE, e o Fallback cai no Patrol nativo -- a 'rede de segurança' que o próprio flight_tree_onnx.xml documenta no cabeçalho." });
  p({ node: "behavior", src: "BtBehavior::genAction", hl: [30, 32], stage: 1, call: "new FlightAction(); action->setCommand(...)",
    title: "BtBehavior::genAction() -- new FlightAction() (pré-referenciada)",
    body: "tickRoot() retornou com currentDecision.taken=true. genAction() cria a FlightAction, copia comando e rótulo ('ONNX'), e devolve pré-referenciada -- o CÓDIGO de BtBehavior::genAction() não sabe (nem precisa saber) que quem decidiu foi uma rede neural." });
  p({ node: "agent", src: "Agent::controller", hl: [12, 13], stage: 2, call: "action->execute(actor)",
    title: "Agent::controller(dt) -- action->execute(actor)",
    body: "A FlightAction retornada por genAction() é executada AQUI -- e só aqui que ela de fato toca o Autopilot." });
  p({ node: "action", src: "FlightAction::execute", hl: [2, 9], stage: 2, call: "player->getPilotByType(typeid(Autopilot))",
    title: "FlightAction::execute() -- resolve o Autopilot",
    body: "O ator chega por PARÂMETRO -- a ação nunca guarda ponteiro pra aeronave. getPilotByType(typeid(Autopilot)) acha o piloto automático." });
  p({ node: "autopilot", src: "FlightAction::execute", hl: [27, 33], stage: 2, call: "setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    title: "autopilot->setCommandedHeadingD/AltitudeFt/VelocityKts(...)",
    body: "Os três comandos que de fato chegam ao JSBSimModel. O Autopilot não sabe (nem precisa saber) que o comando veio de uma rede neural -- recebe os MESMOS três números que receberia de qualquer outro ramo.",
    warn: "É o ÚNICO ponto desta cadeia inteira que é de fato código NATIVO do framework sendo comandado -- tudo antes disso, inclusive a inferência, é código deste modelo/SDK (models/players/A-4, libs/xinfer)." });
  p({ node: "action", src: "FlightAction::execute", hl: [42, 54], stage: 2, call: "xboard::setBehaviorLabel(...); LOG(INFO)",
    title: "FlightAction::execute() -- xboard + log de transição",
    body: "xboard::setBehaviorLabel()/bumpDecisionCount() publicam o rótulo ('ONNX') pro dashboard/dump (dec=) -- é o que faz bt=ONNX aparecer em 100% das linhas do dump desta poc (medido, ver CLAUDE.md)." });
  p({ node: "agent", src: "Agent::controller", hl: [13, 13], stage: 2, call: "action->unref()",
    title: "action->unref() -- a ação é liberada",
    body: "A FlightAction é efêmera: nasceu em genAction(), atuou em execute(actor), e é liberada AQUI, no mesmo ciclo -- nunca fica guardada em lugar nenhum." });
  return st;
}

const FLIGHT_TRACES = {
  patrol: { label: "Patrulha (fallback)", build: traceFlightPatrol },
  evade: { label: "Contato → Evasão (alerta)", build: traceFlightEvade },
  python: { label: "Decisão em Python (PyDecide)", build: traceFlightPython,
    tree: FLIGHT_TREE_PYTHON, btcppScope: FLIGHT_BTCPP_SCOPE_PYTHON,
    edlText: FLIGHT_EDL_TEXT_PYTHON, edlRange: FLIGHT_EDL_RANGE_PYTHON },
  onnx: { label: "Decisão por rede neural (OnnxPolicy)", build: traceFlightOnnx,
    tree: FLIGHT_TREE_ONNX, btcppScope: FLIGHT_BTCPP_SCOPE_ONNX,
    edlText: FLIGHT_EDL_TEXT_ONNX, edlRange: FLIGHT_EDL_RANGE_ONNX },
};

/* ---------------------------- decisao de voo ------------------------- */

function FlightDecision({ onOpenCatalog }) {
  const [traceKey, setTraceKey] = useState("patrol");
  const [i, setI] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(1100);
  const [pinned, setPinned] = useState(null);
  const graphRef = useRef(null);
  const { view, setView, svgRef, onDown, onMove, onUp, drag } = usePanZoom();
  const [detailTab, setDetailTab] = useState("step");
  const [autoFollow, setAutoFollow] = useState(false);
  const [showDetail, setShowDetail] = useState(true);
  // Explicação Agent vs. AgentTC -- discreta de propósito (fechada por        *
  // padrão, atrás de um toggle na legenda): é contexto sobre o PADRÃO, não   *
  // sobre o passo atual, e não deve competir por espaço com o grafo/detalhe. *
  const [showAgentFigure, setShowAgentFigure] = useState(false);
  const sliderActive = useRef(false);

  // Entrada ATIVA de FLIGHT_TRACES -- "tree"/"btcppScope"/"edlText"/"edlRange" *
  // só existem nas trilhas Python/ONNX; Patrulha/Evasão caem nos defaults de   *
  // produção (FLIGHT_TREE/FLIGHT_BTCPP_SCOPE/FLIGHT_EDL_TEXT/FLIGHT_EDL_RANGE) *
  // -- é essa troca que permite a mesma aba mostrar 3 árvores diferentes.
  const activeTrace = FLIGHT_TRACES[traceKey];
  const activeTree = activeTrace.tree || FLIGHT_TREE;
  const activeIndex = useMemo(() => buildFlightIndex(activeTree), [activeTree]);
  const trace = useMemo(() => activeTrace.build(), [traceKey]);
  // "autopilot" é empurrado pra FORA da coluna dos ramos da BT.CPP (que          *
  // também é a coluna natural de qualquer neto de "agent") -- sem isso, a       *
  // faixa "escopo: framework UBF" (que precisa envolver os ramos) também        *
  // envolveria o Autopilot, que é nativo e está FORA do ciclo UBF em si. Ver    *
  // ubfBand/FLIGHT_UBF_SCOPE mais abaixo.
  const nodes = useMemo(() => {
    const ns = flightLayout(activeTree);
    const auto = ns.find((n) => n.id === "autopilot");
    if (auto) auto.x = 3 * FCOL;
    return ns;
  }, [activeTree]);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  useEffect(() => { setI(0); setPinned(null); setView({ k: 1, x: 0, y: 0 }); }, [traceKey]);

  const idx = Math.min(i, trace.length - 1);
  const step = trace[idx] || {};

  useEffect(() => {
    if (!playing) return;
    const t = setTimeout(() => setI((p) => (p + 1 >= trace.length ? (setPlaying(false), p) : p + 1)), speed);
    return () => clearTimeout(t);
  }, [playing, i, speed, trace.length]);

  const move = useCallback((d) => { setPlaying(false); setI((p) => Math.max(0, Math.min(trace.length - 1, p + d))); }, [trace.length]);
  useEffect(() => {
    const h = (e) => {
      if (e.target.tagName === "INPUT" && e.target.type === "text") return;
      if (e.key === "ArrowRight") move(1);
      else if (e.key === "ArrowLeft") move(-1);
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [move]);

  const visitedNodes = useMemo(() => new Set(trace.map((s) => s.node)), [trace]);
  const detail = pinned ? activeIndex.byId[pinned] : activeIndex.byId[step.node] || activeIndex.byId.agent;

  const activeChainIds = useMemo(() => {
    const ids = new Set([step.node]);
    activeIndex.ancestorsOf(step.node || "agent").forEach(([a, b]) => { ids.add(a); ids.add(b); });
    return ids;
  }, [step.node, activeIndex]);
  const pathEdges = useMemo(() => new Set(activeIndex.ancestorsOf(step.node || "agent").map(([a, b]) => a + ">" + b)), [step.node, activeIndex]);

  const snip = flightSnip(step.src);
  const edlRange = flightEdlRangeFor(detail.id, activeTrace.edlRange);
  // Prévia mais curta (12 linhas) embutida na própria aba "Passo" -- pedido  *
  // explícito de mostrar o código de cada passo sem precisar trocar de aba; *
  // a aba "Código" continua com a janela cheia (22 linhas) para quem quiser *
  // mais contexto ao redor do trecho destacado.
  const previewWin = useMemo(() => (snip ? windowLines(snip.lines, step.hl, 12) : null), [snip, step.hl]);
  const codeWin = useMemo(() => (snip ? windowLines(snip.lines, step.hl, 22) : null), [snip, step.hl]);
  const activeEdlText = activeTrace.edlText || FLIGHT_EDL_TEXT;
  const edlWin = useMemo(() => windowLines(activeEdlText, edlRange, 22), [activeEdlText, edlRange]);
  useEffect(() => { if (detailTab === "code" && !snip) setDetailTab("step"); }, [detailTab, snip]);

  const W = Math.max(...nodes.map((n) => n.x)) + FNW + 30;
  const H = Math.max(...nodes.map((n) => n.y)) + FNH + 30;
  // Topo com folga bem maior que o resto: é onde os DOIS rótulos de board     *
  // moram, um dentro do outro (UBF por fora, BehaviorTree.CPP por dentro).
  const topMargin = 85, leftMargin = 14;

  // As duas faixas ("boards") pedidas -- calculadas a partir das posições      *
  // REAIS do layout, nunca de coordenadas fixas. UBF por fora (todo o ciclo    *
  // percepção/decisão/ação, papel genérico), BehaviorTree.CPP aninhada         *
  // dentro dela (só a política de decisão escolhida, que É uma árvore desta    *
  // lib de terceiro -- outra BtBehavior poderia não usar árvore nenhuma).
  const bandFor = (ids, padTop) => {
    const ns = ids.map((id) => pos[id]).filter(Boolean);
    if (!ns.length) return null;
    return {
      x0: Math.min(...ns.map((n) => n.x)) - 14,
      x1: Math.max(...ns.map((n) => n.x)) + FNW + 20,
      y0: Math.min(...ns.map((n) => n.y - FNH / 2)) - padTop,
      y1: Math.max(...ns.map((n) => n.y + FNH / 2)) + 14,
    };
  };
  const ubfBand = useMemo(() => bandFor(FLIGHT_UBF_SCOPE, 46), [pos]);
  const btcppBand = useMemo(() => bandFor(activeTrace.btcppScope || FLIGHT_BTCPP_SCOPE, 30), [pos, activeTrace]);

  const followViewFor = (nodeId, k) => {
    const n = pos[nodeId];
    if (!n) return null;
    const Ox = -leftMargin + W / 2;
    const Oy = (H - topMargin) / 2;
    return { k, x: k * (Ox - n.x), y: k * (Oy - n.y) };
  };
  useEffect(() => {
    if (!autoFollow || !step.node) return;
    const v = followViewFor(step.node, view.k);
    if (v) setView(v);
  }, [autoFollow, idx, step.node, view.k]);

  // Bloco de código compartilhado pela prévia (aba Passo) e pela aba Código  *
  // cheia -- só muda a janela (previewWin/codeWin) e o rótulo do arquivo.
  const renderCodeBlock = (win) => (
    <div className="mx-code">
      {win.cutBefore && <div className="mx-codecut">⋯ {win.offset} linha{win.offset === 1 ? "" : "s"} acima ⋯</div>}
      {win.lines.map((ln, k) => {
        const abs = k + win.offset;
        const on = step.hl && abs >= step.hl[0] && abs <= step.hl[1];
        return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{ln || " "}</span></div>;
      })}
      {win.cutAfter && <div className="mx-codecut">⋯ {snip.lines.length - win.offset - win.lines.length} linhas abaixo ⋯</div>}
    </div>
  );

  const btLeaf = FLIGHT_BT_LEAVES[detail.id];
  const modelEntry = flightEntry(detail.cls);

  return (
    <>
      <div className="mx-body" style={{ paddingBottom: 110 }}>
        <div style={{ display: "flex", gap: 14, alignItems: "center", flexWrap: "wrap", marginBottom: 6 }}>
          <div className="mx-tabs">
            {Object.entries(FLIGHT_TRACES).map(([k, t]) => (
              <button key={k} className="mx-tab" data-on={traceKey === k ? 1 : 0} onClick={() => setTraceKey(k)}>{t.label}</button>
            ))}
          </div>
          <div style={{ display: "flex", gap: 4 }}>
            {FLIGHT_STAGES.map((s) => {
              const on = step.stage === s.n;
              return (
                <div key={s.n} className={on ? "mx-phase-now" : ""} style={{ padding: "3px 9px", borderRadius: 2, fontSize: 11.5, background: on ? "var(--ink)" : "var(--panel)", color: on ? "var(--paper)" : "var(--muted)" }}>
                  {s.label}
                </div>
              );
            })}
          </div>
          <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)" }}>passo {idx + 1}/{trace.length}</span>
        </div>
        {/* Linha de CHAMADA -- pedido explícito de mostrar, na própria         *
           * animação, o nome do método/função em execução neste passo, sem   *
           * precisar abrir a aba Código. Repetida também sob o nó ativo no   *
           * grafo (ver o .map de nodes mais abaixo). */}
        <div className="mx-mono" style={{ fontSize: 12, color: "var(--hot)", marginBottom: 10 }}>
          › {step.src ? `${step.src}()` : detail.cls} <span style={{ color: "var(--muted)" }}>→</span> {step.call}
        </div>

        <div className="mx-graph" ref={graphRef}>
          <div className="mx-zoom">
            <div className="mx-zoomslider" title="Zoom -- também funciona com a roda do mouse">
              <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={0.01} value={view.k} aria-label="Zoom"
                onPointerDown={() => { sliderActive.current = true; }}
                onPointerUp={() => { sliderActive.current = false; }}
                onChange={(e) => setView((v) => ({ ...v, k: Number(e.target.value) }))} />
              <span className="mx-mono">{view.k.toFixed(2)}×</span>
            </div>
            <button className="mx-zbtn" data-w="1" onClick={() => setShowDetail((s) => !s)} title="Oculta o painel de detalhe abaixo, dando mais área ao grafo">
              {showDetail ? "▾ detalhe" : "▸ detalhe"}
            </button>
            <button className="mx-zbtn" data-w="1" onClick={() => setView({ k: 1, x: 0, y: 0 })}>ajustar</button>
          </div>
          <div className="mx-svgwrap" data-expanded={showDetail ? 0 : 1}>
            <svg ref={svgRef} viewBox={`${-leftMargin} ${-topMargin} ${W} ${H + topMargin}`} preserveAspectRatio="xMidYMid meet"
                 onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
              <g transform={`translate(${view.x},${view.y}) scale(${view.k})`}
                 style={{ transformOrigin: "center", transition: autoFollow && !drag.current && !sliderActive.current ? "transform 420ms cubic-bezier(.22,.61,.36,1)" : "none" }}>
                {ubfBand && (
                  <g>
                    <rect x={ubfBand.x0} y={ubfBand.y0} width={ubfBand.x1 - ubfBand.x0} height={ubfBand.y1 - ubfBand.y0}
                      rx="7" fill="var(--band-bg)" stroke="var(--rule)" strokeWidth="1.2" strokeDasharray="6 4" />
                    <text x={ubfBand.x0 + 8} y={ubfBand.y0 + 16} className="mx-mono" style={{ fontSize: 10.5, fontWeight: 600, fill: "var(--muted)" }}>
                      escopo: framework UBF (mixr::base::ubf) -- genérico, vale para qualquer player
                      <title>Agent/AgentTC + AbstractState/AbstractBehavior/AbstractAction -- as tres interfaces que qualquer state/behavior/action concreto implementa. Autopilot fica de fora: e o alvo nativo que a acao alcanca, nao parte do ciclo UBF.</title>
                    </text>
                  </g>
                )}
                {btcppBand && (
                  <g>
                    <rect x={btcppBand.x0} y={btcppBand.y0} width={btcppBand.x1 - btcppBand.x0} height={btcppBand.y1 - btcppBand.y0}
                      rx="6" fill="var(--edl-bg)" stroke="var(--rule)" strokeWidth="1.2" strokeDasharray="5 4" />
                    <text x={btcppBand.x0 + 8} y={btcppBand.y0 + 16} className="mx-mono" style={{ fontSize: 10.5, fontWeight: 600, fill: "var(--muted)" }}>
                      escopo: BehaviorTree.CPP
                      <title>BT::ConditionNode / BT::SyncActionNode, codigo de terceiro (nao deste modelo, nao do MIXR) -- ver flight_tree.xml</title>
                    </text>
                  </g>
                )}
                {activeIndex.edges.map(([a, b]) => {
                  const p = pos[a], q = pos[b];
                  const onPath = pathEdges.has(a + ">" + b);
                  const isCurrent = onPath && b === step.node;
                  const mid = p.x + FNW + 16;
                  const dPath = `M ${p.x + FNW} ${p.y} H ${mid} V ${q.y} H ${q.x}`;
                  return (
                    <g key={a + b}>
                      <path d={dPath} fill="none" stroke={onPath ? "var(--hot)" : "var(--rule)"} strokeWidth={onPath ? 2.2 : 1}
                        className={isCurrent ? "mx-edge-current" : onPath ? "mx-edge-onpath" : ""} />
                      {isCurrent && <circle cx={q.x} cy={q.y} r="5" className="mx-halo" stroke="var(--hot)" strokeWidth="2" />}
                    </g>
                  );
                })}
                {nodes.map((n) => {
                  const active = step.node === n.id;
                  const visited = visitedNodes.has(n.id);
                  const inChain = activeChainIds.has(n.id);
                  // "runtime" -- só as folhas PyDecideAction/OnnxPolicyAction   *
                  // (FLIGHT_TREE_PYTHON/FLIGHT_TREE_ONNX) carregam este campo:  *
                  // marca visualmente QUANDO um nó delega a decisão pra fora   *
                  // do plugin (libs/xpyembed / libs/xinfer), mesmo fora do     *
                  // passo ativo -- não só no instante em que ele tica.
                  const runtimeColor = n.runtime === "python" ? "var(--py-accent)" : n.runtime === "onnx" ? "var(--onnx-accent)" : null;
                  return (
                    <g key={n.id}>
                      <g className="mx-node"
                         transform={`translate(${n.x},${n.y - FNH / 2})`}
                         onClick={() => setPinned((p) => (p === n.id ? null : n.id))}>
                        {active && <circle cx={FNW / 2} cy={FNH / 2} r={FNH / 2} className="mx-halo" stroke="var(--hot)" strokeWidth="2.5" />}
                        <rect x="0" y="0" width={FNW} height={FNH} rx="3"
                          fill={active ? "var(--hot)" : "var(--paper)"}
                          stroke={pinned === n.id ? "var(--ink)" : active ? "var(--hot)" : runtimeColor || (inChain ? "var(--muted)" : "var(--rule)")}
                          strokeWidth={active || pinned === n.id || runtimeColor ? 1.6 : 1}
                          opacity={visited ? 1 : 0.4} strokeDasharray={visited ? "0" : "4 3"} />
                        <foreignObject x="10" y="4" width={FNW - 20} height="16" className="mx-fo">
                          <div className="mx-fo-row" title={n.cls}><span className="mx-fo-cls" style={{ fontSize: 11.5, color: active ? "var(--paper)" : "var(--ink)" }}>{n.cls}</span></div>
                        </foreignObject>
                        {/* Selo PY/ONNX -- mesmo idioma visual do "📌 fixado"    *
                           * já usado no painel de detalhe (mono, pequeno, fundo *
                           * colorido), sem inventar um novo elemento de UI. */}
                        {runtimeColor && (
                          <g>
                            <rect x={FNW - 34} y="3" width="30" height="13" rx="2" fill={runtimeColor} opacity={active ? 1 : 0.9} />
                            <text x={FNW - 19} y="12.5" textAnchor="middle" className="mx-mono" style={{ fontSize: 8.5, fontWeight: 700, fill: "var(--paper)" }}>
                              {n.runtime === "python" ? "PY" : "ONNX"}
                            </text>
                          </g>
                        )}
                        {/* Subtítulo com QUEBRA DE LINHA (não trunca/abrevia) -- *
                           * diferente do resto da página: aqui o texto é maior  *
                           * e o cartão (FNW/FNH) foi dimensionado pra caber 2   *
                           * linhas inteiras sem "...". */}
                        <foreignObject x="10" y="20" width={FNW - 20} height="34" className="mx-fo" style={{ pointerEvents: "none" }}>
                          <div title={n.sub} style={{ fontFamily: "var(--mono)", fontSize: 9.5, lineHeight: "12px", whiteSpace: "normal", wordBreak: "normal", color: active ? "var(--running-fg)" : "var(--sub-muted)" }}>
                            {n.sub}
                          </div>
                        </foreignObject>
                      </g>
                      {/* Rótulo de CHAMADA embaixo do nó ativo -- "na animação",  *
                         * pedido explícito: qual método/função roda AGORA. */}
                      {active && (
                        <foreignObject x={n.x - 40} y={n.y - FNH / 2 + FNH + 4} width={FNW + 80} height="16" className="mx-fo">
                          {/* Largura maior que o próprio cartão, e SEM herdar o    *
                             * "ellipsis" de .mx-fo-sub: chamadas como os três      *
                             * setCommanded* juntos passam de 45 caracteres --      *
                             * cortar isso era voltar a abreviar, o oposto do       *
                             * pedido. */}
                          <div title={step.call} style={{ fontFamily: "var(--mono)", fontSize: 10, color: "var(--hot)", fontWeight: 600, whiteSpace: "nowrap", textAlign: "center" }}>
                            {step.call}
                          </div>
                        </foreignObject>
                      )}
                    </g>
                  );
                })}
              </g>
            </svg>
          </div>
          <div className="mx-leg">
            <button className="mx-leg-toggle" onClick={() => setShowAgentFigure((s) => !s)}>
              {showAgentFigure ? "▾" : "▸"} Agent vs. AgentTC
            </button>
            <span><b style={{ color: "var(--hot)" }}>■</b> executando agora (rótulo abaixo = chamada em curso)</span>
            <span>borda tracejada e apagada = não visitado nesta trilha (curto-circuito do Fallback)</span>
            <span>faixa tracejada = escopo de terceiro (framework UBF e BehaviorTree.CPP, lado a lado)</span>
            <span><b className="mx-mono" style={{ color: "var(--py-accent)" }}>PY</b> = delega a decisão para um script Python embarcado (libs/xpyembed)</span>
            <span><b className="mx-mono" style={{ color: "var(--onnx-accent)" }}>ONNX</b> = delega a decisão para uma rede neural via ONNX Runtime (libs/xinfer)</span>
            <span>roda = zoom · arrastar = mover</span>
          </div>
          {/* "Figura" Agent vs. AgentTC -- pedido explícito, mas discreta de     *
             * propósito: fechada por padrão, atrás do toggle acima, no mesmo    *
             * espírito de "como ler um cartão" na aba Execução/Simulação. Não   *
             * é parte do grafo animado (nenhum dos dois é passo de trace) -- é  *
             * contexto estático sobre o PADRÃO que FlightAgentTC concretiza; a  *
             * própria aba Classe do nó "FlightAgentTC" mostra a cadeia de       *
             * herança completa (extends AgentTC extends Agent) com os slots     *
             * reais (state/behavior) de Agent. */}
          {showAgentFigure && (
            <div className="mx-cardleg">
              <div className="mx-mono" style={{ fontWeight: 600, fontSize: 12, marginBottom: 6 }}>Agent vs. AgentTC -- mesma base, dois pontos de entrada</div>
              <div style={{ display: "flex", gap: 12, flexWrap: "wrap" }}>
                <div style={{ flex: "1 1 240px", border: "1px dashed var(--rule)", borderRadius: 4, padding: "6px 9px" }}>
                  <div className="mx-mono" style={{ fontWeight: 600, fontSize: 11.5 }}>Agent</div>
                  <div style={{ fontSize: 11, color: "var(--muted)" }}>thread de FUNDO -- updateData(dt) chama controller(dt)</div>
                  <div style={{ fontSize: 10.5, color: "var(--sub-muted)", marginTop: 3 }}>ex.: SimAgent nativo (classe do framework, nao usada por nenhuma poc deste repositorio)</div>
                </div>
                <div style={{ flex: "1 1 240px", border: "1px dashed var(--hot)", borderRadius: 4, padding: "6px 9px" }}>
                  <div className="mx-mono" style={{ fontWeight: 600, fontSize: 11.5 }}>AgentTC <span style={{ color: "var(--hot)" }}>← usado neste exemplo</span></div>
                  <div style={{ fontSize: 11, color: "var(--muted)" }}>thread de TEMPO CRÍTICO -- updateTC(dt) chama controller(dt)</div>
                  <div style={{ fontSize: 10.5, color: "var(--sub-muted)", marginTop: 3 }}>ex.: FlightAgentTC (este exemplo, poc flight)</div>
                </div>
              </div>
              <p style={{ fontSize: 11, color: "var(--muted)", margin: "7px 0 0" }}>
                controller(dt) -- percepção, decisão e ação -- é EXATAMENTE o mesmo método, herdado de Agent. A única diferença entre
                as duas classes é QUANDO ele é chamado. Isto é um EXEMPLO de instanciação do padrão: qualquer player pode seguir a
                mesma receita (extends Agent OU AgentTC + AbstractState + AbstractBehavior + AbstractAction) com suas próprias classes
                concretas, sem mudar Agent/AgentTC em si.
              </p>
            </div>
          )}
        </div>

        {showDetail && (
        <div className="mx-pane">
          {pinned && activeIndex.byId[pinned] && (
            <div className="mx-card" style={{ marginBottom: 10, borderLeft: "3px solid var(--hot)" }}>
              <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 8, flexWrap: "wrap" }}>
                <span className="mx-mono" style={{ fontWeight: 600, fontSize: 12.5 }}>📌 fixado — {activeIndex.byId[pinned].cls}</span>
                <button className="mx-btn" style={{ fontSize: 11, padding: "2px 8px" }} onClick={() => setPinned(null)}>soltar</button>
              </div>
            </div>
          )}
          <div className="mx-dtabs" role="tablist" aria-label="Detalhe do passo">
            <button className="mx-dtab" data-on={detailTab === "step" ? 1 : 0} onClick={() => setDetailTab("step")}>Passo</button>
            <button className="mx-dtab" data-on={detailTab === "code" ? 1 : 0} disabled={!snip} onClick={() => snip && setDetailTab("code")}>Código completo</button>
            <button className="mx-dtab" data-on={detailTab === "edl" ? 1 : 0} onClick={() => setDetailTab("edl")}>EDL do cenário</button>
            <button className="mx-dtab" data-on={detailTab === "class" ? 1 : 0} onClick={() => setDetailTab("class")}>Classe</button>
          </div>
          <div className="mx-detailbody" key={detailTab}>
            {detailTab === "step" && (
              <div className="mx-card">
                <div className="mx-mono" style={{ fontSize: 12.5, fontWeight: 600, marginBottom: 5 }}>{step.title}</div>
                <p style={{ margin: 0, fontSize: 13, lineHeight: 1.5 }}>{step.body}</p>
                {step.warn && <p className="mx-warn">{step.warn}</p>}
                {snip ? (
                  <>
                    <div className="mx-lbl" style={{ marginTop: 12 }}>
                      <span className="mx-mono">{snip.file}:{snip.line + (step.hl ? step.hl[0] : 0)}</span>
                      <span>chamada: <b className="mx-mono" style={{ color: "var(--hot)" }}>{step.call}</b></span>
                    </div>
                    {renderCodeBlock(previewWin)}
                    <p style={{ fontSize: 11, color: "var(--muted)", margin: "4px 0 0" }}>
                      trecho reduzido (12 linhas) -- "Código completo" mostra a função inteira, com a mesma linha em destaque.
                    </p>
                  </>
                ) : (
                  <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 10 }}>
                    chamada: <b className="mx-mono">{step.call}</b> -- código-fonte não incluído nesta curadoria (a condição em si é curta; ver as folhas irmãs para o padrão real).
                  </p>
                )}
              </div>
            )}

            {detailTab === "code" && snip && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono">{snip.file}:{snip.line + (step.hl ? step.hl[0] : 0)}</span>
                  <span>C++ real, conferido à mão (fora do escopo do extrator, que só cobre contexts/src/mixr)</span>
                </div>
                {renderCodeBlock(codeWin)}
              </>
            )}
            {detailTab === "edl" && (
              <>
                <div className="mx-lbl"><span className="mx-mono">cenário real (condensado)</span><span>{detail.cls} · {detail.edl}</span></div>
                <div className="mx-edl">
                  {edlWin.cutBefore && <div className="mx-codecut">⋯ {edlWin.offset} linha{edlWin.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {edlWin.lines.map((ln, k) => {
                    const abs = k + edlWin.offset;
                    const on = abs >= edlRange[0] && abs <= edlRange[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{abs + 1}</span><span className="mx-src">{ln || " "}</span></div>;
                  })}
                  {edlWin.cutAfter && <div className="mx-codecut">⋯ {activeEdlText.length - edlWin.offset - edlWin.lines.length} linhas abaixo ⋯</div>}
                </div>
                {detail.id !== "agent" && detail.id !== "state" && detail.id !== "behavior" && detail.id !== "autopilot" && (
                  <p className="mx-warn">Este nó não tem slot próprio no EDL -- a árvore de comportamento é referenciada por treeFile: (destacado acima), não por um bloco EDL separado.</p>
                )}
              </>
            )}
            {detailTab === "class" && (
              <>
                <div className="mx-lbl">
                  <span className="mx-mono" style={{ color: "var(--ink)", fontWeight: 600 }}>{detail.cls}</span>
                  <span>{pinned ? "fixado" : "segue a execução"}</span>
                </div>
                {btLeaf ? (
                  <>
                    <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 8px" }}>
                      Folha(s) da árvore de comportamento -- código de TERCEIRO (biblioteca BehaviorTree.CPP, vendorizada em{" "}
                      <span className="mx-mono">contexts/src/BehaviorTree.CPP/</span>), não deste modelo nem do MIXR. "Slot" (EDL)
                      não se aplica aqui -- o mecanismo próprio da lib é "port" (par chave/valor lido do atributo XML do nó, via
                      providedPorts()).
                    </p>
                    {["cond", "act"].map((k) => {
                      const leaf = btLeaf[k];
                      if (!leaf) return null;
                      return (
                        <div key={k} style={{ padding: "6px 8px", marginBottom: 6, borderLeft: "2px solid var(--rule)" }}>
                          <div>
                            <span className="mx-mono" style={{ fontWeight: 600, fontSize: 11.5 }}>{leaf.cls}</span>
                            <span style={{ fontSize: 11, color: "var(--muted)" }}> {"<"} {leaf.base}</span>
                          </div>
                          <div style={{ fontSize: 10.5, color: "var(--muted)" }}>{leaf.hd}</div>
                          <div style={{ fontSize: 11, marginTop: 3 }}>
                            {leaf.ports.length
                              ? <>ports: {leaf.ports.map(([pn, pt]) => <span key={pn} className="mx-mono" style={{ marginRight: 10 }}>{pn} {"<"}{pt}{">"}</span>)}</>
                              : <span style={{ color: "var(--muted)" }}>sem ports (providedPorts() vazio)</span>}
                          </div>
                          {/* Só PyDecideAction/OnnxPolicyAction têm "delegate" -- *
                             * o que distingue estas duas folhas de qualquer      *
                             * outra: quem calcula o comando não é este .cpp, é   *
                             * uma biblioteca de PONTE (libs/xpyembed/libs/xinfer)*
                             * chamando pra FORA do plugin. */}
                          {leaf.delegate && (
                            <p className="mx-warn" style={{ marginTop: 6 }}>
                              Delega para <b className="mx-mono">{leaf.delegate.lib}</b> via{" "}
                              <b className="mx-mono">{leaf.delegate.to}</b> -- custo medido{" "}
                              <b>{leaf.delegate.cost}</b>. Degradação: {leaf.delegate.fail}.
                            </p>
                          )}
                        </div>
                      );
                    })}
                  </>
                ) : (
                  <>
                    <div style={{ fontSize: 11.5, color: "var(--muted)", marginBottom: 7 }}>
                      módulo <b className="mx-mono">{flightModuleOf(detail.cls)}</b> · EDL <b className="mx-mono">( {flightFactoryOf(detail.cls)} )</b>
                      {modelEntry && modelEntry.f && modelEntry.f !== detail.cls ? <span style={{ color: "var(--rf)" }}> · nome divergente</span> : null}
                      <br />{(modelEntry || {}).src || (modelEntry || {}).hd}
                    </div>
                    {flightChainOf(detail.cls).map((c, k) => {
                      const e = flightEntry(c);
                      const n = e ? e.sl.length : 0;
                      return (
                        <div key={c} style={{ padding: "3px 8px", marginLeft: k * 6, borderLeft: `2px solid ${n ? "var(--hot)" : "var(--rule)"}`, background: n ? "var(--panel)" : "transparent" }}>
                          <span className="mx-mono" style={{ fontSize: 11.5, fontWeight: n ? 600 : 400 }}>{c}</span>
                          <span style={{ fontSize: 11, color: "var(--muted)" }}>{n ? ` -- ${n} slot${n > 1 ? "s" : ""} próprio${n > 1 ? "s" : ""}` : ""}</span>
                        </div>
                      );
                    })}
                    <div className="mx-lbl" style={{ marginTop: 12 }}>
                      <span>Slots ({flightAllSlotsOf(detail.cls).length} na cadeia)</span>
                      <span>{modelEntry ? modelEntry.sl.length : 0} próprios</span>
                    </div>
                    <div className="mx-slotgrid">
                      {flightAllSlotsOf(detail.cls).map(([s, from], k) => (
                        <div className="mx-slot" key={s + k}>
                          <span>{s}{flightSlotType(from, s) ? <span style={{ color: "var(--muted)" }}> {"<"}{flightSlotType(from, s)}{">"}</span> : ""}</span>
                          <span>{from}</span>
                        </div>
                      ))}
                      {!flightAllSlotsOf(detail.cls).length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>Nenhum slot em toda a cadeia.</div>}
                    </div>
                    {MODEL[detail.cls] && onOpenCatalog && (
                      <button className="mx-btn" style={{ marginTop: 10, fontSize: 11 }} onClick={() => onOpenCatalog(detail.cls)}>Ver classe completa no Catálogo →</button>
                    )}
                  </>
                )}
                {detail.note && <p className="mx-warn">{detail.note}</p>}
              </>
            )}
          </div>
        </div>
        )}
      </div>

      <div className="mx-transport">
        <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>{playing ? "Pausar" : "Reproduzir"}</button>
        <button className="mx-btn" onClick={() => move(-1)}>←</button>
        <button className="mx-btn" onClick={() => move(1)}>→</button>
        <button className="mx-btn" onClick={() => { setPlaying(false); setI(0); }}>Início</button>
        <div className="mx-tl" role="slider" aria-label="Linha do tempo" aria-valuenow={idx} aria-valuemin={0} aria-valuemax={trace.length - 1} tabIndex={0}
             onKeyDown={(e) => { if (e.key === "ArrowRight") move(1); if (e.key === "ArrowLeft") move(-1); }}>
          {trace.map((s, k) => (
            <div key={k} className="mx-seg" onClick={() => { setPlaying(false); setI(k); }} title={`${s.title} — ${s.call}`}
              style={{ background: k === idx ? "var(--hot)" : s.stage != null ? ["var(--seg-phase-0)", "var(--seg-phase-1)", "var(--seg-phase-2)"][s.stage] : "var(--rule)", height: k === idx ? "100%" : "45%", opacity: k <= idx ? 1 : 0.4 }} />
          ))}
        </div>
        <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", minWidth: 52 }}>{idx + 1}/{trace.length}</span>
        <label style={{ fontSize: 12.3, display: "flex", gap: 5, alignItems: "center" }} title="Zoom/pan acompanham sozinhos o nó ativo a cada passo">
          <input type="checkbox" checked={autoFollow} onChange={(e) => setAutoFollow(e.target.checked)} /> Seguir ramo
        </label>
        <select className="mx-input" value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Velocidade">
          <option value={1800}>Lento</option><option value={1100}>Normal</option><option value={550}>Rápido</option>
        </select>
      </div>
    </>
  );
}

/* ---------------------------- catálogo ----------------------------- */

const MOD_ORDER = ["base", "simulation", "terrain", "linkage", "recorder", "models", "interop/dis", "plugin:A-4"];

function Catalog({ onOpen, openClass, setOpenClass }) {
  const [q, setQ] = useState("");
  const [filter, setFilter] = useState("all");
  const [sel, setSel] = useState(null);
  const boxRef = useRef(null);

  useEffect(() => {
    const h = (e) => { if (e.key === "/" && document.activeElement !== boxRef.current) { e.preventDefault(); boxRef.current && boxRef.current.focus(); } };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, []);

  // Chegada vinda do popup de nó da aba Execução ("ver classe completa no
  // Catálogo") -- mesmo padrão do useEffect de `focus` dentro de Exec:
  // consome o pedido (volta a null) pra um clique repetido na MESMA classe
  // reabrir o cartão de novo.
  useEffect(() => {
    if (!openClass) return;
    setSel(openClass);
    setOpenClass(null);
  }, [openClass, setOpenClass]);

  const match = (c) => {
    const e = MODEL[c]; if (!e) return false;
    const t = q.trim().toLowerCase();
    if (t && !(c.toLowerCase().includes(t) || (e.f || "").toLowerCase().includes(t) ||
      (e.sl || []).some((s) => s.toLowerCase().includes(t)))) return false;
    if (filter === "div" && !e.f) return false;
    if (filter === "phase" && !e.wp.length) return false;
    if (filter === "scn" && !IN_SCENARIO.has(c)) return false;
    if (filter === "ubf" && !chainOf(c).some((a) => UBF_ROOTS.includes(a))) return false;
    return true;
  };

  const mods = useMemo(() => {
    const byMod = {};
    Object.entries(FACTORIES).forEach(([mod, d]) => { byMod[mod] = { file: d.file, list: d.classes.filter(match) }; });
    return byMod;
  }, [q, filter]);

  const shownCount = Object.values(mods).reduce((a, m) => a + m.list.length, 0);

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-stats">
        <span><b>{STATS.classes}</b> classes com DECLARE_SUBCLASS</span>
        <span><b>{STATS.registered}</b> registradas em fábrica</span>
        <span><b>{STATS.divergent}</b> com nome de fábrica divergente</span>
        <span><b>{STATS.slotsTotal}</b> slots em <b>{STATS.withSlots}</b> classes</span>
        <span><b>{STATS.dispatch}</b> despacham por fase (derivam de System)</span>
        <span><b>{STATS.phaseWork}</b> fazem trabalho em alguma fase — <b>{STATS.phaseOwn}</b> a implementam de fato</span>
      </div>

      <div style={{ display: "flex", gap: 10, alignItems: "center", flexWrap: "wrap", marginBottom: 8 }}>
        <input ref={boxRef} className="mx-input" type="text" value={q} onChange={(e) => setQ(e.target.value)}
          placeholder="Classe, nome de fábrica ou slot  ( / )" style={{ minWidth: 240, flex: "0 1 340px" }} aria-label="Filtrar" />
        <div className="mx-tabs">
          {[["all", "Todas"], ["div", "Nome divergente"], ["phase", "Trabalha em fase"], ["scn", "No cenário"], ["ubf", "Decisão (UBF)"]].map(([k, l]) => (
            <button key={k} className="mx-tab" data-on={filter === k ? 1 : 0} onClick={() => setFilter(k)}>{l}</button>
          ))}
        </div>
        <span style={{ fontSize: 12, color: "var(--muted)" }}>{shownCount} resultados</span>
      </div>

      {sel && <ClassCard c={sel} onClose={() => setSel(null)} onOpen={onOpen} />}

      {MOD_ORDER.filter((m) => mods[m] && mods[m].list.length).map((m) => (
        <div key={m} className="mx-mod">
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", gap: 12, flexWrap: "wrap" }}>
            <h2 className="mx-mono" style={{ fontSize: 13.5, margin: 0, fontWeight: 600 }}>{mods[m].file}</h2>
            <span className="mx-chip">{FACTORIES[m].classes.length} nomes registrados</span>
          </div>
          <div className="mx-wrap" style={{ marginTop: 8 }}>
            {mods[m].list.map((c) => <ClassChip key={c} c={c} onClick={() => setSel(c)} />)}
          </div>
        </div>
      ))}
    </div>
  );
}

function ClassChip({ c, onClick }) {
  const e = MODEL[c] || {};
  return (
    <span className="mx-cls" data-scn={IN_SCENARIO.has(c) ? 1 : 0} data-div={e.f ? 1 : 0} data-reg={e.r ? 1 : 0}
      onClick={onClick} title={`${c} < ${e.b || "—"}`}>
      {c}
      {e.f && <em style={{ color: "var(--rf)", fontStyle: "normal", fontSize: 10.5 }}>→{e.f}</em>}
      {e.wp && e.wp.length ? <span style={{ color: "var(--hot)", fontSize: 10 }}>{e.wp.join("")}</span> : null}
    </span>
  );
}

function ClassCard({ c, onClose, onOpen }) {
  const e = MODEL[c]; if (!e) return null;
  const slots = allSlots(c);
  const derived = Object.keys(MODEL).filter((k) => MODEL[k].b === c);
  // Corpo real só existe pros métodos que a PRÓPRIA classe sobrescreve com
  // corpo não-vazio -- SNIPPETS cobre TODO o universo do catálogo agora
  // (as 225 classes nativas + as 9 do plugin:A-4, não mais uma curadoria
  // manual restrita às trilhas de Execução; ver
  // tools/generate_manual_catalog.py), então uma classe sem nenhum
  // snippet é uma classe que legitimamente não sobrescreve nenhum dos
  // métodos rastreados (dado puro, sem trabalho de fase), não uma que
  // "ficou de fora da curadoria". Varre as CHAVES de SNIPPETS por prefixo
  // "Classe::" em vez de cruzar com e.ov -- os dois vêm da MESMA passada
  // de extração, mas varrer por prefixo também cobre método que a classe
  // DECLARA pela primeira vez (não "sobrescreve" nada), caso de Agent::
  // controller. Só filhos DIRETOS aparecem (própria classe, não herdados) --
  // a cadeia continua navegável pelo próprio card.
  const ownSnippets = Object.keys(SNIPPETS)
    .filter((k) => k.startsWith(`${c}::`))
    .map((k) => [k.slice(c.length + 2), SNIPPETS[k]]);
  return (
    <div className="mx-card" style={{ margin: "8px 0 6px" }}>
      <div style={{ display: "flex", justifyContent: "space-between", gap: 12, flexWrap: "wrap" }}>
        <div>
          <span className="mx-mono" style={{ fontWeight: 600, fontSize: 14 }}>{c}</span>
          <span className="mx-mono" style={{ color: "var(--muted)", fontSize: 12 }}> · EDL ( {e.f || c} ){e.r ? "" : " · NÃO registrada"}</span>
          <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)" }}>{e.src || e.hd}</div>
        </div>
        <div style={{ display: "flex", gap: 6 }}>
          {IN_SCENARIO.has(c) && <button className="mx-btn" onClick={() => onOpen(c)}>ver no cenário</button>}
          <button className="mx-btn" onClick={onClose}>fechar</button>
        </div>
      </div>

      <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit, minmax(240px, 1fr))", gap: 14, marginTop: 10 }}>
        <div>
          <div className="mx-lbl"><span>Cadeia de herança</span></div>
          {e.ch.map((a, k) => (
            <div key={a} style={{ padding: "2px 8px", marginLeft: k * 6, borderLeft: "2px solid var(--rule)" }}>
              <span className="mx-mono" style={{ fontSize: 11.5 }}>{a}</span>
              <span style={{ fontSize: 11, color: "var(--muted)" }}>
                {(MODEL[a] && MODEL[a].sl.length) ? ` · ${MODEL[a].sl.length} slots` : ""}
              </span>
            </div>
          ))}
          <div style={{ fontSize: 12, marginTop: 8 }}>
            {e.wp.length
              ? <>Trabalha nas fases <b className="mx-mono">{e.wp.join(", ")}</b> — {e.wp.map((p) => `${PHASES[p].m}() em ${e.po[String(p)]}`).join("; ")}.</>
              : e.d ? "Deriva de System (despacha por fase), mas nenhuma classe da cadeia implementa um método de fase."
                    : "Não deriva de System: não participa do despacho por fase."}
          </div>
          {e.ov && e.ov.length ? (
            <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 6 }}>
              sobrescreve: <span className="mx-mono">{e.ov.join(", ")}</span>
            </div>
          ) : null}
          {derived.length ? (
            <div style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 6 }}>
              derivadas ({derived.length}): <span className="mx-mono">{derived.slice(0, 12).join(", ")}{derived.length > 12 ? "…" : ""}</span>
            </div>
          ) : null}
        </div>
        <div>
          <div className="mx-lbl"><span>Slots ({slots.length})</span><span>{e.sl.length} próprios</span></div>
          <div>
            {slots.map(([s, from], k) => <div className="mx-slot" key={s + k}><span>{s}</span><span>{from}</span></div>)}
            {!slots.length && <div style={{ fontSize: 11.5, color: "var(--muted)" }}>EMPTY_SLOTTABLE em toda a cadeia.</div>}
          </div>
        </div>
      </div>

      <div style={{ marginTop: 12 }}>
        <div className="mx-lbl">
          <span>Código-fonte</span>
          <span>{ownSnippets.length ? `${ownSnippets.length} método${ownSnippets.length > 1 ? "s" : ""} extraído${ownSnippets.length > 1 ? "s" : ""}` : "nenhum método extraído"}</span>
        </div>
        {ownSnippets.length ? ownSnippets.map(([m, s]) => (
          <div key={m} style={{ marginBottom: 10 }}>
            <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)", marginBottom: 3 }}>{c}::{m} — {s.file}:{s.line}</div>
            <div className="mx-code">
              {s.lines.map((ln, k) => (
                <div key={k} className="mx-cl"><span className="mx-num">{s.line + k}</span><span className="mx-src">{ln || " "}</span></div>
              ))}
              {s.trunc && <div className="mx-codecut">⋯ corpo truncado nesta visualização ⋯</div>}
            </div>
          </div>
        )) : (
          <div style={{ fontSize: 11.5, color: "var(--muted)" }}>
            Esta classe não sobrescreve nenhum dos métodos rastreados com corpo próprio (dado puro, sem trabalho de fase) — nada para extrair aqui. arquivo:linha do topo do card continua valendo.
          </div>
        )}
      </div>
    </div>
  );
}

/* ============= diagrama de classe estrutural (aba "Estrutura") ======== *
 * CLASS_DIAGRAM (classes/tier2) é dado REAL, extraído do header C++ por  *
 * tools/extract_class_diagram.py -- ver a auto-verificação embutida     *
 * naquele script e tests/tools/test_extract_class_diagram.py. Tudo o    *
 * resto nesta seção (STRUCT_TOPOLOGY/STRUCT_BACKREFS/STRUCT_NOTES, o    *
 * layout umlLayout() e o componente StructDiagram) é curadoria/desenho  *
 * escritos à mão sobre esse dado -- ver o aviso na própria aba.         *
 * ======================================================================= */

/* GERADO por tools/extract_class_diagram.py. Nao editar. */
const CLASS_DIAGRAM = {"generatedBy":"tools/extract_class_diagram.py","classes":{"Referenced":{"file":"contexts/src/mixr/include/mixr/base/Referenced.hpp","namespace":"mixr::base","base":null,"attributes":[{"name":"semaphore","type":"long","visibility":"private","static":false},{"name":"refCount","type":"int","visibility":"private","static":false}],"components":[],"methods":[{"name":"Referenced","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Referenced() =default"},{"name":"Referenced","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Referenced(const Referenced&) = delete"},{"name":"operator=","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Referenced& operator=(const Referenced&) = delete"},{"name":"~Referenced","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual ~Referenced() =0"},{"name":"getRefCount","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"int getRefCount() const"},{"name":"ref","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"void ref() const"},{"name":"unref","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"void unref() const"}]},"Object":{"file":"contexts/src/mixr/include/mixr/base/Object.hpp","namespace":"mixr::base","base":"Referenced","attributes":[{"name":"slottable","type":"const SlotTable","visibility":"protected","static":true},{"name":"slotnames","type":"const char*","visibility":"private","static":true},{"name":"nslots","type":"const int","visibility":"private","static":true},{"name":"MSG_ERROR","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_WARNING","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_INFO","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_DEBUG","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_DATA","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_USER","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_STD_ALL","type":"const unsigned short","visibility":"public","static":true},{"name":"MSG_ALL","type":"const unsigned short","visibility":"public","static":true},{"name":"slotTable","type":"const SlotTable*","visibility":"protected","static":false},{"name":"enbMsgBits","type":"unsigned short","visibility":"private","static":false},{"name":"disMsgBits","type":"unsigned short","visibility":"private","static":false},{"name":"metaObject","type":"MetaObject","visibility":"private","static":true}],"components":[],"methods":[{"name":"Object","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Object()"},{"name":"Object","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Object(const Object& org)"},{"name":"operator=","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Object& operator=(const Object& org)"},{"name":"clone","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual Object* clone() const"},{"name":"~Object","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual ~Object()"},{"name":"copyData","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void copyData(const Object& org, const bool cc = false)"},{"name":"deleteData","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void deleteData()"},{"name":"isClassType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isClassType(const std::type_info& type) const"},{"name":"isFactoryName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isFactoryName(const char name[]) const"},{"name":"getFactoryName","visibility":"public","static":true,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"static const char* getFactoryName()"},{"name":"getSlotTable","visibility":"public","static":true,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"static const SlotTable& getSlotTable()"},{"name":"setSlotByIndex","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotByIndex(const int slotindex, Object* const obj)"},{"name":"setSlotByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotByName(const char* const slotname, Object* const obj)"},{"name":"slotIndex2Name","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const char* slotIndex2Name(const int slotindex) const"},{"name":"slotName2Index","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"int slotName2Index(const char* const slotname) const"},{"name":"isValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isValid() const"},{"name":"isMessageEnabled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isMessageEnabled(const unsigned short msgType) const"},{"name":"isMessageDisabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isMessageDisabled(const unsigned short msgType) const"},{"name":"enableMessageTypes","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool enableMessageTypes(const unsigned short msgTypeBits)"},{"name":"disableMessageTypes","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool disableMessageTypes(const unsigned short msgTypeBits)"},{"name":"getMetaObject","visibility":"public","static":true,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"static const MetaObject* getMetaObject()"},{"name":"getMessageEnableBits","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned short getMessageEnableBits() const"},{"name":"getMessageDisableBits","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned short getMessageDisableBits() const"}]},"Component":{"file":"contexts/src/mixr/include/mixr/base/Component.hpp","namespace":"mixr::base","base":"Object","attributes":[{"name":"pts","type":"bool","visibility":"private","static":false},{"name":"frz","type":"bool","visibility":"private","static":false},{"name":"shutdown","type":"bool","visibility":"private","static":false}],"components":[{"name":"components","target":"PairStream","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"containerPtr","target":"Component","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"selected","target":"Component","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"selection","target":"Object","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"timingStats","target":"Statistic","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Component","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Component()"},{"name":"container","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Component* container()"},{"name":"container","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Component* container() const"},{"name":"findContainerByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Component* findContainerByType(const std::type_info& type)"},{"name":"findContainerByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Component* findContainerByType(const std::type_info& type) const"},{"name":"container","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Component* container(Component* const p)"},{"name":"getNumberOfComponents","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getNumberOfComponents() const"},{"name":"getComponents","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"PairStream* getComponents()"},{"name":"getComponents","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const PairStream* getComponents() const"},{"name":"addComponent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addComponent(Pair* const)"},{"name":"findByName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Pair* findByName(const char* const slotname)"},{"name":"findByName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Pair* findByName(const char* const slotname) const"},{"name":"findByIndex","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Pair* findByIndex(const int slotindex)"},{"name":"findByIndex","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Pair* findByIndex(const int slotindex) const"},{"name":"findByType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Pair* findByType(const std::type_info& type)"},{"name":"findByType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Pair* findByType(const std::type_info& type) const"},{"name":"isComponentSelected","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isComponentSelected() const"},{"name":"getSelectedComponent","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Component* getSelectedComponent()"},{"name":"getSelectedComponent","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Component* getSelectedComponent() const"},{"name":"findNameOfComponent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Identifier* findNameOfComponent(const Component* const) const"},{"name":"updateTC","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateTC(const double dt = 0.0)"},{"name":"updateData","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateData(const double dt = 0.0)"},{"name":"tcFrame","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void tcFrame(const double dt = 0.0)"},{"name":"isFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isFrozen() const"},{"name":"isNotFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isNotFrozen() const"},{"name":"freeze","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void freeze(const bool)"},{"name":"reset","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void reset()"},{"name":"isShutdown","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isShutdown() const"},{"name":"isNotShutdown","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isNotShutdown() const"},{"name":"event","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool event(const int event, Object* const obj = nullptr)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const int value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const float value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const double value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const char* const value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const bool value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, Object* const value, SendData&)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const int value[], SendData sd[], const int n)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const float value[], SendData sd[], const int n)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const double value[], SendData sd[], const int n)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const char* const value[], SendData sd[], const int n)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, const bool value[], SendData sd[], const int n)"},{"name":"send","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool send(const char* const id, const int event, Object* const value[], SendData sd[], const int n)"},{"name":"getTimingStats","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Statistic* getTimingStats() const"},{"name":"isTimingStatsEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isTimingStatsEnabled() const"},{"name":"isTimingStatsPrintEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isTimingStatsPrintEnabled() const"},{"name":"setTimingStatsEnabled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setTimingStatsEnabled(const bool)"},{"name":"setPrintTimingStats","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPrintTimingStats(const bool)"},{"name":"isMessageEnabled","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":true,"signature":"bool isMessageEnabled(const unsigned short msgType) const override"},{"name":"printTimingStats","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void printTimingStats()"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool shutdownNotification()"},{"name":"onEventReset","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onEventReset()"},{"name":"setSelectionName","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSelectionName(const Object* const s)"},{"name":"select","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool select(const String* const name)"},{"name":"select","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool select(const Number* const num)"},{"name":"processComponents","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processComponents( PairStream* const list, const std::type_info& filter, Pair* const add = nullptr, Component* const remove = nullptr )"},{"name":"setSlotComponent","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotComponent(PairStream* const multiple)"},{"name":"setSlotComponent","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotComponent(Component* const single)"},{"name":"setSlotSelect","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSelect(const String* const name)"},{"name":"setSlotSelect","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSelect(const Number* const num)"},{"name":"setSlotEnableTimingStats","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableTimingStats(const Number* const)"},{"name":"setSlotPrintTimingStats","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotPrintTimingStats(const Number* const)"},{"name":"setSlotFreeze","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotFreeze(const Number* const)"},{"name":"setSlotEnableMsgType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableMsgType(const Identifier* const)"},{"name":"setSlotEnableMsgType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableMsgType(const Number* const)"},{"name":"setSlotDisableMsgType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDisableMsgType(const Identifier* const)"},{"name":"setSlotDisableMsgType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDisableMsgType(const Number* const)"}]},"Pair":{"file":"contexts/src/mixr/include/mixr/base/Pair.hpp","namespace":"mixr::base","base":"Object","attributes":[{"name":"slotname","type":"Identifier*","visibility":"private","static":false}],"components":[{"name":"obj","target":"Object","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Pair","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pair(const char* slot, Object* object)"},{"name":"slot","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Identifier* slot()"},{"name":"slot","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Identifier* slot() const"},{"name":"object","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Object* object()"},{"name":"object","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Object* object() const"},{"name":"isValid","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":true,"signature":"bool isValid() const override"}]},"PairStream":{"file":"contexts/src/mixr/include/mixr/base/PairStream.hpp","namespace":"mixr::base","base":"List","attributes":[],"components":[{"name":"(contents)","target":"Pair","targetTier":1,"multiplicity":"many","resolvedVia":"inferred-from-methods","visibility":"public"}],"methods":[{"name":"PairStream","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"PairStream()"},{"name":"operator==","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool operator==(const PairStream& stream) const"},{"name":"operator!=","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool operator!=(const PairStream& stream) const"},{"name":"findByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pair* findByType(const std::type_info& type)"},{"name":"findByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Pair* findByType(const std::type_info& type) const"},{"name":"findByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pair* findByName(const char* const slotname)"},{"name":"findByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Pair* findByName(const char* const slotname) const"},{"name":"findName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Identifier* findName(const Object* const obj) const"},{"name":"getPosition","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pair* getPosition(const unsigned int n)"},{"name":"getPosition","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Pair* getPosition(const unsigned int n) const"},{"name":"get","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pair* get()"},{"name":"put","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void put(Pair* pair1)"},{"name":"remove","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool remove(Pair* pair1)"}]},"System":{"file":"contexts/src/mixr/include/mixr/models/system/System.hpp","namespace":"mixr::models","base":"Component","attributes":[{"name":"pwrSw","type":"unsigned int","visibility":"private","static":false}],"components":[{"name":"ownship","target":"Player","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"System","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"System()"},{"name":"getPowerSwitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual unsigned int getPowerSwitch() const"},{"name":"setPowerSwitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPowerSwitch(const unsigned int p)"},{"name":"killedNotification","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool killedNotification(Player* const killedBy = 0)"},{"name":"getOwnship","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Player* getOwnship()"},{"name":"getOwnship","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Player* getOwnship() const"},{"name":"updateData","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateData(const double dt = 0.0) override"},{"name":"updateTC","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateTC(const double dt = 0.0) override"},{"name":"event","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool event(const int event, base::Object* const obj = nullptr) override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"isFrozen","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":true,"signature":"bool isFrozen() const override"},{"name":"getWorldModel","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual WorldModel* getWorldModel()"},{"name":"getWorldModel","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const WorldModel* getWorldModel() const"},{"name":"dynamics","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void dynamics(const double dt)"},{"name":"transmit","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void transmit(const double dt)"},{"name":"receive","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void receive(const double dt)"},{"name":"process","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void process(const double dt)"},{"name":"findOwnship","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool findOwnship()"},{"name":"setSlotPowerSwitch","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotPowerSwitch(const base::String* const)"}]},"AbstractPlayer":{"file":"contexts/src/mixr/include/mixr/simulation/AbstractPlayer.hpp","namespace":"mixr::simulation","base":"Component","attributes":[{"name":"mode","type":"Mode","visibility":"protected","static":false},{"name":"id","type":"unsigned short","visibility":"private","static":false},{"name":"pname","type":"base::Identifier","visibility":"private","static":false},{"name":"initMode","type":"Mode","visibility":"private","static":false},{"name":"netID","type":"int","visibility":"private","static":false},{"name":"enableNetOutput","type":"bool","visibility":"private","static":false}],"components":[{"name":"nib","target":"AbstractNib","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"protected"},{"name":"nibList","target":"AbstractNib","targetTier":2,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"AbstractPlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractPlayer()"},{"name":"setID","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setID(const unsigned short newId)"},{"name":"isID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isID(const unsigned short tst) const"},{"name":"getID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned short getID() const"},{"name":"setName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setName(const base::Identifier& newName)"},{"name":"setName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setName(const char* const newName)"},{"name":"isName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isName(const base::Identifier* const) const"},{"name":"isName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isName(const char* const) const"},{"name":"getName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getName() const"},{"name":"setMode","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setMode(const Mode newMode)"},{"name":"setInitMode","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setInitMode(const Mode newMode)"},{"name":"getMode","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"Mode getMode() const"},{"name":"isActive","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isActive() const"},{"name":"isKilled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isKilled() const"},{"name":"isCrashed","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isCrashed() const"},{"name":"isDetonated","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isDetonated() const"},{"name":"isInactive","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isInactive() const"},{"name":"isMode","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isMode(const Mode tst) const"},{"name":"isNotMode","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isNotMode(const Mode tst) const"},{"name":"isDead","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isDead() const"},{"name":"isNetworkedPlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isNetworkedPlayer() const"},{"name":"isLocalPlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isLocalPlayer() const"},{"name":"getNetworkID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"int getNetworkID() const"},{"name":"getNib","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractNib* getNib()"},{"name":"getNib","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractNib* getNib() const"},{"name":"isNetOutputEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isNetOutputEnabled() const"},{"name":"getLocalNib","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractNib* getLocalNib(const unsigned int netId)"},{"name":"getLocalNib","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractNib* getLocalNib(const unsigned int netId) const"},{"name":"setNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setNib(AbstractNib* const p)"},{"name":"setEnableNetOutput","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setEnableNetOutput(const bool f)"},{"name":"setOutgoingNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setOutgoingNib(AbstractNib* const p, const unsigned int id)"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"initData","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void initData()"},{"name":"setSlotID","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotID(const base::Number* const)"},{"name":"setSlotInitMode","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitMode(base::String* const)"}]},"Player":{"file":"contexts/src/mixr/include/mixr/models/player/Player.hpp","namespace":"mixr::models","base":"AbstractPlayer","attributes":[{"name":"type","type":"base::safe_ptr<base::String>","visibility":"private","static":false},{"name":"side","type":"Side","visibility":"private","static":false},{"name":"useCoordSys","type":"CoordSys","visibility":"private","static":false},{"name":"useCoordSysN1","type":"CoordSys","visibility":"private","static":false},{"name":"latitude","type":"double","visibility":"private","static":false},{"name":"longitude","type":"double","visibility":"private","static":false},{"name":"altitude","type":"double","visibility":"private","static":false},{"name":"posVecNED","type":"base::Vec3d","visibility":"private","static":false},{"name":"posVecECEF","type":"base::Vec3d","visibility":"private","static":false},{"name":"velVecNED","type":"base::Vec3d","visibility":"private","static":false},{"name":"velVecECEF","type":"base::Vec3d","visibility":"private","static":false},{"name":"velVecBody","type":"base::Vec3d","visibility":"private","static":false},{"name":"velVecN1","type":"base::Vec3d","visibility":"private","static":false},{"name":"accelVecNED","type":"base::Vec3d","visibility":"private","static":false},{"name":"accelVecECEF","type":"base::Vec3d","visibility":"private","static":false},{"name":"accelVecBody","type":"base::Vec3d","visibility":"private","static":false},{"name":"vp","type":"double","visibility":"private","static":false},{"name":"gndSpd","type":"double","visibility":"private","static":false},{"name":"gndTrk","type":"double","visibility":"private","static":false},{"name":"angles","type":"base::Vec3d","visibility":"private","static":false},{"name":"scPhi","type":"base::Vec2d","visibility":"private","static":false},{"name":"scTheta","type":"base::Vec2d","visibility":"private","static":false},{"name":"scPsi","type":"base::Vec2d","visibility":"private","static":false},{"name":"anglesW","type":"base::Vec3d","visibility":"private","static":false},{"name":"scPhiW","type":"base::Vec2d","visibility":"private","static":false},{"name":"scThetaW","type":"base::Vec2d","visibility":"private","static":false},{"name":"scPsiW","type":"base::Vec2d","visibility":"private","static":false},{"name":"angularVel","type":"base::Vec3d","visibility":"private","static":false},{"name":"gcAngVel","type":"base::Vec3d","visibility":"private","static":false},{"name":"q","type":"base::Quat","visibility":"private","static":false},{"name":"rm","type":"base::Matrixd","visibility":"private","static":false},{"name":"wm","type":"base::Matrixd","visibility":"private","static":false},{"name":"rmW2B","type":"base::Matrixd","visibility":"private","static":false},{"name":"tElev","type":"double","visibility":"private","static":false},{"name":"tElevValid","type":"bool","visibility":"private","static":false},{"name":"tElevReq","type":"bool","visibility":"private","static":false},{"name":"interpTrrn","type":"bool","visibility":"private","static":false},{"name":"tOffset","type":"double","visibility":"private","static":false},{"name":"posVecValid","type":"bool","visibility":"private","static":false},{"name":"altSlaved","type":"bool","visibility":"private","static":false},{"name":"posSlaved","type":"bool","visibility":"private","static":false},{"name":"posFrz","type":"bool","visibility":"private","static":false},{"name":"altFrz","type":"bool","visibility":"private","static":false},{"name":"attFrz","type":"bool","visibility":"private","static":false},{"name":"fuelFrz","type":"bool","visibility":"private","static":false},{"name":"crashOverride","type":"bool","visibility":"private","static":false},{"name":"killOverride","type":"bool","visibility":"private","static":false},{"name":"killRemoval","type":"bool","visibility":"private","static":false},{"name":"camouflage","type":"unsigned int","visibility":"private","static":false},{"name":"damage","type":"double","visibility":"private","static":false},{"name":"smoking","type":"double","visibility":"private","static":false},{"name":"flames","type":"double","visibility":"private","static":false},{"name":"justKilled","type":"bool","visibility":"private","static":false},{"name":"killedBy","type":"unsigned short","visibility":"private","static":false},{"name":"initPosVec","type":"base::Vec2d","visibility":"private","static":false},{"name":"initPosFlg","type":"bool","visibility":"private","static":false},{"name":"initGeoPosVec","type":"base::Vec3d","visibility":"private","static":false},{"name":"initGeoPosFlg","type":"bool","visibility":"private","static":false},{"name":"initLat","type":"double","visibility":"private","static":false},{"name":"initLon","type":"double","visibility":"private","static":false},{"name":"initLatLonFlg","type":"bool","visibility":"private","static":false},{"name":"initAlt","type":"double","visibility":"private","static":false},{"name":"initVp","type":"double","visibility":"private","static":false},{"name":"initAngles","type":"base::Vec3d","visibility":"private","static":false},{"name":"testAngRates","type":"base::Vec3d","visibility":"private","static":false},{"name":"testBodyAxis","type":"bool","visibility":"private","static":false},{"name":"dataLogTimer","type":"double","visibility":"private","static":false},{"name":"dataLogTime","type":"double","visibility":"private","static":false},{"name":"loadSysPtrs","type":"bool","visibility":"private","static":false},{"name":"MAX_RF_REFLECTIONS","type":"const unsigned int","visibility":"private","static":true},{"name":"rfReflectTimer","type":"std::array<double, MAX_RF_REFLECTIONS>","visibility":"private","static":false},{"name":"syncState1Ready","type":"bool","visibility":"private","static":false},{"name":"syncState2Ready","type":"bool","visibility":"private","static":false}],"components":[{"name":"signature","target":"RfSignature","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"irSignature","target":"IrSignature","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"sim","target":"WorldModel","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"rfReflect","target":"Component","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"syncState1","target":"SynchronizedState","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"syncState2","target":"SynchronizedState","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"dynamicsModel","target":"DynamicsModel","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"datalink","target":"Datalink","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"gimbal","target":"Gimbal","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"irSystem","target":"IrSystem","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"navigation","target":"Navigation","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"onboardComputer","target":"OnboardComputer","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"pilot","target":"Pilot","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"radio","target":"Radio","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"rfSensor","target":"RfSensor","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"},{"name":"storesMgr","target":"StoresMgr","targetTier":2,"multiplicity":"one","resolvedVia":"typeid()-in-setter-body","visibility":"private"}],"methods":[{"name":"Player","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Player()"},{"name":"getMajorType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual unsigned int getMajorType() const"},{"name":"isMajorType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isMajorType(const unsigned int tst) const"},{"name":"getType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::String* getType() const"},{"name":"getSide","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual Side getSide() const"},{"name":"isSide","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isSide(const unsigned int tst) const"},{"name":"isNotSide","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isNotSide(const unsigned int tst) const"},{"name":"getRoll","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getRoll() const"},{"name":"getRollR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getRollR() const"},{"name":"getRollD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getRollD() const"},{"name":"getSinRoll","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getSinRoll() const"},{"name":"getCosRoll","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCosRoll() const"},{"name":"getPitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getPitch() const"},{"name":"getPitchR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getPitchR() const"},{"name":"getPitchD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getPitchD() const"},{"name":"getSinPitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getSinPitch() const"},{"name":"getCosPitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCosPitch() const"},{"name":"getHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getHeading() const"},{"name":"getHeadingR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getHeadingR() const"},{"name":"getHeadingD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getHeadingD() const"},{"name":"getSinHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getSinHeading() const"},{"name":"getCosHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCosHeading() const"},{"name":"getEulerAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getEulerAngles() const"},{"name":"getQuaternions","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Quat& getQuaternions() const"},{"name":"getRotMat","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Matrixd& getRotMat() const"},{"name":"getRotMatW2B","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Matrixd& getRotMatW2B() const"},{"name":"getGeocEulerAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getGeocEulerAngles() const"},{"name":"getAngularVelocities","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getAngularVelocities() const"},{"name":"getGeocAngularVelocities","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getGeocAngularVelocities() const"},{"name":"getGeocPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getGeocPosition() const"},{"name":"getLatitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getLatitude() const"},{"name":"getLongitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getLongitude() const"},{"name":"getWorldMat","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Matrixd& getWorldMat() const"},{"name":"getEarthRadius","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getEarthRadius() const"},{"name":"getPositionLL","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool getPositionLL(double* const lat, double* const lon) const"},{"name":"getPositionLLA","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool getPositionLLA(double* const lat, double* const lon, double* const alt) const"},{"name":"getXPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getXPosition() const"},{"name":"getYPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getYPosition() const"},{"name":"getPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getPosition() const"},{"name":"isPositionVectorValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isPositionVectorValid() const"},{"name":"getAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitude() const"},{"name":"getAltitudeM","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitudeM() const"},{"name":"getAltitudeFt","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitudeFt() const"},{"name":"getAltitudeAgl","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitudeAgl() const"},{"name":"getAltitudeAglM","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitudeAglM() const"},{"name":"getAltitudeAglFt","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getAltitudeAglFt() const"},{"name":"isTerrainElevationValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isTerrainElevationValid() const"},{"name":"getTerrainElevation","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTerrainElevation() const"},{"name":"getTerrainElevationM","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTerrainElevationM() const"},{"name":"getTerrainElevationFt","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTerrainElevationFt() const"},{"name":"getTotalVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTotalVelocity() const"},{"name":"getTotalVelocityFPS","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTotalVelocityFPS() const"},{"name":"getTotalVelocityKts","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTotalVelocityKts() const"},{"name":"getGroundSpeed","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundSpeed() const"},{"name":"getGroundSpeedFPS","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundSpeedFPS() const"},{"name":"getGroundSpeedKts","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundSpeedKts() const"},{"name":"getGroundTrack","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundTrack() const"},{"name":"getGroundTrackR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundTrackR() const"},{"name":"getGroundTrackD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGroundTrackD() const"},{"name":"getVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getVelocity() const"},{"name":"getAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getAcceleration() const"},{"name":"getGeocVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getGeocVelocity() const"},{"name":"getGeocAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getGeocAcceleration() const"},{"name":"getVelocityBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getVelocityBody() const"},{"name":"getAccelerationBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getAccelerationBody() const"},{"name":"getGrossWeight","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getGrossWeight() const"},{"name":"getMach","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMach() const"},{"name":"getCG","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCG() const"},{"name":"getRFSignature","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"inline virtual RfSignature* const getRFSignature()"},{"name":"getIRSignature","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"inline virtual IrSignature* const getIRSignature()"},{"name":"getCamouflageType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual unsigned int getCamouflageType() const"},{"name":"isDestroyed","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isDestroyed() const"},{"name":"getDamage","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getDamage() const"},{"name":"getSmoke","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getSmoke() const"},{"name":"getFlames","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getFlames() const"},{"name":"isJustKilled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isJustKilled() const"},{"name":"killedByPlayerID","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual int killedByPlayerID() const"},{"name":"getInitGeocentricPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getInitGeocentricPosition() const"},{"name":"isInitGeocentricPositionValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isInitGeocentricPositionValid() const"},{"name":"getInitLatitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getInitLatitude() const"},{"name":"getInitLongitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getInitLongitude() const"},{"name":"isInitLatLonValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isInitLatLonValid() const"},{"name":"getInitVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getInitVelocity() const"},{"name":"getInitPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec2d& getInitPosition() const"},{"name":"isInitPositionValid","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isInitPositionValid() const"},{"name":"getInitAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getInitAltitude() const"},{"name":"getInitAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::Vec3d& getInitAngles() const"},{"name":"isPositionFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isPositionFrozen() const"},{"name":"isAltitudeFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isAltitudeFrozen() const"},{"name":"isAttitudeFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isAttitudeFrozen() const"},{"name":"isFuelFrozen","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isFuelFrozen() const"},{"name":"isCrashOverride","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isCrashOverride() const"},{"name":"isKillOverride","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isKillOverride() const"},{"name":"isKillRemovalEnabled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isKillRemovalEnabled() const"},{"name":"isAltitudeSlaved","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isAltitudeSlaved() const"},{"name":"isPositionSlaved","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isPositionSlaved() const"},{"name":"getTerrainOffset","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getTerrainOffset() const"},{"name":"isDtedTerrainInterpolationEnabled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isDtedTerrainInterpolationEnabled() const"},{"name":"isTerrainElevationRequired","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isTerrainElevationRequired() const"},{"name":"getCoordSystemInUse","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual CoordSys getCoordSystemInUse() const"},{"name":"isHeadingHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isHeadingHoldOn() const"},{"name":"getCommandedHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedHeading() const"},{"name":"getCommandedHeadingD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedHeadingD() const"},{"name":"getCommandedHeadingR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedHeadingR() const"},{"name":"isVelocityHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isVelocityHoldOn() const"},{"name":"getCommandedVelocityKts","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedVelocityKts() const"},{"name":"getCommandedVelocityFps","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedVelocityFps() const"},{"name":"getCommandedVelocityMps","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedVelocityMps() const"},{"name":"isAltitudeHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual bool isAltitudeHoldOn() const"},{"name":"getCommandedAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedAltitude() const"},{"name":"getCommandedAltitudeM","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedAltitudeM() const"},{"name":"getCommandedAltitudeFt","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getCommandedAltitudeFt() const"},{"name":"getSynchronizedState","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const SynchronizedState& getSynchronizedState() const"},{"name":"getWorldModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"WorldModel* getWorldModel()"},{"name":"getWorldModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const WorldModel* getWorldModel() const"},{"name":"getDynamicsModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"DynamicsModel* getDynamicsModel()"},{"name":"getDynamicsModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const DynamicsModel* getDynamicsModel() const"},{"name":"getDynamicsModelName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getDynamicsModelName() const"},{"name":"getPilot","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pilot* getPilot()"},{"name":"getPilot","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Pilot* getPilot() const"},{"name":"getPilotName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getPilotName() const"},{"name":"getPilotByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Pilot* getPilotByName(const char* const)"},{"name":"getPilotByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getPilotByType(const std::type_info& type)"},{"name":"getStoresManagement","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"StoresMgr* getStoresManagement()"},{"name":"getStoresManagement","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const StoresMgr* getStoresManagement() const"},{"name":"getStoresManagementName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getStoresManagementName() const"},{"name":"getDatalink","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Datalink* getDatalink()"},{"name":"getDatalink","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Datalink* getDatalink() const"},{"name":"getDatalinkName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getDatalinkName() const"},{"name":"getDatalinkByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Datalink* getDatalinkByName(const char* const)"},{"name":"getDatalinkByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getDatalinkByType(const std::type_info& type)"},{"name":"getGimbal","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Gimbal* getGimbal()"},{"name":"getGimbal","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Gimbal* getGimbal() const"},{"name":"getGimbalName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getGimbalName() const"},{"name":"getGimbalByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Gimbal* getGimbalByName(const char* const)"},{"name":"getGimbalByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getGimbalByType(const std::type_info& type)"},{"name":"getNavigation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Navigation* getNavigation()"},{"name":"getNavigation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Navigation* getNavigation() const"},{"name":"getNavigationName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getNavigationName() const"},{"name":"getNavigationByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Navigation* getNavigationByName(const char* const)"},{"name":"getNavigationByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getNavigationByType(const std::type_info& type)"},{"name":"getOnboardComputer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"OnboardComputer* getOnboardComputer()"},{"name":"getOnboardComputer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const OnboardComputer* getOnboardComputer() const"},{"name":"getOnboardComputerName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getOnboardComputerName() const"},{"name":"getOnboardComputerByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"OnboardComputer* getOnboardComputerByName(const char* const)"},{"name":"getOnboardComputerByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getOnboardComputerByType(const std::type_info& type)"},{"name":"getRadio","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Radio* getRadio()"},{"name":"getRadio","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Radio* getRadio() const"},{"name":"getRadioName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getRadioName() const"},{"name":"getRadioByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Radio* getRadioByName(const char* const)"},{"name":"getRadioByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getRadioByType(const std::type_info& type)"},{"name":"getSensor","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"RfSensor* getSensor()"},{"name":"getSensor","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const RfSensor* getSensor() const"},{"name":"getSensorName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getSensorName() const"},{"name":"getSensorByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"RfSensor* getSensorByName(const char* const)"},{"name":"getSensorByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getSensorByType(const std::type_info& type)"},{"name":"getIrSystem","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"IrSystem* getIrSystem()"},{"name":"getIrSystem","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const IrSystem* getIrSystem() const"},{"name":"getIrSystemName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Identifier* getIrSystemName() const"},{"name":"getIrSystemByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"IrSystem* getIrSystemByName(const char* const)"},{"name":"getIrSystemByType","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Pair* getIrSystemByType(const std::type_info& type)"},{"name":"setType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setType(const base::String* const newTypeString)"},{"name":"setSide","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setSide(const Side)"},{"name":"setUseCoordSys","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setUseCoordSys(const CoordSys)"},{"name":"setFuelFreeze","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setFuelFreeze(const bool)"},{"name":"setCrashOverride","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCrashOverride(const bool)"},{"name":"setKillOverride","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setKillOverride(const bool)"},{"name":"setKillRemoval","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setKillRemoval(const bool)"},{"name":"resetJustKilled","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void resetJustKilled()"},{"name":"setDamage","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setDamage(const double)"},{"name":"setSmoke","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSmoke(const double)"},{"name":"setFlames","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setFlames(const double)"},{"name":"setCamouflageType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCamouflageType(const unsigned int)"},{"name":"setPositionFreeze","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPositionFreeze(const bool)"},{"name":"setAltitudeFreeze","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAltitudeFreeze(const bool)"},{"name":"setAttitudeFreeze","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAttitudeFreeze(const bool)"},{"name":"setHeadingHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setHeadingHoldOn(const bool)"},{"name":"setCommandedHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedHeading(const double)"},{"name":"setCommandedHeadingD","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedHeadingD(const double)"},{"name":"setCommandedHeadingR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedHeadingR(const double)"},{"name":"setVelocityHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setVelocityHoldOn(const bool)"},{"name":"setCommandedVelocityKts","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedVelocityKts(const double)"},{"name":"setAltitudeHoldOn","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAltitudeHoldOn(const bool)"},{"name":"setCommandedAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedAltitude(const double)"},{"name":"setCommandedAltitudeM","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedAltitudeM(const double)"},{"name":"setCommandedAltitudeFt","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setCommandedAltitudeFt(const double)"},{"name":"setTerrainElevation","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setTerrainElevation(const double)"},{"name":"setTerrainOffset","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setTerrainOffset(const double)"},{"name":"setInterpolateTerrain","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInterpolateTerrain(const bool)"},{"name":"setTerrainElevationRequired","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setTerrainElevationRequired(const bool)"},{"name":"setAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAltitude(const double alt, const bool slaved = false)"},{"name":"setPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPosition(const double north, const double east, const bool slaved = false)"},{"name":"setPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPosition(const double north, const double east, const double down, const bool slaved = false)"},{"name":"setPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPosition(const base::Vec3d& newPos, const bool slaved = false)"},{"name":"setPositionLL","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPositionLL(const double lat, const double lon, const bool slaved = false)"},{"name":"setPositionLLA","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPositionLLA(const double lat, const double lon, const double alt, const bool slaved = false)"},{"name":"setGeocPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocPosition(const base::Vec3d& gcPos, const bool slaved = false)"},{"name":"setEulerAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setEulerAngles(const double r, const double p, const double y)"},{"name":"setEulerAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setEulerAngles(const base::Vec3d& newAngles)"},{"name":"setGeocEulerAngles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocEulerAngles(const base::Vec3d& newAngles)"},{"name":"setRotMat","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setRotMat(const base::Matrixd&)"},{"name":"setQuaternions","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setQuaternions(const base::Quat&)"},{"name":"setInitPitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitPitch(const base::Angle* const)"},{"name":"setInitPitch","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitPitch(const base::Number* const)"},{"name":"setInitHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitHeading(const base::Angle* const)"},{"name":"setInitHeading","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitHeading(const base::Number* const)"},{"name":"setAngularVelocities","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAngularVelocities(const double pa, const double qa, const double ra)"},{"name":"setAngularVelocities","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAngularVelocities(const base::Vec3d&)"},{"name":"setGeocAngularVelocities","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocAngularVelocities(const base::Vec3d&)"},{"name":"setVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setVelocity(const double ue, const double ve, const double we)"},{"name":"setVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setVelocity(const base::Vec3d& newVel)"},{"name":"setAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAcceleration(const double due, const double dve, const double dwe)"},{"name":"setAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAcceleration(const base::Vec3d& newAccel)"},{"name":"setVelocityBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setVelocityBody(const double ua, const double va, const double wa)"},{"name":"setVelocityBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setVelocityBody(const base::Vec3d& newVelBody)"},{"name":"setAccelerationBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAccelerationBody(const double dua, const double dva, const double dwa)"},{"name":"setAccelerationBody","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setAccelerationBody(const base::Vec3d& newAccelBody)"},{"name":"setGeocVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocVelocity(const double vx, const double vy, const double vz)"},{"name":"setGeocVelocity","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocVelocity(const base::Vec3d& newVelEcef)"},{"name":"setGeocAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocAcceleration(const double dvx, const double dvy, const double dvz)"},{"name":"setGeocAcceleration","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGeocAcceleration(const base::Vec3d& newAccelEcef)"},{"name":"setInitLat","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitLat(const double)"},{"name":"setInitLon","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitLon(const double)"},{"name":"setInitAltitude","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitAltitude(const double)"},{"name":"setInitGeocentricPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitGeocentricPosition(const base::Vec3d&)"},{"name":"setInitPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitPosition(const double north, const double east)"},{"name":"setInitPosition","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitPosition(const base::Vec2d& newPos)"},{"name":"setControlStickRollInput","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setControlStickRollInput(const double)"},{"name":"setControlStickPitchInput","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setControlStickPitchInput(const double)"},{"name":"setThrottles","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual int setThrottles(const double* const positions, const int num)"},{"name":"processDetonation","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processDetonation(const double detRange, AbstractWeapon* const wpn = nullptr)"},{"name":"killedNotification","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool killedNotification(Player* const killedBy = nullptr)"},{"name":"collisionNotification","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool collisionNotification(Player* const)"},{"name":"crashNotification","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool crashNotification()"},{"name":"onWpnRelEvent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onWpnRelEvent(const base::Boolean* const msg = nullptr)"},{"name":"onTriggerSwEvent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onTriggerSwEvent(const base::Boolean* const msg = nullptr)"},{"name":"onTgtStepEvent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onTgtStepEvent()"},{"name":"onRfEmissionEventPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onRfEmissionEventPlayer(Emission* const)"},{"name":"onRfReflectedEmissionEventPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onRfReflectedEmissionEventPlayer(Emission* const)"},{"name":"onReflectionsRequest","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onReflectionsRequest(base::Component* const)"},{"name":"onReflectionsCancel","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onReflectionsCancel(const base::Component* const)"},{"name":"onIrMsgEventPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onIrMsgEventPlayer(IrQueryMsg* const)"},{"name":"onDatalinkMessageEventPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onDatalinkMessageEventPlayer(base::Object* const)"},{"name":"onDeEmissionEvent","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool onDeEmissionEvent(base::Object* const)"},{"name":"isFrozen","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":true,"signature":"bool isFrozen() const override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"updateTC","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateTC(const double dt = 0.0) override"},{"name":"updateData","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateData(const double dt = 0.0) override"},{"name":"event","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool event(const int event, base::Object* const obj = nullptr) override"},{"name":"dynamics","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void dynamics(const double dt = 0.0)"},{"name":"positionUpdate","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void positionUpdate(const double dt)"},{"name":"deadReckonPosition","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void deadReckonPosition(const double dt)"},{"name":"updateSystemPointers","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateSystemPointers()"},{"name":"updateElevation","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateElevation()"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"printTimingStats","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void printTimingStats() override"},{"name":"setDynamicsModel","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setDynamicsModel(base::Pair* const)"},{"name":"setDatalink","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setDatalink(base::Pair* const)"},{"name":"setGimbal","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGimbal(base::Pair* const)"},{"name":"setNavigation","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setNavigation(base::Pair* const)"},{"name":"setOnboardComputer","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setOnboardComputer(base::Pair* const)"},{"name":"setPilot","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setPilot(base::Pair* const)"},{"name":"setRadio","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setRadio(base::Pair* const)"},{"name":"setSensor","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSensor(base::Pair* const)"},{"name":"setIrSystem","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setIrSystem(base::Pair* const)"},{"name":"setStoresMgr","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setStoresMgr(base::Pair* const)"},{"name":"processComponents","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void processComponents( base::PairStream* const list, const std::type_info& filter, base::Pair* const add = nullptr, base::Component* const remove = nullptr ) override"},{"name":"setSlotSignature","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSignature(RfSignature *const)"},{"name":"initData","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void initData()"},{"name":"getSimulationImp","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"WorldModel* getSimulationImp()"},{"name":"setSlotInitXPos","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitXPos(const base::Distance* const)"},{"name":"setSlotInitXPos","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitXPos(const base::Number* const)"},{"name":"setSlotInitYPos","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitYPos(const base::Distance* const)"},{"name":"setSlotInitYPos","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitYPos(const base::Number* const)"},{"name":"setSlotInitAlt","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitAlt(const base::Distance* const)"},{"name":"setSlotInitAlt","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitAlt(const base::Number* const)"},{"name":"setSlotInitPosition","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitPosition(const base::List* const)"},{"name":"setSlotInitLat","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLat(const base::LatLon* const)"},{"name":"setSlotInitLat","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLat(const base::Angle* const)"},{"name":"setSlotInitLat","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLat(const base::Number* const)"},{"name":"setSlotInitLon","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLon(const base::LatLon* const)"},{"name":"setSlotInitLon","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLon(const base::Angle* const)"},{"name":"setSlotInitLon","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitLon(const base::Number* const)"},{"name":"setSlotInitGeocentric","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitGeocentric(const base::List* const)"},{"name":"setSlotInitRoll","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitRoll(const base::Angle* const)"},{"name":"setSlotInitRoll","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitRoll(const base::Number* const)"},{"name":"setSlotInitPitch","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitPitch(const base::Angle* const x)"},{"name":"setSlotInitPitch","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitPitch(const base::Number* const x)"},{"name":"setSlotInitHeading","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitHeading(const base::Angle* const x)"},{"name":"setSlotInitHeading","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitHeading(const base::Number* const x)"},{"name":"setSlotInitEulerAngles","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitEulerAngles(const base::List* const)"},{"name":"setSlotInitVelocity","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitVelocity(const base::Number* const)"},{"name":"setSlotInitVelocityKts","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInitVelocityKts(const base::Number* const)"},{"name":"setSlotType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotType(const base::String* const x)"},{"name":"setSlotSide","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSide(base::String* const)"},{"name":"setSlotIrSignature","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotIrSignature(IrSignature* const)"},{"name":"setSlotCamouflageType","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotCamouflageType(const base::Number* const)"},{"name":"setSlotTerrainElevReq","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTerrainElevReq(const base::Number* const)"},{"name":"setSlotInterpolateTerrain","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInterpolateTerrain(const base::Number* const)"},{"name":"setSlotTerrainOffset","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTerrainOffset(const base::Distance* const)"},{"name":"setSlotPositionFreeze","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotPositionFreeze(const base::Number* const)"},{"name":"setSlotAltitudeFreeze","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotAltitudeFreeze(const base::Number* const)"},{"name":"setSlotAttitudeFreeze","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotAttitudeFreeze(const base::Number* const)"},{"name":"setSlotFuelFreeze","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotFuelFreeze(const base::Number* const)"},{"name":"setSlotCrashOverride","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotCrashOverride(const base::Number* const)"},{"name":"setSlotKillOverride","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotKillOverride(const base::Number* const)"},{"name":"setSlotKillRemoval","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotKillRemoval(const base::Number* const)"},{"name":"setSlotEnableNetOutput","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableNetOutput(const base::Number* const)"},{"name":"setSlotDataLogTime","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDataLogTime(const base::Time* const)"},{"name":"setSlotTestRollRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTestRollRate(const base::Angle* const)"},{"name":"setSlotTestPitchRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTestPitchRate(const base::Angle* const)"},{"name":"setSlotTestYawRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTestYawRate(const base::Angle* const)"},{"name":"setSlotTestBodyAxis","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTestBodyAxis(const base::Number* const)"},{"name":"setSlotUseCoordSys","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotUseCoordSys(base::String* const)"}]},"Station":{"file":"contexts/src/mixr/include/mixr/simulation/Station.hpp","namespace":"mixr::simulation","base":"Component","attributes":[{"name":"DEFAULT_TC_THREAD_PRI","type":"const double","visibility":"public","static":true},{"name":"DEFAULT_BG_THREAD_PRI","type":"const double","visibility":"public","static":true},{"name":"DEFAULT_NET_THREAD_PRI","type":"const double","visibility":"public","static":true},{"name":"DEFAULT_FAST_FORWARD_RATE","type":"const unsigned int","visibility":"public","static":true},{"name":"ownshipName","type":"const base::String*","visibility":"private","static":false},{"name":"tmrUpdateEnbl","type":"bool","visibility":"private","static":false},{"name":"tcRate","type":"double","visibility":"private","static":false},{"name":"tcPri","type":"double","visibility":"private","static":false},{"name":"tcStackSize","type":"unsigned int","visibility":"private","static":false},{"name":"tcThread","type":"base::safe_ptr<StationTcPeriodicThread>","visibility":"private","static":false},{"name":"fastForwardRate","type":"unsigned int","visibility":"private","static":false},{"name":"netRate","type":"double","visibility":"private","static":false},{"name":"netPri","type":"double","visibility":"private","static":false},{"name":"netStackSize","type":"unsigned int","visibility":"private","static":false},{"name":"netThread","type":"base::safe_ptr<StationNetPeriodicThread>","visibility":"private","static":false},{"name":"bgRate","type":"double","visibility":"private","static":false},{"name":"bgPri","type":"double","visibility":"private","static":false},{"name":"bgStackSize","type":"unsigned int","visibility":"private","static":false},{"name":"bgThread","type":"base::safe_ptr<StationBgPeriodicThread>","visibility":"private","static":false},{"name":"startupResetTimer","type":"double","visibility":"private","static":false},{"name":"startupResetTimer0","type":"const base::Time*","visibility":"private","static":false}],"components":[{"name":"sim","target":"Simulation","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"networks","target":"PairStream","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"igHosts","target":"PairStream","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"ioHandler","target":"AbstractIoHandler","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"ownship","target":"AbstractPlayer","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"dataRecorder","target":"AbstractDataRecorder","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Station","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Station()"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Simulation* getSimulation()"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Simulation* getSimulation() const"},{"name":"getPlayers","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::PairStream* getPlayers()"},{"name":"getPlayers","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::PairStream* getPlayers() const"},{"name":"getOwnship","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractPlayer* getOwnship()"},{"name":"getOwnship","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractPlayer* getOwnship() const"},{"name":"getOwnshipName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::String* getOwnshipName() const"},{"name":"setOwnshipPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setOwnshipPlayer(AbstractPlayer* const newOS)"},{"name":"setOwnshipByName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setOwnshipByName(const char* const newOS)"},{"name":"getIgHostList","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::PairStream* getIgHostList()"},{"name":"getIgHostList","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::PairStream* getIgHostList() const"},{"name":"getNetworks","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::PairStream* getNetworks()"},{"name":"getNetworks","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::PairStream* getNetworks() const"},{"name":"getIoHandler","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::AbstractIoHandler* getIoHandler()"},{"name":"getIoHandler","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::AbstractIoHandler* getIoHandler() const"},{"name":"getDataRecorder","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractDataRecorder* getDataRecorder()"},{"name":"getDataRecorder","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractDataRecorder* getDataRecorder() const"},{"name":"setDataRecorder","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setDataRecorder(AbstractDataRecorder* const p)"},{"name":"isUpdateTimersEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isUpdateTimersEnabled() const"},{"name":"setUpdateTimersEnable","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setUpdateTimersEnable(const bool enb)"},{"name":"processTimeCriticalTasks","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processTimeCriticalTasks(const double dt)"},{"name":"processBackgroundTasks","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processBackgroundTasks(const double dt)"},{"name":"processNetworkInputTasks","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processNetworkInputTasks(const double dt)"},{"name":"processNetworkOutputTasks","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processNetworkOutputTasks(const double dt)"},{"name":"getTimeCriticalRate","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getTimeCriticalRate() const"},{"name":"getTimeCriticalPriority","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getTimeCriticalPriority() const"},{"name":"getTimeCriticalStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getTimeCriticalStackSize() const"},{"name":"setTimeCriticalStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setTimeCriticalStackSize(const unsigned int bytes)"},{"name":"createTimeCriticalProcess","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void createTimeCriticalProcess()"},{"name":"doWeHaveTheTcThread","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool doWeHaveTheTcThread() const"},{"name":"getFastForwardRate","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getFastForwardRate() const"},{"name":"setFastForwardRate","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setFastForwardRate(const unsigned int r)"},{"name":"getNetworkRate","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getNetworkRate() const"},{"name":"getNetworkPriority","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getNetworkPriority() const"},{"name":"getNetworkStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getNetworkStackSize() const"},{"name":"setNetworkStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setNetworkStackSize(const unsigned int bytes)"},{"name":"doWeHaveTheNetThread","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool doWeHaveTheNetThread() const"},{"name":"getBackgroundRate","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getBackgroundRate() const"},{"name":"getBackgroundPriority","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getBackgroundPriority() const"},{"name":"getBackgroundStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getBackgroundStackSize() const"},{"name":"setBackgroundStackSize","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setBackgroundStackSize(const unsigned int bytes)"},{"name":"doWeHaveTheBgThread","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool doWeHaveTheBgThread() const"},{"name":"updateTC","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateTC(const double dt = 0.0) override"},{"name":"updateData","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateData(const double dt = 0.0) override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"inputDevices","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void inputDevices(const double dt)"},{"name":"outputDevices","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void outputDevices(const double dt)"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"setTcThread","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setTcThread(StationTcPeriodicThread*)"},{"name":"setNetThread","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setNetThread(StationNetPeriodicThread*)"},{"name":"setBgThread","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setBgThread(StationBgPeriodicThread*)"},{"name":"createNetworkProcess","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void createNetworkProcess()"},{"name":"createBackgroundProcess","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void createBackgroundProcess()"},{"name":"setSlotSimulation","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSimulation(Simulation* const)"},{"name":"setSlotNetworks","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNetworks(base::PairStream* const)"},{"name":"setSlotIgHosts","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotIgHosts(base::PairStream* const)"},{"name":"setSlotIoHandler","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotIoHandler(base::AbstractIoHandler* const)"},{"name":"setSlotOwnshipName","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotOwnshipName(const base::String* const)"},{"name":"setSlotTimeCriticalRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTimeCriticalRate(const base::Number* const hz)"},{"name":"setSlotTimeCriticalPri","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTimeCriticalPri(const base::Number* const)"},{"name":"setSlotTimeCriticalStackSize","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTimeCriticalStackSize(const base::Number* const)"},{"name":"setSlotFastForwardRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotFastForwardRate(const base::Number* const)"},{"name":"setSlotNetworkRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNetworkRate(const base::Number* const hz)"},{"name":"setSlotNetworkPri","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNetworkPri(const base::Number* const)"},{"name":"setSlotNetworkStackSize","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNetworkStackSize(const base::Number* const)"},{"name":"setSlotBackgroundRate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotBackgroundRate(const base::Number* const hz)"},{"name":"setSlotBackgroundPri","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotBackgroundPri(const base::Number* const)"},{"name":"setSlotBackgroundStackSize","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotBackgroundStackSize(const base::Number* const)"},{"name":"setSlotStartupResetTime","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotStartupResetTime(const base::Time* const)"},{"name":"setSlotEnableUpdateTimers","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableUpdateTimers(const base::Number* const)"},{"name":"setSlotDataRecorder","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDataRecorder(AbstractDataRecorder* const x)"}]},"Simulation":{"file":"contexts/src/mixr/include/mixr/simulation/Simulation.hpp","namespace":"mixr::simulation","base":"Component","attributes":[{"name":"MIN_WPN_ID","type":"const unsigned short","visibility":"public","static":true},{"name":"MAX_NEW_PLAYERS","type":"const int","visibility":"public","static":true},{"name":"cycleCnt","type":"unsigned int","visibility":"private","static":false},{"name":"frameCnt","type":"unsigned int","visibility":"private","static":false},{"name":"phaseCnt","type":"unsigned int","visibility":"private","static":false},{"name":"execTime","type":"double","visibility":"private","static":false},{"name":"pcTime","type":"double","visibility":"private","static":false},{"name":"pcTvSec","type":"unsigned long","visibility":"private","static":false},{"name":"pcTvUSec","type":"unsigned long","visibility":"private","static":false},{"name":"simTime","type":"double","visibility":"private","static":false},{"name":"simTvSec","type":"unsigned long","visibility":"private","static":false},{"name":"simTvUSec","type":"unsigned long","visibility":"private","static":false},{"name":"simTimeSlaved","type":"bool","visibility":"private","static":false},{"name":"simTime0","type":"long","visibility":"private","static":false},{"name":"simDay0","type":"unsigned short","visibility":"private","static":false},{"name":"simMonth0","type":"unsigned short","visibility":"private","static":false},{"name":"simYear0","type":"unsigned short","visibility":"private","static":false},{"name":"eventID","type":"unsigned short","visibility":"private","static":false},{"name":"eventWpnID","type":"unsigned short","visibility":"private","static":false},{"name":"relWpnId","type":"unsigned short","visibility":"private","static":false},{"name":"MAX_TC_THREADS","type":"const unsigned short","visibility":"private","static":true},{"name":"tcThreads","type":"std::array<SimulationTcSyncThread*, MAX_TC_THREADS>","visibility":"private","static":false},{"name":"reqTcThreads","type":"int","visibility":"private","static":false},{"name":"numTcThreads","type":"int","visibility":"private","static":false},{"name":"tcThreadsFailed","type":"bool","visibility":"private","static":false},{"name":"MAX_BG_THREADS","type":"const unsigned short","visibility":"private","static":true},{"name":"bgThreads","type":"std::array<SimulationBgSyncThread*, MAX_BG_THREADS>","visibility":"private","static":false},{"name":"reqBgThreads","type":"int","visibility":"private","static":false},{"name":"numBgThreads","type":"int","visibility":"private","static":false},{"name":"bgThreadsFailed","type":"bool","visibility":"private","static":false}],"components":[{"name":"players","target":"PairStream","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"origPlayers","target":"PairStream","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"newPlayerQueue","target":"Pair","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"station","target":"Station","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Simulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Simulation()"},{"name":"getPlayers","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::PairStream* getPlayers()"},{"name":"getPlayers","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::PairStream* getPlayers() const"},{"name":"cycle","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int cycle() const"},{"name":"frame","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int frame() const"},{"name":"phase","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int phase() const"},{"name":"getExecCounter","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getExecCounter() const"},{"name":"getExecTimeSec","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getExecTimeSec() const"},{"name":"getSysTimeOfDay","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getSysTimeOfDay() const"},{"name":"getSimTimeOfDay","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getSimTimeOfDay() const"},{"name":"getSimTimeValues","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"void getSimTimeValues( unsigned long* const simSec, unsigned long* const simUSec) const"},{"name":"getNewEventID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"unsigned short getNewEventID()"},{"name":"getNewWeaponEventID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"unsigned short getNewWeaponEventID()"},{"name":"getNewReleasedWeaponID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"unsigned short getNewReleasedWeaponID()"},{"name":"getDataRecorder","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractDataRecorder* getDataRecorder()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Station* getStation()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Station* getStation() const"},{"name":"findPlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractPlayer* findPlayer(const short id, const int netID = 0)"},{"name":"findPlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractPlayer* findPlayer(const short id, const int netID = 0) const"},{"name":"findPlayerByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractPlayer* findPlayerByName(const char* const playerName)"},{"name":"findPlayerByName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractPlayer* findPlayerByName(const char* const playerName) const"},{"name":"addNewPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addNewPlayer(const char* const playerName, AbstractPlayer* const player)"},{"name":"addNewPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addNewPlayer(base::Pair* const player)"},{"name":"setInitialSimulationTime","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setInitialSimulationTime(const long time)"},{"name":"updateTC","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateTC(const double dt = 0.0) override"},{"name":"updateData","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateData(const double dt = 0.0) override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"updateTcPlayerList","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void updateTcPlayerList( base::PairStream* const playerList, const double dt, const unsigned int idx, const unsigned int n )"},{"name":"updateBgPlayerList","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void updateBgPlayerList( base::PairStream* const playerList, const double dt, const unsigned int idx, const unsigned int n )"},{"name":"updatePlayerList","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updatePlayerList()"},{"name":"incCycle","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void incCycle()"},{"name":"setCycle","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setCycle(const unsigned int c)"},{"name":"setFrame","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setFrame(const unsigned int f)"},{"name":"setPhase","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setPhase(const unsigned int c)"},{"name":"setEventID","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setEventID(unsigned short id)"},{"name":"setWeaponEventID","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setWeaponEventID(unsigned short id)"},{"name":"printTimingStats","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void printTimingStats() override"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"getStationImp","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Station* getStationImp()"},{"name":"insertPlayerSort","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool insertPlayerSort(base::Pair* const newPlayer, base::PairStream* const newList)"},{"name":"findPlayerPrivate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"AbstractPlayer* findPlayerPrivate(const short id, const int netID) const"},{"name":"findPlayerByNamePrivate","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"AbstractPlayer* findPlayerByNamePrivate(const char* const playerName) const"},{"name":"setSlotPlayers","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotPlayers(base::PairStream* const)"},{"name":"setSlotSimulationTime","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSimulationTime(const base::Time* const)"},{"name":"setSlotDay","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDay(const base::Number* const)"},{"name":"setSlotMonth","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotMonth(const base::Number* const)"},{"name":"setSlotYear","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotYear(const base::Number* const)"},{"name":"setSlotFirstWeaponId","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotFirstWeaponId(const base::Number* const)"},{"name":"setSlotNumTcThreads","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNumTcThreads(const base::Number* const)"},{"name":"setSlotNumBgThreads","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNumBgThreads(const base::Number* const)"}]},"WorldModel":{"file":"contexts/src/mixr/include/mixr/models/WorldModel.hpp","namespace":"mixr::models","base":"Simulation","attributes":[{"name":"refLat","type":"double","visibility":"private","static":false},{"name":"refLon","type":"double","visibility":"private","static":false},{"name":"sinRlat","type":"double","visibility":"private","static":false},{"name":"cosRlat","type":"double","visibility":"private","static":false},{"name":"maxRefRange","type":"double","visibility":"private","static":false},{"name":"gaUseEmFlg","type":"bool","visibility":"private","static":false},{"name":"wm","type":"base::Matrixd","visibility":"private","static":false}],"components":[{"name":"em","target":"EarthModel","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"atmosphere","target":"AbstractAtmosphere","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"terrain","target":"Terrain","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"WorldModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"WorldModel()"},{"name":"getRefLatitude","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getRefLatitude() const"},{"name":"getRefLongitude","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getRefLongitude() const"},{"name":"getSinRefLat","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getSinRefLat() const"},{"name":"getCosRefLat","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getCosRefLat() const"},{"name":"getMaxRefRange","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"double getMaxRefRange() const"},{"name":"getWorldMat","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::Matrixd& getWorldMat() const"},{"name":"getEarthModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const base::EarthModel* getEarthModel() const"},{"name":"isGamingAreaUsingEarthModel","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isGamingAreaUsingEarthModel() const"},{"name":"getTerrain","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const terrain::Terrain* getTerrain() const"},{"name":"getAtmosphere","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractAtmosphere* getAtmosphere()"},{"name":"getAtmosphere","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const AbstractAtmosphere* getAtmosphere() const"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"setEarthModel","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setEarthModel(const base::EarthModel* const msg)"},{"name":"setGamingAreaUseEarthModel","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setGamingAreaUseEarthModel(const bool flg)"},{"name":"setRefLatitude","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setRefLatitude(const double v)"},{"name":"setRefLongitude","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setRefLongitude(const double v)"},{"name":"setMaxRefRange","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxRefRange(const double v)"},{"name":"getTerrain","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"terrain::Terrain* getTerrain()"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"initData","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void initData()"},{"name":"setSlotRefLatitude","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotRefLatitude(const base::LatLon* const)"},{"name":"setSlotRefLatitude","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotRefLatitude(const base::Number* const)"},{"name":"setSlotRefLongitude","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotRefLongitude(const base::LatLon* const)"},{"name":"setSlotRefLongitude","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotRefLongitude(const base::Number* const)"},{"name":"setSlotGamingAreaRange","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotGamingAreaRange(const base::Distance* const)"},{"name":"setSlotEarthModel","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEarthModel(const base::EarthModel* const)"},{"name":"setSlotEarthModel","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEarthModel(const base::String* const)"},{"name":"setSlotGamingAreaEarthModel","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotGamingAreaEarthModel(const base::Number* const)"},{"name":"setSlotTerrain","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTerrain(terrain::Terrain* const)"},{"name":"setSlotAtmosphere","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotAtmosphere(AbstractAtmosphere* const)"}]},"Agent":{"file":"contexts/src/mixr/include/mixr/base/ubf/Agent.hpp","namespace":"mixr::base::ubf","base":"Component","attributes":[],"components":[{"name":"behavior","target":"AbstractBehavior","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"state","target":"AbstractState","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"myActor","target":"Component","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Agent","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Agent()"},{"name":"updateData","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void updateData(const double dt = 0.0) override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"controller","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void controller(const double dt = 0.0)"},{"name":"getBehavior","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"AbstractBehavior* getBehavior() const"},{"name":"setBehavior","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setBehavior(AbstractBehavior* const)"},{"name":"getState","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"AbstractState* getState() const"},{"name":"setState","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setState(AbstractState* const)"},{"name":"initActor","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void initActor()"},{"name":"getActor","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"base::Component* getActor()"},{"name":"setActor","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setActor(base::Component* const myActor)"},{"name":"setSlotBehavior","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotBehavior(AbstractBehavior* const)"},{"name":"setSlotState","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotState(AbstractState* const)"}]},"AbstractBehavior":{"file":"contexts/src/mixr/include/mixr/base/ubf/AbstractBehavior.hpp","namespace":"mixr::base::ubf","base":"Component","attributes":[{"name":"vote","type":"int","visibility":"private","static":false}],"components":[],"methods":[{"name":"AbstractBehavior","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractBehavior()"},{"name":"genAction","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual AbstractAction* genAction(const AbstractState* const state, const double dt) = 0"},{"name":"getVote","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"int getVote() const"},{"name":"setVote","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void setVote(const int x)"},{"name":"setSlotVote","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotVote(const base::Number* const)"}]},"AbstractState":{"file":"contexts/src/mixr/include/mixr/base/ubf/AbstractState.hpp","namespace":"mixr::base::ubf","base":"Component","attributes":[],"components":[],"methods":[{"name":"AbstractState","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractState()"},{"name":"updateGlobalState","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateGlobalState()"},{"name":"updateState","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void updateState(const base::Component* const actor)"},{"name":"getUbfStateByType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const AbstractState* getUbfStateByType(const std::type_info& type) const"}]},"NetIO":{"file":"contexts/src/mixr/include/mixr/interop/common/NetIO.hpp","namespace":"mixr::interop::common","base":"AbstractNetIO","attributes":[{"name":"MAX_NEW_OUTGOING","type":"const unsigned int","visibility":"public","static":true},{"name":"MAX_OBJECTS","type":"const int","visibility":"protected","static":true},{"name":"netID","type":"unsigned short","visibility":"private","static":false},{"name":"federationName","type":"base::safe_ptr<const base::String>","visibility":"private","static":false},{"name":"federateName","type":"base::safe_ptr<const base::String>","visibility":"private","static":false},{"name":"timeline","type":"TSource","visibility":"private","static":false},{"name":"iffEventID","type":"unsigned short","visibility":"private","static":false},{"name":"emEventID","type":"unsigned short","visibility":"private","static":false},{"name":"inputFlg","type":"bool","visibility":"private","static":false},{"name":"outputFlg","type":"bool","visibility":"private","static":false},{"name":"relayFlg","type":"bool","visibility":"private","static":false},{"name":"netInit","type":"bool","visibility":"private","static":false},{"name":"netInitFail","type":"bool","visibility":"private","static":false},{"name":"maxEntityRange","type":"double","visibility":"private","static":false},{"name":"maxEntityRange2","type":"double","visibility":"private","static":false},{"name":"maxTimeDR","type":"double","visibility":"private","static":false},{"name":"maxPositionErr","type":"double","visibility":"private","static":false},{"name":"maxOrientationErr","type":"double","visibility":"private","static":false},{"name":"maxAge","type":"double","visibility":"private","static":false},{"name":"nInNibs","type":"unsigned int","visibility":"private","static":false},{"name":"nOutNibs","type":"unsigned int","visibility":"private","static":false},{"name":"MAX_ENTITY_TYPES","type":"const unsigned int","visibility":"private","static":true},{"name":"nInputEntityTypes","type":"unsigned int","visibility":"private","static":false},{"name":"nOutputEntityTypes","type":"unsigned int","visibility":"private","static":false}],"components":[{"name":"station","target":"Station","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"simulation","target":"Simulation","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"inputList","target":"Nib","targetTier":2,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"outputList","target":"Nib","targetTier":2,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"inputNtmTree","target":"NtmInputNode","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"outputNtmTree","target":"NtmOutputNode","targetTier":2,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"inputEntityTypes","target":"Ntm","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"},{"name":"outputEntityTypes","target":"Ntm","targetTier":1,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"NetIO","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"NetIO()"},{"name":"inputFrame","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void inputFrame(const double dt) override"},{"name":"outputFrame","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void outputFrame(const double dt) override"},{"name":"getNetworkID","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":true,"signature":"unsigned short getNetworkID() const override"},{"name":"getFederateName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::String* getFederateName() const"},{"name":"getFederationName","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const base::String* getFederationName() const"},{"name":"getCurrentTime","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"double getCurrentTime()"},{"name":"getTimeline","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"TSource getTimeline() const"},{"name":"isInputEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isInputEnabled() const"},{"name":"isOutputEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isOutputEnabled() const"},{"name":"isRelayEnabled","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isRelayEnabled() const"},{"name":"getMaxEntityRange","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxEntityRange(const Nib* const nib = nullptr) const"},{"name":"getMaxEntityRangeSquared","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxEntityRangeSquared(const Nib* const nib = nullptr) const"},{"name":"getMaxTimeDR","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxTimeDR(const Nib* const nib = nullptr) const"},{"name":"getMaxPositionErr","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxPositionErr(const Nib* const nib = nullptr) const"},{"name":"getMaxOrientationErr","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxOrientationErr(const Nib* const nib = nullptr) const"},{"name":"getMaxAge","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual double getMaxAge(const Nib* const nib = nullptr) const"},{"name":"isNetworkInitialized","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isNetworkInitialized() const"},{"name":"didInitializationFail","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool didInitializationFail() const"},{"name":"networkInitialization","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool networkInitialization()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"simulation::Station* getStation()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const simulation::Station* getStation() const"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"simulation::Simulation* getSimulation()"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const simulation::Simulation* getSimulation() const"},{"name":"getNewIffEventID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"unsigned short getNewIffEventID()"},{"name":"getNewEmissionEventID","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"unsigned short getNewEmissionEventID()"},{"name":"createIPlayer","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual models::Player* createIPlayer(Nib* const nib)"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"initNetwork","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual bool initNetwork()=0"},{"name":"netInputHander","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual void netInputHander()=0"},{"name":"processInputList","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual void processInputList()=0"},{"name":"processOutputList","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processOutputList()"},{"name":"setNetworkID","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setNetworkID(const unsigned short)"},{"name":"setTimeline","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setTimeline(const TSource)"},{"name":"setMaxTimeDR","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxTimeDR(const double)"},{"name":"setMaxPositionErr","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxPositionErr(const double)"},{"name":"setMaxOrientationErr","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxOrientationErr(const double)"},{"name":"setMaxAge","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxAge(const double)"},{"name":"setMaxEntityRange","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setMaxEntityRange(const double)"},{"name":"setFederateName","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setFederateName(const base::String* const)"},{"name":"setFederationName","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setFederationName(const base::String* const)"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"findNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Nib* findNib(const unsigned short playerID, const base::String* const federateName, const IoType ioType)"},{"name":"findNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Nib* findNib(const models::Player* const player, const IoType ioType)"},{"name":"addNibToList","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addNibToList(Nib* const nib, const IoType ioType)"},{"name":"removeNibFromList","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void removeNibFromList(Nib* const nib, const IoType ioType)"},{"name":"createNewInputNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Nib* createNewInputNib()"},{"name":"createNewOutputNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual Nib* createNewOutputNib(models::Player* const)"},{"name":"destroyInputNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void destroyInputNib(Nib* const)"},{"name":"destroyOutputNib","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void destroyOutputNib(Nib* const)"},{"name":"addNib2InputList","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addNib2InputList(Nib* const)"},{"name":"nibFactory","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual Nib* nibFactory(const NetIO::IoType ioType)=0"},{"name":"insertNewOutputNib","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Nib* insertNewOutputNib(models::Player* const player)"},{"name":"getInputListSize","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getInputListSize() const"},{"name":"getInputNib","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Nib* getInputNib(const unsigned int idx)"},{"name":"getInputNib","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Nib* getInputNib(const unsigned int idx) const"},{"name":"getInputList","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Nib** getInputList()"},{"name":"getOutputListSize","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getOutputListSize() const"},{"name":"getOutputList","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Nib** getOutputList()"},{"name":"getOutputNib","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Nib* getOutputNib(const unsigned int idx)"},{"name":"getOutputNib","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Nib* getOutputNib(const unsigned int idx) const"},{"name":"findNetworkTypeMapper","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Ntm* findNetworkTypeMapper(const Nib* const nib) const"},{"name":"findNetworkTypeMapper","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual const Ntm* findNetworkTypeMapper(const models::Player* const p) const"},{"name":"addOutputEntityType","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addOutputEntityType(Ntm* const item)"},{"name":"addInputEntityType","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool addInputEntityType(Ntm* const item)"},{"name":"clearOutputEntityTypes","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool clearOutputEntityTypes()"},{"name":"clearInputEntityTypes","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool clearInputEntityTypes()"},{"name":"getRootNtmOutputNode","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const NtmOutputNode* getRootNtmOutputNode() const"},{"name":"rootNtmOutputNodeFactory","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":true,"signature":"virtual NtmOutputNode* rootNtmOutputNodeFactory() const"},{"name":"getRootNtmInputNode","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const NtmInputNode* getRootNtmInputNode() const"},{"name":"rootNtmInputNodeFactory","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":true,"signature":"virtual NtmInputNode* rootNtmInputNodeFactory() const =0"},{"name":"getOutputEntityTypes","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Ntm* getOutputEntityTypes(const unsigned int) const"},{"name":"getInputEntityType","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Ntm* getInputEntityType(const unsigned int) const"},{"name":"getNumOutputEntityTypes","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getNumOutputEntityTypes() const"},{"name":"getNumInputEntityTypes","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getNumInputEntityTypes() const"},{"name":"testOutputEntityTypes","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void testOutputEntityTypes(const unsigned int n)"},{"name":"testInputEntityTypes","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void testInputEntityTypes(const unsigned int n)"},{"name":"updateOutputList","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void updateOutputList()"},{"name":"cleanupInputList","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void cleanupInputList()"},{"name":"compareKey2Nib","visibility":"private","static":true,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"static int compareKey2Nib(const void* key, const void* nib)"},{"name":"setSlotFederateName","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotFederateName(const base::String* const)"},{"name":"setSlotFederationName","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotFederationName(const base::String* const)"},{"name":"setSlotMaxTimeDR","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotMaxTimeDR(const base::Time* const)"},{"name":"setSlotMaxPositionErr","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotMaxPositionErr(const base::Distance* const)"},{"name":"setSlotMaxOrientationErr","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotMaxOrientationErr(const base::Angle* const)"},{"name":"setSlotMaxEntityRange","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotMaxEntityRange(const base::Distance* const)"},{"name":"setSlotMaxAge","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotMaxAge(const base::Time* const)"},{"name":"setSlotNetworkID","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotNetworkID(const base::Number* const)"},{"name":"setSlotEnableInput","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableInput(const base::Number* const)"},{"name":"setSlotEnableOutput","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableOutput(const base::Number* const)"},{"name":"setSlotEnableRelay","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEnableRelay(const base::Number* const)"},{"name":"setSlotTimeline","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotTimeline(const base::Identifier* const)"},{"name":"setSlotInputEntityTypes","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotInputEntityTypes(base::PairStream* const)"},{"name":"setSlotOutputEntityTypes","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotOutputEntityTypes(base::PairStream* const)"}]},"Ntm":{"file":"contexts/src/mixr/include/mixr/interop/common/Ntm.hpp","namespace":"mixr::interop::common","base":"Object","attributes":[],"components":[{"name":"tPlayer","target":"Player","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"Ntm","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Ntm()"},{"name":"getTemplatePlayer","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const models::Player* getTemplatePlayer() const"},{"name":"copyEntityType","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":true,"signature":"virtual bool copyEntityType(Nib* const targetNib) const =0"},{"name":"setSlotTemplatePlayer","visibility":"private","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool setSlotTemplatePlayer(const models::Player* const)"}]},"AbstractDataRecorder":{"file":"contexts/src/mixr/include/mixr/simulation/AbstractDataRecorder.hpp","namespace":"mixr::simulation","base":"AbstractRecorderComponent","attributes":[],"components":[{"name":"sta","target":"Station","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"},{"name":"sim","target":"Simulation","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"AbstractDataRecorder","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"AbstractDataRecorder()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Station* getStation()"},{"name":"getStation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Station* getStation() const"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Simulation* getSimulation()"},{"name":"getSimulation","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const Simulation* getSimulation() const"},{"name":"recordData","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool recordData( const unsigned int id, const base::Object* pObjects[4], const double values[4] )"},{"name":"processRecords","visibility":"public","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processRecords()"},{"name":"recordDataImp","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordDataImp( const unsigned int id, const base::Object* pObjects[4], const double values[4] )"},{"name":"processUnhandledId","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":true,"const":false,"signature":"virtual bool processUnhandledId(const unsigned int id) =0"},{"name":"getStationImp","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Station* getStationImp()"},{"name":"getSimulationImp","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"Simulation* getSimulationImp()"}]},"DataRecorder":{"file":"contexts/src/mixr/include/mixr/recorder/DataRecorder.hpp","namespace":"mixr::recorder","base":"AbstractDataRecorder","attributes":[{"name":"firstPass","type":"bool","visibility":"private","static":false},{"name":"eventName","type":"std::string","visibility":"private","static":false},{"name":"application","type":"std::string","visibility":"private","static":false},{"name":"caseNum","type":"unsigned int","visibility":"private","static":false},{"name":"missionNum","type":"unsigned int","visibility":"private","static":false},{"name":"subjectNum","type":"unsigned int","visibility":"private","static":false},{"name":"runNum","type":"unsigned int","visibility":"private","static":false},{"name":"day","type":"unsigned int","visibility":"private","static":false},{"name":"month","type":"unsigned int","visibility":"private","static":false},{"name":"year","type":"unsigned int","visibility":"private","static":false}],"components":[{"name":"outputHandler","target":"OutputHandler","targetTier":1,"multiplicity":"one","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"DataRecorder","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"DataRecorder()"},{"name":"getEventName","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const char* getEventName() const"},{"name":"getApplication","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const char* getApplication() const"},{"name":"getCaseNum","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getCaseNum() const"},{"name":"getMissionNum","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getMissionNum() const"},{"name":"getSubjectNum","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getSubjectNum() const"},{"name":"getRunNum","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getRunNum() const"},{"name":"getDay","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getDay() const"},{"name":"getMonth","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getMonth() const"},{"name":"getYear","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"unsigned int getYear() const"},{"name":"processRecords","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void processRecords() override"},{"name":"reset","visibility":"public","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void reset() override"},{"name":"getOutputHandler","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"OutputHandler* getOutputHandler()"},{"name":"getOutputHandler","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"const OutputHandler* getOutputHandler() const"},{"name":"isFirstPass","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isFirstPass() const"},{"name":"setOutputHandler","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setOutputHandler(OutputHandler* const)"},{"name":"genPlayerId","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void genPlayerId( pb::PlayerId* const id, const models::Player* const player )"},{"name":"genPlayerState","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void genPlayerState( pb::PlayerState* const state, const models::Player* const player )"},{"name":"genTrackData","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void genTrackData( pb::TrackData* const trkMsg, const models::Track* const track )"},{"name":"genEmissionData","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void genEmissionData( pb::EmissionData* const emMsg, const models::Emission* const emData)"},{"name":"sendDataRecord","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void sendDataRecord(pb::DataRecord* const msg)"},{"name":"timeStamp","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void timeStamp(pb::DataRecord* const msg)"},{"name":"genTrackId","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual std::string genTrackId(const models::Track* const track)"},{"name":"setFirstPass","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void setFirstPass(const bool f)"},{"name":"recordMarker","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordMarker(const base::Object* objs[4], const double values[4])"},{"name":"recordAI","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordAI(const base::Object* objs[4], const double values[4])"},{"name":"recordDI","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordDI(const base::Object* objs[4], const double values[4])"},{"name":"recordNewPlayer","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordNewPlayer(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerRemoved","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerRemoved(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerData","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerData(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerDamaged","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerDamaged(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerCollision","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerCollision(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerCrash","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerCrash(const base::Object* objs[4], const double values[4])"},{"name":"recordPlayerKilled","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordPlayerKilled(const base::Object* objs[4], const double values[4])"},{"name":"recordWeaponReleased","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordWeaponReleased(const base::Object* objs[4], const double values[4])"},{"name":"recordWeaponHung","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordWeaponHung(const base::Object* objs[4], const double values[4])"},{"name":"recordWeaponDetonation","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordWeaponDetonation(const base::Object* objs[4], const double values[4])"},{"name":"recordGunFired","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordGunFired(const base::Object* objs[4], const double values[4])"},{"name":"recordNewTrack","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordNewTrack(const base::Object* objs[4], const double values[4])"},{"name":"recordTrackRemoved","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordTrackRemoved(const base::Object* objs[4], const double values[4])"},{"name":"recordTrackData","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual bool recordTrackData(const base::Object* objs[4], const double values[4])"},{"name":"recordDataImp","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool recordDataImp( const unsigned int id, const base::Object* pObjects[4], const double values[4] ) override"},{"name":"processUnhandledId","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool processUnhandledId(const unsigned int id) override"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"},{"name":"initData","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void initData()"},{"name":"setSlotOutputHandler","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotOutputHandler(OutputHandler* const x)"},{"name":"setSlotEventName","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotEventName(base::String* const)"},{"name":"setSlotApplication","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotApplication(base::String* const)"},{"name":"setSlotCaseNum","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotCaseNum(base::Number* const)"},{"name":"setSlotMissionNum","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotMissionNum(base::Number* const)"},{"name":"setSlotSubjectNum","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotSubjectNum(base::Number* const)"},{"name":"setSlotRunNum","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotRunNum(base::Number* const)"},{"name":"setSlotDay","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotDay(base::Number* const)"},{"name":"setSlotMonth","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotMonth(base::Number* const)"},{"name":"setSlotYear","visibility":"private","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"bool setSlotYear(base::Number* const)"}]},"OutputHandler":{"file":"contexts/src/mixr/include/mixr/recorder/OutputHandler.hpp","namespace":"mixr::recorder","base":"AbstractRecorderComponent","attributes":[{"name":"semaphore","type":"long","visibility":"private","static":false}],"components":[{"name":"queue","target":"List","targetTier":2,"multiplicity":"many","resolvedVia":"direct-member","visibility":"private"}],"methods":[{"name":"OutputHandler","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"OutputHandler()"},{"name":"processRecord","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void processRecord(const DataRecordHandle* const)"},{"name":"addToQueue","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void addToQueue(const DataRecordHandle* const)"},{"name":"processQueue","visibility":"public","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":false,"signature":"void processQueue()"},{"name":"processRecordImp","visibility":"protected","static":false,"virtual":true,"override":false,"pureVirtual":false,"const":false,"signature":"virtual void processRecordImp(const DataRecordHandle* const)"},{"name":"isDataTypeEnabled","visibility":"protected","static":false,"virtual":false,"override":false,"pureVirtual":false,"const":true,"signature":"bool isDataTypeEnabled(const DataRecordHandle* const handle) const"},{"name":"processComponents","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"void processComponents( base::PairStream* const list, const std::type_info& filter, base::Pair* const add = nullptr, base::Component* const remove = nullptr ) override"},{"name":"shutdownNotification","visibility":"protected","static":false,"virtual":false,"override":true,"pureVirtual":false,"const":false,"signature":"bool shutdownNotification() override"}]}},"tier2":{"Datalink":{"base":"System","source":"role"},"Pilot":{"base":"System","source":"role"},"RfSignature":{"base":"Component","source":"seed"},"AbstractNib":{"base":"Component","source":"seed"},"IrSignature":{"base":"Component","source":"seed"},"List":{"base":"Object","source":"seed"},"RfSensor":{"base":"RfSystem","source":"role"},"OnboardComputer":{"base":"System","source":"role"},"AbstractAtmosphere":{"base":"Component","source":"seed"},"Statistic":{"base":"Object","source":"seed"},"Radio":{"base":"RfSystem","source":"role"},"Navigation":{"base":"System","source":"role"},"Terrain":{"base":"Component","source":"seed"},"StoresMgr":{"base":"Stores","source":"role"},"DynamicsModel":{"base":"Component","source":"role"},"Gimbal":{"base":"System","source":"role"},"IrSystem":{"base":"System","source":"role"},"SynchronizedState":{"base":"Object","source":"discovered"},"AbstractIoHandler":{"base":"Component","source":"discovered"},"EarthModel":{"base":"Object","source":"discovered"},"Nib":{"base":"AbstractNib","source":"discovered"},"NtmInputNode":{"base":"Object","source":"discovered"},"NtmOutputNode":{"base":"Object","source":"discovered"},"AbstractRecorderComponent":{"base":"Component","source":"discovered"},"Stores":{"base":"ExternalStore","source":"discovered"},"RfSystem":{"base":"System","source":"discovered"},"AbstractNetIO":{"base":"Component","source":"discovered"},"ExternalStore":{"base":"System","source":"discovered"}}};

/* --------- topologia (curada à mão, com o MESMO helper N() de SCENARIO) --
 * Escolhe UMA aresta primária por nó -- o grafo real tem mais de um "pai"
 * possível para várias destas classes (Player é composto por AbstractPlayer
 * via herança E é o alvo de Ntm.tPlayer E é apontado de volta por
 * System.ownship; Station compõe Simulation E Simulation aponta de volta
 * pra Station...). `kind` no FILHO descreve a aresta que liga ele ao PAI:
 * "inherit" (heranca real, sem diamante) ou "compose" (agregação real --
 * um membro de fato existe com esse nome no header, ver `label`/`mult`) --
 * default é "inherit" quando omitido. O cabeçalho de CADA caixa sempre
 * mostra a base REAL (campo `base` do JSON), então uma classe encaixada
 * aqui por composição (ex.: Pair sob PairStream) nunca esconde sua herança
 * verdadeira (Pair extends Object) -- só não repete essa aresta como uma
 * segunda linha no desenho.
 *
 * Três alvos do JSON (Stores/ExternalStore/RfSystem) ficam DE FORA da
 * árvore: só aparecem nos dados como BASE de StoresMgr/RfSensor/Radio
 * (heranca entre dois Tier-2, não composição de nenhum Tier-1 daqui) --
 * incluí-los exigiria um segundo pai pros mesmos três stubs que a árvore já
 * pendura em Player. Ver o aviso na própria aba. */
const STRUCT_TOPOLOGY = N("referenced", "Referenced", { children: [
  N("object", "Object", { kind: "inherit", children: [
    N("component", "Component", { kind: "inherit", children: [
      // Component.components -> PairStream (many, membro direto) -- e a
      // PRÓPRIA PairStream, sem atributo proprio, tem sua composição com
      // Pair inferida das assinaturas dos métodos (findByType/put/get...).
      N("pairStream", "PairStream", { kind: "compose", label: "components", mult: "many", children: [
        N("pair", "Pair", { kind: "compose", label: "(contents)", mult: "many" }),
      ] }),
      N("system", "System", { kind: "inherit" }),
      N("abstractPlayer", "AbstractPlayer", { kind: "inherit", children: [
        // Os 10 "papéis" -- todos base::Pair* genérico no header; o tipo
        // real só aparece dentro do CORPO do setter, via typeid(...). Ver
        // STRUCT_NOTES.Player.
        N("player", "Player", { kind: "inherit", children: [
          N("dynamicsModel", "DynamicsModel", { kind: "compose", label: "dynamicsModel" }),
          N("pilot", "Pilot", { kind: "compose", label: "pilot" }),
          N("navigation", "Navigation", { kind: "compose", label: "navigation" }),
          N("datalink", "Datalink", { kind: "compose", label: "datalink" }),
          N("radio", "Radio", { kind: "compose", label: "radio" }),
          N("gimbal", "Gimbal", { kind: "compose", label: "gimbal" }),
          N("rfSensor", "RfSensor", { kind: "compose", label: "rfSensor" }),
          N("irSystem", "IrSystem", { kind: "compose", label: "irSystem" }),
          N("onboardComputer", "OnboardComputer", { kind: "compose", label: "onboardComputer" }),
          N("storesMgr", "StoresMgr", { kind: "compose", label: "storesMgr" }),
          N("rfSignature", "RfSignature", { kind: "compose", label: "signature" }),
          N("irSignature", "IrSignature", { kind: "compose", label: "irSignature" }),
          N("synchronizedState", "SynchronizedState", { kind: "compose", label: "syncState1/syncState2", mult: "many" }),
        ] }),
        N("abstractNib", "AbstractNib", { kind: "compose", label: "nib/nibList", mult: "many" }),
      ] }),
      N("station", "Station", { kind: "inherit", children: [
        // Station.sim -> Simulation e' membro direto; Simulation.station
        // (o back-pointer) vira STRUCT_BACKREFS -- os dois nao podem ser
        // aresta de arvore ao mesmo tempo sem criar um ciclo de 2 nos.
        N("simulation", "Simulation", { kind: "compose", label: "sim", children: [
          N("worldModel", "WorldModel", { kind: "inherit", children: [
            N("earthModel", "EarthModel", { kind: "compose", label: "em" }),
            N("abstractAtmosphere", "AbstractAtmosphere", { kind: "compose", label: "atmosphere" }),
            N("terrain", "Terrain", { kind: "compose", label: "terrain" }),
          ] }),
        ] }),
        // Station.networks e' generico (PairStream, igual Component.components)
        // -- pendurar NetIO aqui e conceitual ("e' onde um NetIO mora"), no
        // mesmo espirito de Player->papeis (tambem generico no header).
        N("netIO", "NetIO", { kind: "compose", label: "networks", mult: "many", children: [
          N("ntm", "Ntm", { kind: "compose", label: "inputEntityTypes/outputEntityTypes", mult: "many" }),
          N("nib", "Nib", { kind: "compose", label: "inputList/outputList", mult: "many" }),
          N("ntmInputNode", "NtmInputNode", { kind: "compose", label: "inputNtmTree" }),
          N("ntmOutputNode", "NtmOutputNode", { kind: "compose", label: "outputNtmTree" }),
        ] }),
        N("abstractDataRecorder", "AbstractDataRecorder", { kind: "compose", label: "dataRecorder", children: [
          N("dataRecorder", "DataRecorder", { kind: "inherit", children: [
            N("outputHandler", "OutputHandler", { kind: "compose", label: "outputHandler", children: [
              N("list", "List", { kind: "compose", label: "queue", mult: "many" }),
            ] }),
          ] }),
        ] }),
        N("abstractIoHandler", "AbstractIoHandler", { kind: "compose", label: "ioHandler" }),
      ] }),
      N("agent", "Agent", { kind: "inherit", children: [
        N("abstractBehavior", "AbstractBehavior", { kind: "compose", label: "behavior" }),
        N("abstractState", "AbstractState", { kind: "compose", label: "state" }),
      ] }),
      // Bases REAIS de NetIO/AbstractDataRecorder+OutputHandler -- pendu-
      // radas aqui so pra existirem no desenho (documentam a heranca sem
      // reencaixar os filhos ja colocados em Station via composicao).
      N("abstractRecorderComponent", "AbstractRecorderComponent", { kind: "inherit" }),
      N("abstractNetIO", "AbstractNetIO", { kind: "inherit" }),
      N("statistic", "Statistic", { kind: "compose", label: "timingStats" }),
    ] }),
  ] }),
] });

// Arestas que NAO podem ser de arvore -- criariam ciclo com a topologia
// acima (ex.: Player -> WorldModel fecharia Player -> WorldModel -> Simulation
// -> Station -> ... -> Player). Mesmo idioma visual de NAME_LINKS (aba
// Simulação): tracejada, sem marcador de diamante, cor var(--ok).
//
// Nem toda composição real do CLASS_DIAGRAM vira uma dessas arestas --
// deliberado, não esquecido. Um membro cujo alvo é um tipo GENÉRICO/raiz já
// denso na árvore por outro caminho (Object, Component, PairStream, Pair --
// ex.: Pair.obj->Object, Component.selection->Object, Agent.myActor->
// Component, Simulation.players/origPlayers->PairStream, Simulation.
// newPlayerQueue->Pair, Station.igHosts->PairStream) não ganha seta: like
// Player já é uma composição real e nomeada em outro ramo da árvore
// (dezenas de classes "têm um Object" no sentido raso de herdar dele), uma
// seta pra cada uma encheria o diagrama sem acrescentar leitura nova -- o
// dado continua visível, sem seta, dentro do compartimento "componentes" da
// própria caixa dona. Reservado pra back-pointers ESPECÍFICOS entre duas
// entidades já nomeadas no diagrama, o mesmo padrão do trio Player/System/
// Simulation abaixo.
const STRUCT_BACKREFS = [
  { from: "player", to: "worldModel", label: "sim" },
  { from: "system", to: "player", label: "ownship" },
  { from: "simulation", to: "station", label: "station" },
  { from: "netIO", to: "station", label: "station" },
  { from: "netIO", to: "simulation", label: "simulation" },
  // Mesmo padrão do NetIO acima -- AbstractDataRecorder também guarda os
  // dois back-pointers (sta/sim) que STRUCT_NOTES.AbstractDataRecorder já
  // descreve em prosa; sem esta aresta o diagrama tratava dois casos
  // idênticos de forma inconsistente (achado numa revisão adversarial).
  { from: "abstractDataRecorder", to: "station", label: "sta" },
  { from: "abstractDataRecorder", to: "simulation", label: "sim" },
  // Station.ownship: o player "foco" (ex.: o HUD/instrumentos mirando nele)
  // -- referência a um Player especifico dentro da MESMA PairStream que
  // Simulation.players já guarda, não uma segunda posse.
  { from: "station", to: "abstractPlayer", label: "ownship" },
  // Bonus (nao e' cycle -- Player ja mora em outro ramo da arvore, so
  // registra uma referencia cruzada real e pouco obvia): NetIO::createIPlayer()
  // clona ESTE Player-molde quando chega o primeiro PDU de um tipo novo.
  { from: "ntm", to: "player", label: "tPlayer" },
];

// Notas de "filosofia de emprego" -- HAND-CURADAS, uma por classe Tier 1,
// prosa própria a partir do que a composição real (JSON acima) e o resto
// deste arquivo (MODEL/CLAUDE.md) já registram. Mostradas no card de
// detalhe quando a caixa correspondente é selecionada.
const STRUCT_NOTES = {
  Referenced: `Contagem de referências por ref()/unref() -- os únicos dois atributos, além do semáforo de exclusão mútua. Todo Object herda daqui, e é por isso que ~Referenced() é virtual PURA: o destrutor que de fato roda é sempre o da subclasse concreta, disparado quando o último unref() zera o contador.`,
  Object: `RTTI própria do MIXR por cima do que o C++ já dá de graça (isClassType()/isType()) -- o motivo é o dlopen: um plugin carregado em RTLD_LOCAL não compartilha type_info de forma confiável entre bibliotecas diferentes, então MetaObject compara CADEIA DE NOMES, nunca ponteiro de type_info.`,
  Component: `components é uma PairStream de Pair, e é RECURSIVA -- um Component pode conter outros Component, encadeados por Pair (nome + valor). É a MESMA estrutura de dado que representa tanto o cenário EDL em texto quanto a árvore em runtime: um "( Aircraft ) components: { ... }" no arquivo vira exatamente esta composição em memória.`,
  Pair: `Um par nome/valor -- o "name:" (ou a chave de uma lista "{ chave: (Classe) }") é o próprio Pair, e "obj" é o valor. PairStream é uma lista de Pair; é assim que "ache o componente chamado antenna1" vira uma busca por nome dentro de uma PairStream (findByName()).`,
  PairStream: `Não declara nenhum atributo próprio -- herda o armazenamento de List (Tier 2) e só acrescenta os métodos de busca por nome/tipo sobre pares. "De que é a lista" não aparece na declaração da classe, só nas assinaturas dos próprios métodos (Pair* findByType(...), void put(Pair*), Pair* get()...).`,
  System: `Base de todo subsistema que participa do despacho por FASE (dynamics/transmit/receive/process) -- é o que ubf::Agent, Autopilot, RfSensor etc. têm em comum. ownship é o único atributo: todo System sabe a que Player pertence sem precisar subir a árvore de Component inteira a cada consulta.`,
  AbstractPlayer: `Interface pura entre Player (a implementação concreta) e o resto do framework -- nib/nibList são o rastro de rede (DIS/HLA) deste player quando ele existe como entidade remota, não uma segunda cópia do estado físico.`,
  Player: `A classe mais carregada do recorte: 73 atributos próprios (posição/atitude em três referenciais, combustível, dano, assinaturas...) e 314 métodos, a maioria getters/setters gerados por grandeza física. Os DEZ "papéis" (dynamicsModel/pilot/navigation/datalink/radio/gimbal/rfSensor/irSystem/onboardComputer/storesMgr) são TODOS declarados como base::Pair* genérico no header -- o tipo real só aparece dentro do CORPO do setter, via typeid(DynamicsModel) etc. (Player::updateSystemPointers()). É por isso que o nome do slot EDL é cosmético: "dynamicsModel:" poderia se chamar qualquer coisa -- quem resolve o papel é o TIPO do objeto, nunca o nome.`,
  Station: `O executivo do processo: agrega a Simulation (a lista de players) e um DataRecorder (a cadeia de saída -- Tacview, log...), além de zero ou mais NetIO (interoperabilidade DIS/HLA). Um processo normalmente sobe UMA Station só.`,
  Simulation: `A lista de players em si (players/origPlayers, ambas PairStream) mais a fila de entrada de player novo (newPlayerQueue, de Pair) -- o back-pointer pra Station (station) é o que permite a um NetIO::createIPlayer() alcançar tanto a Simulation quanto a Station a partir de um único Player clonado.`,
  WorldModel: `Simulation + o que faz dela um MUNDO físico: modelo de terra (EarthModel), atmosfera e terreno (Terrain -- o mesmo SrtmHgtFile que resolve elevação/AGL, ver a seção "Terreno" deste repositório). É também o único dos 19 cuja base é outro Tier 1 (Simulation), não Component -- updateElevation() e afins só existem por causa dessa herança.`,
  Agent: `O "controller" genérico do ciclo de decisão UBF: um behavior (a política -- Arbiter, BtBehavior, RLBridgeBehavior...) e um state (a leitura do mundo -- o WorldView de flight). myActor é quem está sendo controlado. Nada aqui fala de FASE ou THREAD -- é FlightAgentTC/SimAgent (fora deste recorte) quem decide ONDE controller() é chamado.`,
  AbstractBehavior: `genAction(state, dt) é pura -- cada Behavior concreto decide sozinho como transformar um AbstractState em AbstractAction. Arbitragem entre Behaviors concorrentes (o "vote") vive nas SUBCLASSES, não aqui.`,
  AbstractState: `Interface mínima (4 métodos) -- o suficiente pra um Behavior ler "o mundo" sem saber se está lendo um FlightState/WorldView de verdade ou um FakeDecisionContext de teste.`,
  NetIO: `94 métodos, o maior depois de Player -- e onde moram os DOIS sentidos de DIS/HLA: inputList/outputList (Nib, o registro por entidade) e inputEntityTypes/outputEntityTypes (Ntm, o TEMPLATE que cada tipo de entidade casa). Guarda back-pointers pra Station E Simulation, setados na construção -- por isso as duas aparecem como referência de VOLTA neste diagrama, nunca como filhas.`,
  Ntm: `"Network Translation Module" -- o template que NetIO::createIPlayer() clona quando chega o PRIMEIRO PDU de um tipo de entidade novo (ver a seção do bandit/DIS no CLAUDE.md). tPlayer é literalmente o Player-molde: dynamicsModel/pilot do lado receptor nunca rodam -- só a posição é mantida por dead reckoning.`,
  AbstractDataRecorder: `Interface de gravação -- sta/sim são os back-pointers que todo gravador precisa pra ler o estado a cada evento. processUnhandledId() é pura: um token REID sem handler cai aqui (ver a armadilha do REID_WEAPON_RELEASED registrada na seção xtacview).`,
  DataRecorder: `A implementação de produção -- 55 métodos, a maior parte recordXxx() por tipo de evento (recordNewPlayer, recordPlayerData, recordMarker...). outputHandler é PRA ONDE os registros vão -- TacviewOutput é um OutputHandler encadeado por baixo dele.`,
  OutputHandler: `queue é uma List POR VALOR, não ponteiro -- a fila que DataRecorder empurra registros e um OutputHandler concreto (RecorderOutputHandler, TabPrinter, TacviewOutput...) drena. É a fila sem teto documentada na seção de encerramento do ./app deste repositório.`,
};

const flattenStruct = (n, out = []) => (out.push(n), (n.children || []).forEach((c) => flattenStruct(c, out)), out);
const STRUCT_ALL = flattenStruct(STRUCT_TOPOLOGY);
const STRUCT_BY_ID = Object.fromEntries(STRUCT_ALL.map((n) => [n.id, n]));
const STRUCT_EDGES = [];
(function collectStructEdges(n) {
  (n.children || []).forEach((c) => { STRUCT_EDGES.push([n.id, c.id]); collectStructEdges(c); });
})(STRUCT_TOPOLOGY);

/* --------------------------- layout UML -------------------------------- *
 * Diferente de layout()/flightLayout() (nó de tamanho FIXO): aqui a altura
 * varia MUITO por nó (um stub Tier 2 tem ~1 linha; Player, expandida, tem
 * mais de 400). Largura de caixa fica fixa (UML_BOX_W); só a altura varia
 * -- evita um bin-packer 2D completo, desnecessário pra uma árvore.
 *
 * Duas passadas: pós-ordem MEDINDO a extensão vertical real de cada
 * subárvore (_subH = max(altura própria, soma das alturas dos filhos +
 * espaçamento)); pré-ordem POSICIONANDO (x = profundidade * coluna fixa;
 * y empilha os filhos pela extensão real, centralizados no espaço que a
 * própria subárvore ocupa -- pai centrado entre o PRIMEIRO e o ÚLTIMO
 * filho, mesma regra de layout(), só alimentada com extensões reais em vez
 * de um contador de folha uniforme). */
const UML_BOX_W = 260, UML_COLW = 340, UML_GAP = 20;
const UML_HEAD_BLOCK = 35, UML_HEADER_H = 22, UML_COMPT_PAD = 3, UML_ROW_H = 14, UML_DIVIDER_H = 1;

function umlBoxContentHeight(clsName) {
  const e = CLASS_DIAGRAM.classes[clsName];
  if (!e) return null; // Tier 2: sempre cabeçalho mínimo
  const parts = [e.attributes.length, e.components.length, e.methods.length].filter((n) => n > 0);
  const body = parts.reduce((a, n) => a + UML_COMPT_PAD * 2 + n * UML_ROW_H, 0) + Math.max(0, parts.length - 1) * UML_DIVIDER_H;
  return UML_HEAD_BLOCK + body;
}
function umlNodeHeight(node, collapsedSet) {
  if (collapsedSet.has(node.id)) return UML_HEAD_BLOCK;
  const full = umlBoxContentHeight(node.cls);
  return full == null ? UML_HEAD_BLOCK : full;
}

function umlLayout(root, heightOf) {
  function measure(n) {
    n._h = heightOf(n);
    const kids = n.children || [];
    if (!kids.length) { n._subH = n._h; return n._subH; }
    const kidsH = kids.map(measure);
    const total = kidsH.reduce((a, h) => a + h, 0) + UML_GAP * (kids.length - 1);
    n._subH = Math.max(n._h, total);
    return n._subH;
  }
  measure(root);
  const nodes = [];
  function place(n, depth, top) {
    const kids = n.children || [];
    const x = depth * UML_COLW;
    if (!kids.length) {
      nodes.push({ ...n, x, y: top + n._subH / 2, depth, h: n._h });
      return;
    }
    const kidsTotal = kids.reduce((a, k) => a + k._subH, 0) + UML_GAP * (kids.length - 1);
    let cursor = top + (n._subH - kidsTotal) / 2;
    kids.forEach((k) => { place(k, depth + 1, cursor); cursor += k._subH + UML_GAP; });
    const f = nodes.find((m) => m.id === kids[0].id);
    const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
    nodes.push({ ...n, x, y: (f.y + l.y) / 2, depth, h: n._h });
  }
  place(root, 0, 0);
  return nodes;
}

const VIS_SYM = { public: "+", protected: "#", private: "-" };

/* -------------------------- StructDiagram ------------------------------ */
function StructDiagram({ focus, setFocus, onOpenCatalog }) {
  // Nasce TOTALMENTE recolhido (pedido explícito) -- `collapsed` encolhe
  // por interação do usuário (clique no cabeçalho, ou "expandir tudo").
  const [collapsed, setCollapsed] = useState(() => new Set(STRUCT_ALL.map((n) => n.id)));
  const [pinned, setPinned] = useState(null);
  const { view, setView, svgRef, onDown, onMove, onUp } = usePanZoom();

  // Chegada vinda de outra aba (deep-link futuro) -- mesmo padrão do
  // useEffect de `focus` em Exec/Catalog.
  useEffect(() => {
    if (!focus) return;
    const n = STRUCT_ALL.find((x) => x.cls === focus);
    if (n) setPinned(n.id);
    setFocus(null);
  }, [focus, setFocus]);

  const heightOf = useCallback((n) => umlNodeHeight(n, collapsed), [collapsed]);
  // Colapsar um nó muda seu {h} -- força o recálculo do layout INTEIRO
  // (nunca scroll interno num foreignObject: sem precedente no arquivo, e
  // relayoutar ~44 nós é desprezível -- o Catálogo já refiltra 342 a cada
  // tecla).
  const nodes = useMemo(() => umlLayout(STRUCT_TOPOLOGY, heightOf), [heightOf]);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  const W = Math.max(...nodes.map((n) => n.x)) + UML_BOX_W + 40;
  const H = Math.max(...nodes.map((n) => n.y + n.h / 2)) + 30;
  const topMargin = 20, leftMargin = 16;

  const toggleNode = (id) => setCollapsed((s) => { const n = new Set(s); if (n.has(id)) n.delete(id); else n.add(id); return n; });
  const expandAll = () => setCollapsed(new Set());
  const collapseAll = () => setCollapsed(new Set(STRUCT_ALL.map((n) => n.id)));

  const pn = pinned ? STRUCT_BY_ID[pinned] : null;
  const pnTier1 = pn ? CLASS_DIAGRAM.classes[pn.cls] : null;
  const pnTier2 = pn && !pnTier1 ? CLASS_DIAGRAM.tier2[pn.cls] : null;
  const pnInCatalog = pn ? !!MODEL[pn.cls] : false;

  const tier1Count = Object.keys(CLASS_DIAGRAM.classes).length;
  const tier2Count = Object.keys(CLASS_DIAGRAM.tier2).length;

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <h2 style={{ fontSize: 15, margin: "0 0 6px", fontWeight: 600 }}>Diagrama de Classe Estrutural</h2>
      {/* Aviso obrigatório -- mesmo padrão de outras abas deste arquivo    *
         * (ex.: "não é traçado ao vivo" da aba Simulação/Componentes,      *
         * FLIGHT_SLOT_TYPES na aba Comportamento): dizer explicitamente o  *
         * que é medido/extraído e o que é curado à mão. */}
      {/* maxWidth generoso (nao 980px como outros avisos do arquivo) --
         * de proposito: este texto e' mais longo que os demais, e uma
         * largura estreita forcaria mais linhas de quebra do que a
         * largura real da tela permite, empurrando o grafo abaixo pra
         * fora do viewport e obrigando a JANELA a rolar -- o efeito
         * colateral que esta aba especificamente nao pode ter (o unico
         * efeito do scroll do mouse aqui deve ser zoom na arvore). */}
      <p className="mx-warn" style={{ borderLeftColor: "var(--rule)", color: "var(--muted)", maxWidth: 1400 }}>
        Recorte curado de <b style={{ color: "var(--ink)" }}>{tier1Count}</b> classes fundacionais do MIXR (caixa completa —
        atributos/componentes/métodos, extraídos de verdade do header C++ por <span className="mx-mono">tools/extract_class_diagram.py</span>)
        mais <b style={{ color: "var(--ink)" }}>{tier2Count}</b> alvos de composição (caixa mínima, só nome + base) — não as {STATS.classes} classes do Catálogo.
        A TOPOLOGIA (quem aparece filho de quem) e as notas de "filosofia de emprego" no card de detalhe abaixo são organizadas à mão
        (STRUCT_TOPOLOGY/STRUCT_NOTES em doc.jsx) — uma classe MIXR raramente tem um único "pai", então a árvore escolhe UMA aresta
        primária por nó pra caber num desenho legível. Referências que fechariam ciclo nessa árvore (ex.: Player aponta pra WorldModel,
        que é filho de Simulation, que é filho de Station, que é filho de Player via System.ownship) aparecem tracejadas, sem diamante —
        mesma convenção das setas "por nome" da aba Simulação. Três classes (Stores/ExternalStore/RfSystem) existem só nos dados brutos,
        como base de StoresMgr/RfSensor/Radio, sem caixa própria neste desenho.
      </p>

      <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginBottom: 8 }}>
        <button className="mx-btn" onClick={expandAll}>expandir tudo</button>
        <button className="mx-btn" onClick={collapseAll}>recolher tudo</button>
        <span style={{ fontSize: 11.5, color: "var(--muted)", alignSelf: "center" }}>clique no cabeçalho de uma caixa Tier 1 pra recolher/expandir só ela · clique em qualquer lugar da caixa pra ver detalhe</span>
      </div>

      <div className="mx-graph">
        <div className="mx-zoom">
          <div className="mx-zoomslider" title="Zoom -- também funciona com a roda do mouse">
            <input type="range" min={ZOOM_MIN} max={ZOOM_MAX} step={0.01} value={view.k} aria-label="Zoom"
              onChange={(e) => setView((v) => ({ ...v, k: Number(e.target.value) }))} />
            <span className="mx-mono">{view.k.toFixed(2)}×</span>
          </div>
          <button className="mx-zbtn" data-w="1" onClick={() => setView({ k: 1, x: 0, y: 0 })}>ajustar</button>
        </div>
        {/* Altura um pouco mais conservadora que o clamp(500px,82vh,1500px)
           * padrao de .mx-svgwrap[data-expanded="1"] (Simulação/Comportamento)
           * -- esta aba tem mais texto de aviso ACIMA do grafo; reservar 82vh
           * pra ele nesta aba especificamente empurraria a página pra além
           * do viewport na maioria das janelas, obrigando rolagem. */}
        <div className="mx-svgwrap" data-expanded="1" style={{ height: "clamp(420px, 62vh, 1300px)" }}>
          <svg ref={svgRef} viewBox={`${-leftMargin} ${-topMargin} ${W + leftMargin} ${H + topMargin}`} preserveAspectRatio="xMidYMid meet"
               onPointerDown={onDown} onPointerMove={onMove} onPointerUp={onUp} onPointerLeave={onUp}>
            <defs>
              {/* Único <marker> do arquivo -- diamante preenchido de       *
                 * composição UML. Mais simples e correto que desenhar o    *
                 * losango à mão em ~40 arestas. orient="auto" alinha com a *
                 * direção do trecho INICIAL do path (o lado do "dono"). */}
              <marker id="uml-diamond" viewBox="0 0 16 10" refX="0.5" refY="5" markerWidth="13" markerHeight="8" orient="auto">
                <path d="M0.5,5 L8,0.5 L15.5,5 L8,9.5 Z" fill="var(--ink)" />
              </marker>
            </defs>
            <g transform={`translate(${view.x},${view.y}) scale(${view.k})`} style={{ transformOrigin: "center" }}>
              {STRUCT_EDGES.map(([a, b]) => {
                const p = pos[a], q = pos[b];
                if (!p || !q) return null;
                const child = STRUCT_BY_ID[b];
                const compose = child.kind === "compose";
                const mid = p.x + UML_BOX_W + (UML_COLW - UML_BOX_W) / 2;
                const d = `M ${p.x + UML_BOX_W} ${p.y} H ${mid} V ${q.y} H ${q.x}`;
                return (
                  <g key={a + ">" + b}>
                    <path d={d} fill="none" stroke={compose ? "var(--ink)" : "var(--rule)"} strokeWidth={compose ? 1.5 : 1.2}
                      markerStart={compose ? "url(#uml-diamond)" : undefined} />
                    {compose && child.label && (
                      <foreignObject x={mid - 90} y={q.y - 15} width="180" height="12" style={{ pointerEvents: "none" }}>
                        <div className="mx-mono" title={child.label}
                             style={{ textAlign: "center", fontSize: 8.5, color: "var(--sub-muted)", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                          {child.label}{child.mult === "many" ? " [*]" : ""}
                        </div>
                      </foreignObject>
                    )}
                  </g>
                );
              })}
              {STRUCT_BACKREFS.map((bref) => {
                const p = pos[bref.from], q = pos[bref.to];
                if (!p || !q) return null;
                const pRight = p.x + UML_BOX_W, qRight = q.x + UML_BOX_W;
                const d = p.x <= q.x
                  ? `M ${pRight} ${p.y} C ${pRight + 70} ${p.y}, ${q.x - 70} ${q.y}, ${q.x} ${q.y}`
                  : `M ${p.x} ${p.y} C ${p.x - 70} ${p.y}, ${qRight + 70} ${q.y}, ${qRight} ${q.y}`;
                const lx = (p.x + q.x) / 2 + UML_BOX_W / 2, ly = (p.y + q.y) / 2;
                return (
                  <g key={bref.from + "~" + bref.to} opacity="0.8">
                    <path d={d} fill="none" stroke="var(--ok)" strokeWidth="1.3" strokeDasharray="2 3" />
                    <foreignObject x={lx - 90} y={ly - 6} width="180" height="12" style={{ pointerEvents: "none" }}>
                      <div className="mx-mono" title={bref.label}
                           style={{ textAlign: "center", fontSize: 8.5, color: "var(--ok)", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                        {bref.label}
                      </div>
                    </foreignObject>
                  </g>
                );
              })}
              {nodes.map((n) => {
                const tier1 = CLASS_DIAGRAM.classes[n.cls];
                const tier2 = !tier1 ? CLASS_DIAGRAM.tier2[n.cls] : null;
                const isCollapsed = collapsed.has(n.id);
                const hasCompartments = !!tier1;
                const realBase = tier1 ? tier1.base : (tier2 ? tier2.base : null);
                const compts = tier1 && !isCollapsed ? [
                  ["attributes", tier1.attributes, "a"],
                  ["components", tier1.components, "c"],
                  ["methods", tier1.methods, "m"],
                ].filter(([, arr]) => arr.length) : [];
                return (
                  <g key={n.id} className="mx-node" transform={`translate(${n.x},${n.y - n.h / 2})`}
                     onClick={() => setPinned(n.id)}>
                    <rect x="0" y="0" width={UML_BOX_W} height={n.h} rx="2"
                      fill={tier2 ? "var(--panel)" : (pinned === n.id ? "var(--active-bg)" : "var(--paper)")}
                      stroke={pinned === n.id ? "var(--ink)" : "var(--rule)"}
                      strokeWidth={pinned === n.id ? 1.6 : 1}
                      strokeDasharray={tier2 ? "3 2" : "0"} />
                    <foreignObject x="0" y="0" width={UML_BOX_W} height={n.h}>
                      <div style={{ width: UML_BOX_W, height: n.h, overflow: "hidden" }}>
                        <div onClick={(e) => { e.stopPropagation(); if (hasCompartments) toggleNode(n.id); setPinned(n.id); }}
                             style={{ display: "flex", alignItems: "center", gap: 4, height: UML_HEADER_H, padding: "0 8px", cursor: hasCompartments ? "pointer" : "default" }}>
                          {hasCompartments && <span style={{ color: "var(--muted)", fontSize: 10, width: 9, flexShrink: 0 }}>{isCollapsed ? "▸" : "▾"}</span>}
                          <span className="mx-mono" style={{ fontWeight: 600, fontSize: 11, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }} title={n.cls}>{n.cls}</span>
                        </div>
                        <div className="mx-mono" style={{ fontSize: 9, color: "var(--muted)", fontStyle: "italic", padding: "0 8px 3px", whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                          {tier2 ? "«tier 2» " : ""}{realBase ? `: ${realBase}` : ""}
                        </div>
                        {compts.map(([kind, arr, pfx]) => (
                          <div key={kind} style={{ borderTop: "1px solid var(--rule)", padding: "2px 8px" }}>
                            {arr.map((it, i) => {
                              if (kind === "methods") {
                                let sig = it.signature;
                                if (it.pureVirtual) sig = sig.replace(/\s*=\s*0\s*$/, "") + " {abstract}";
                                return (
                                  <div key={pfx + i} className="mx-mono" title={sig}
                                       style={{ fontSize: 9.5, lineHeight: "14px", height: 14, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis", fontStyle: it.virtual ? "italic" : "normal" }}>
                                    <span style={{ color: "var(--muted)", display: "inline-block", width: 10 }}>{VIS_SYM[it.visibility] || "~"}</span>{sig}
                                  </div>
                                );
                              }
                              const target = kind === "attributes" ? it.type : `${it.target}${it.multiplicity === "many" ? "[*]" : ""}`;
                              return (
                                <div key={pfx + i} className="mx-mono" title={`${it.name} : ${target}`}
                                     style={{ fontSize: 9.5, lineHeight: "14px", height: 14, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
                                  <span style={{ color: "var(--muted)", display: "inline-block", width: 10 }}>{VIS_SYM[it.visibility] || "~"}</span>
                                  {it.name}<span style={{ color: kind === "components" ? "var(--rf)" : "var(--sub-muted)" }}> : {target}</span>
                                </div>
                              );
                            })}
                          </div>
                        ))}
                      </div>
                    </foreignObject>
                  </g>
                );
              })}
            </g>
          </svg>
        </div>
      </div>

      {pn && (
        <div className="mx-card" style={{ margin: "10px 0" }}>
          <div style={{ display: "flex", justifyContent: "space-between", gap: 12, flexWrap: "wrap", alignItems: "baseline" }}>
            <div>
              <span className="mx-mono" style={{ fontWeight: 600, fontSize: 13.5 }}>{pn.cls}</span>
              <span className="mx-mono" style={{ color: "var(--muted)", fontSize: 11.5 }}>
                {" "}· {pnTier1 ? `Tier 1 (caixa completa) · extends ${pnTier1.base || "—"}` : `Tier 2 (caixa mínima) · extends ${(pnTier2 || {}).base || "—"}`}
              </span>
            </div>
            <div style={{ display: "flex", gap: 6 }}>
              {pnInCatalog && <button className="mx-btn" onClick={() => onOpenCatalog(pn.cls)}>ver no Catálogo →</button>}
              <button className="mx-btn" onClick={() => setPinned(null)}>fechar</button>
            </div>
          </div>
          {STRUCT_NOTES[pn.cls] ? (
            <p style={{ fontSize: 12, lineHeight: 1.55, marginTop: 8, maxWidth: 900 }}>{STRUCT_NOTES[pn.cls]}</p>
          ) : (
            <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 8 }}>
              {pnTier1 ? "Sem nota de filosofia de emprego curada pra esta classe." : "Caixa mínima (Tier 2) -- alvo de composição de algum Tier 1, sem extração de corpo (atributos/métodos) nesta aba."}
            </p>
          )}
        </div>
      )}
    </div>
  );
}
