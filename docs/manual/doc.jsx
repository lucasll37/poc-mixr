import React, { useState, useEffect, useLayoutEffect, useMemo, useRef, useCallback } from "react";

/* ==================================================================== *
 * MIXR — Explorador de execução, EDL e classes built-in   (v6)
 *
 * MODEL, FACTORIES, SNIPPETS e STATS NAO sao declarados neste arquivo --
 * sao injetados em tempo de build por docs/manual/compile.js, concatenando
 * o texto de docs/manual/catalog.generated.js (escrito por
 * tools/generate_manual_catalog.py; 'make open-docs' roda o gerador ANTES do
 * compile.js, e so entao abre a pagina) como um <script> proprio, antes do
 * app transpilado. Universo:
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
// É o mesmo desenho de tests/fixtures/built-in_mixr_1/configs/scenario_max_player.edl.in
// (ex-poc, removida de src/poc/ -- ver CLAUDE.md) -- "qual o player mais
// elaborado que dá para montar só com componentes NATIVOS
// do mixr::models?", com uma única diferença deliberada: ali o Datalink é
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
          note: "53 das 96 classes que mixr::models::factory publica, num Aircraft só — ver a seção 'built-in_mixr_1/full-systems-nav — removidas como pocs' no CLAUDE.md.",
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
// tests/fixtures/built-in_mixr_1/configs/scenario_max_player.edl.in (ex-poc,
// removida de src/poc/ -- ver CLAUDE.md; "qual o player
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
                  rootDir: "./dist/share/mixr-plugins/A-4/jsbsim/"
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

/* ---------------------------------------------------------------------------
 * cppTokenizeLines(lines) -- highlight de sintaxe C++ LEVE, por heurística
 * (não é um lexer C++ de verdade -- não precisa: o objetivo é dar pista
 * visual num trecho de código já correto, não validar sintaxe). Roda sobre
 * TODAS as linhas do snippet de uma vez (não sobre a janela recortada por
 * windowLines()) porque um comentário de bloco C (o de barra-asterisco, não
 * o de barra-barra) pode abrir numa linha ACIMA da janela visível --
 * tokenizar só o recorte perderia esse estado e coloriria código como
 * comentário por engano.
 *
 * Sete categorias, coloridas por --cpp-* (CSS, fixas nos dois temas -- ver
 * o comentário ao lado da declaração): kw (palavra reservada), str
 * (string/char literal), num (número), com (comentário), macro
 * (identificador TODO-MAIUSCULO -- cobre macro de verdade, ex.
 * BEGIN_RECORD_DATA_SAMPLE, E constante de enum, ex. KILL_EVENT/PRE_RELEASE/
 * DETONATED -- a mesma classe visual serve às duas porque o que importa
 * aqui é "isto é uma CONSTANTE nomeada do framework", não a distinção
 * lexical entre as duas), fn (identificador seguido de "(", exceto palavra
 * reservada) e type (identificador seguido de "::"). Tudo o mais fica sem
 * cor -- pontuação, operadores, identificador comum.
 * ------------------------------------------------------------------------ */
const CPP_KEYWORDS = new Set([
  "if", "else", "for", "while", "do", "return", "switch", "case", "default", "break", "continue",
  "class", "struct", "namespace", "public", "private", "protected", "virtual", "override", "final",
  "static", "const", "constexpr", "void", "bool", "double", "float", "int", "unsigned", "char",
  "long", "short", "auto", "new", "delete", "nullptr", "true", "false", "typename", "template",
  "using", "typedef", "enum", "this", "sizeof", "explicit", "friend", "inline", "mutable",
  "operator", "throw", "try", "catch", "noexcept", "volatile", "goto", "union", "signed",
]);

const CPP_TOKEN_RE = /\/\/.*$|"(?:[^"\\]|\\.)*"|'(?:[^'\\]|\\.)*'|\b0[xX][0-9a-fA-F]+\b|\b\d+\.?\d*[fFuUlL]*\b|[A-Za-z_]\w*|\/\*|\*\//g;

function cppTokenizeLines(lines) {
  let inBlock = false;
  return lines.map((line) => {
    const out = [];
    let last = 0;
    if (inBlock) {
      const endIdx = line.indexOf("*/");
      if (endIdx === -1) { out.push({ t: line, c: "com" }); return out; }
      out.push({ t: line.slice(0, endIdx + 2), c: "com" });
      last = endIdx + 2;
      inBlock = false;
    }
    CPP_TOKEN_RE.lastIndex = last;
    let m;
    while ((m = CPP_TOKEN_RE.exec(line))) {
      if (m.index > last) out.push({ t: line.slice(last, m.index), c: null });
      const tok = m[0];
      if (tok === "/*") {
        const endIdx = line.indexOf("*/", m.index + 2);
        if (endIdx === -1) { out.push({ t: line.slice(m.index), c: "com" }); inBlock = true; last = line.length; break; }
        out.push({ t: line.slice(m.index, endIdx + 2), c: "com" });
        last = endIdx + 2;
        CPP_TOKEN_RE.lastIndex = last;
        continue;
      }
      if (tok === "*/") { out.push({ t: tok, c: null }); last = m.index + tok.length; continue; }
      if (tok.startsWith("//")) { out.push({ t: tok, c: "com" }); last = line.length; break; }
      if (tok[0] === "\"" || tok[0] === "'") { out.push({ t: tok, c: "str" }); last = m.index + tok.length; continue; }
      if (/^[0-9]/.test(tok)) { out.push({ t: tok, c: "num" }); last = m.index + tok.length; continue; }
      if (CPP_KEYWORDS.has(tok)) { out.push({ t: tok, c: "kw" }); last = m.index + tok.length; continue; }
      if (tok.length > 2 && /^[A-Z][A-Z0-9_]*$/.test(tok)) { out.push({ t: tok, c: "macro" }); last = m.index + tok.length; continue; }
      const after = line.slice(m.index + tok.length);
      if (/^\s*\(/.test(after)) out.push({ t: tok, c: "fn" });
      else if (/^\s*::/.test(after)) out.push({ t: tok, c: "type" });
      else out.push({ t: tok, c: null });
      last = m.index + tok.length;
    }
    if (last < line.length) out.push({ t: line.slice(last), c: null });
    return out;
  });
}

/* Renderiza uma linha já tokenizada dentro de .mx-src -- usado pelas quatro
 * telas que mostram código C++ (Execução, Comportamento, step-by-step,
 * Catálogo), então uma mudança na paleta/heurística vale pras quatro de
 * uma vez. 'raw' é o texto puro da linha, usado só se não houver token
 * nenhum (linha vazia) -- mesma razão do "{ln || ' '}" que existia antes:
 * uma <span> sem filho nenhum não ocupa altura de linha em todo navegador. */
function renderCppSrc(tokens, raw) {
  if (!tokens || !tokens.length) return raw || " ";
  return tokens.map((tk, i) => (tk.c ? <span key={i} className={`mx-cpp-${tk.c}`}>{tk.t}</span> : <React.Fragment key={i}>{tk.t}</React.Fragment>));
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
 * consumidor: StructDiagram (aba Diagrama de Classes). Devolve exatamente os       *
 * mesmos nomes que os três call-sites já usam como variáveis locais.    */
function usePanZoom(initial = { k: 1, x: 0, y: 0 }, maxZoom = ZOOM_MAX) {
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
      setView((v) => ({ ...v, k: Math.max(ZOOM_MIN, Math.min(maxZoom, v.k * f)) }));
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
  return { view, setView, svgRef, onDown, onMove, onUp, drag, maxZoom };
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
  --card-shadow: 0 1px 2px rgba(22,35,46,0.05), 0 4px 14px rgba(22,35,46,0.045);
  --warn-bg: rgba(140,47,61,0.07);
  /* Cores de sintaxe C++ (mx-cpp-*) -- FIXAS, não redefinidas no tema escuro:
   * o fundo do bloco de código (--code) já é escuro nos DOIS temas (só muda
   * de tom, #1B2730 claro / #12171B escuro), então uma paleta calibrada pra
   * fundo escuro serve nos dois sem duplicar sete variáveis a mais no bloco
   * [data-theme="dark"]. */
  --cpp-kw:#E3A15A; --cpp-str:#9BC97C; --cpp-num:#7FB8D9; --cpp-com:#6E828C;
  --cpp-macro:#D98CDB; --cpp-fn:#D9C77D; --cpp-type:#6FD1C5;
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
  --card-shadow: 0 1px 2px rgba(0,0,0,0.28), 0 4px 14px rgba(0,0,0,0.22);
  --warn-bg: rgba(224,128,143,0.10);
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
/* Arrastar pra pan (onPointerDown/Move) não impede a seleção nativa de   *
 * texto do navegador -- sem isto, arrastar dentro de QUALQUER caixa/nó    *
 * (Simulação, Comportamento, Diagrama de Classes) seleciona o texto dos  *
 * <foreignObject> embaixo do cursor, junto com o pan. user-select:none   *
 * aqui (herdado por tudo dentro do <svg>, inclusive o HTML dos           *
 * foreignObject) tira a seleção sem precisar de e.preventDefault() no    *
 * pointerdown -- que quebraria o clique normal em botão/nó (mesma        *
 * armadilha já documentada em usePanZoom() sobre capturar o pointer cedo *
 * demais). */
.mx-svgwrap svg { width:100%; height:100%; display:block; touch-action:none; cursor:grab;
  user-select:none; -webkit-user-select:none; -moz-user-select:none; -ms-user-select:none; }
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
.mx-card { background:var(--panel); padding:14px 17px; border-radius:6px;
  border:1px solid var(--rule); box-shadow:var(--card-shadow); }
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
.mx-cpp-kw { color:var(--cpp-kw); }
.mx-cpp-str { color:var(--cpp-str); }
.mx-cpp-num { color:var(--cpp-num); }
.mx-cpp-com { color:var(--cpp-com); font-style:italic; }
.mx-cpp-macro { color:var(--cpp-macro); font-weight:600; }
.mx-cpp-fn { color:var(--cpp-fn); }
.mx-cpp-type { color:var(--cpp-type); }

/* --- aba Referência: tile de leitura (HUD) e pilula de status -- reusa as       *
 * mesmas variaveis de tema de sempre, nenhuma cor nova. --- */
.mx-stat { background:var(--panel); border-radius:3px; padding:6px 12px; min-width:92px; }
.mx-stat-label { font-size:9px; color:var(--muted); text-transform:uppercase; letter-spacing:0.05em; margin-bottom:2px; }
.mx-stat-value { font-family:var(--mono); font-size:13.5px; font-weight:700; color:var(--ink); white-space:nowrap; }
.mx-pill { display:inline-flex; align-items:center; gap:5px; padding:3px 11px; border-radius:11px;
  font-size:11px; font-weight:600; font-family:var(--mono); }
.mx-refhero { border:1px solid var(--rule); border-radius:7px; background:var(--panel); padding:16px 19px;
  box-shadow:var(--card-shadow); }
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
.mx-warn { margin:8px 0 0; padding:9px 12px; border-left:3px solid var(--rf);
  border-radius:0 4px 4px 0; background:var(--warn-bg);
  font-size:12px; line-height:1.5; color:var(--ink); }
/* O rotulo inicial ("Achado:", "Bug confirmado, nao redescobrir:") vem como
 * <b> logo no comeco do paragrafo em toda ocorrencia -- vira uma pilula
 * visual so com CSS, sem tocar as ~20 chamadas que ja escrevem esse padrao. */
.mx-warn > b:first-child { display:inline-block; color:var(--rf); font-size:10.5px;
  font-weight:700; text-transform:uppercase; letter-spacing:0.03em; margin-right:2px; }
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
.mx-slot { display:flex; gap:9px; font-family:var(--mono); font-size:11px; padding:3px 4px;
  border-radius:3px; }
.mx-slot span:first-child { min-width:112px; flex:0 0 auto; color:var(--ink); font-weight:600;
  overflow-wrap:anywhere; }
/* min-width:0 é a correção do bug: um item flex, por padrão, recusa encolher
 * abaixo do seu min-content -- e o min-content de um identificador SEM espaço
 * (um valor de enum, um tipo "PairStream<Vec2d>") é a palavra inteira. Sem
 * isto, .mx-slot exigia mais largura que a coluna de columns:Npx tem, e como
 * layout multi-coluna não recorta overflow horizontal, o texto vazava por
 * cima da coluna vizinha -- o bug de sobreposição confirmado rodando (Missile
 * e Gimbal, achado com screenshot real). overflow-wrap:anywhere garante que
 * ATE um token sem espaço nenhum quebre, em vez de forçar a largura mínima. */
.mx-slot span:last-child { color:var(--muted); flex:1 1 auto; min-width:0;
  overflow-wrap:anywhere; }
.mx-slot:hover { background:var(--band-bg); }
/* Grade de colunas em vez de lista alta com scroll: uma cadeia com 40+ slots
 * ainda cabe sem caixa de rolagem, so ficando mais larga que alta. */
.mx-slotgrid { columns:260px; column-gap:22px; column-rule:1px solid var(--rule); }
.mx-slotgrid .mx-slot { break-inside:avoid; }
/* Linha de enum: NOME+valor costuma ser mais comprido que qualquer nome de
 * slot ("DETONATE_GROUND_PROXIMATE_DETONATION =4") -- lado a lado com a
 * descricao na mesma linha do .mx-slot comum, o par nunca cabe na coluna e
 * os dois espremem para uma fatia minuscula (o MESMO bug do overlap, so que
 * virando quebra caractere-a-caractere em vez de vazamento). Empilhado (nome
 * em cima, descricao embaixo, cada um usando a largura INTEIRA da coluna)
 * nao tem essa disputa -- e le melhor como enum de qualquer forma. */
.mx-enumrow { font-family:var(--mono); font-size:11px; padding:3px 4px; border-radius:3px; }
.mx-enumrow:hover { background:var(--band-bg); }
.mx-enumrow-name { color:var(--ink); font-weight:600; overflow-wrap:anywhere; }
.mx-enumrow-desc { color:var(--muted); margin-top:2px; overflow-wrap:anywhere; }
.mx-slotgrid .mx-enumrow { break-inside:avoid; }

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
  // outra aba para a aba Diagrama de Classes (nenhum ponto do app ainda dispara
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
            <button className="mx-tab" data-on={mode === "steps" ? 1 : 0} onClick={() => setMode("steps")}>step-by-step</button>
            <button className="mx-tab" data-on={mode === "struct" ? 1 : 0} onClick={() => setMode("struct")}>Diagrama de Classes</button>
            <button className="mx-tab" data-on={mode === "ref" ? 1 : 0} onClick={() => setMode("ref")}>Referência</button>
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
      {mode === "steps" && (
        <MissileTrace onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }} />
      )}
      {mode === "struct" && (
        <StructDiagram onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }}
                       focus={structFocus} setFocus={setStructFocus} />
      )}
      {mode === "ref" && (
        <Reference onOpenCatalog={(c) => { setCatalogFocus(c); setMode("cat"); }} />
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
  const cppTokens = useMemo(() => (snip ? cppTokenizeLines(snip.lines) : null), [snip]);
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
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{renderCppSrc(cppTokens && cppTokens[abs], ln)}</span></div>;
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
            treeFile: "./dist/share/mixr-plugins/A-4/flight_tree.xml"
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
  const cppTokens = useMemo(() => (snip ? cppTokenizeLines(snip.lines) : null), [snip]);
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
        return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{renderCppSrc(cppTokens && cppTokens[abs], ln)}</span></div>;
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

/* ====================== rastreio: disparo de missil, passo a passo (aba "step-by-step") ==========
 * Curadoria manual sobre dado real -- MESMA pratica ja registrada acima para FLIGHT_SNIPPETS:
 * cada trecho abaixo foi conferido direto no fonte, arquivo e linha reais, nao gerado pelo
 * extrator automatico. Aqui por um motivo a mais que o de FLIGHT_SNIPPETS: esta trilha atravessa
 * DOIS plugins (models/players/A-4 e models/players/missile) e duas classes ABSTRATAS do MIXR
 * (AbstractWeapon, StoresMgr) que ficam de fora do Catalogo (ele so' cobre classe CONCRETA,
 * IMPLEMENT_SUBCLASS -- ver MOD_ORDER/Catalog abaixo) -- tools/generate_manual_catalog.py nao
 * tem como alcancar nada disso automaticamente.
 *
 * O cenario de referencia e' sandbox/A4-6DOF-MISSILE (ver README la): a4_shooter detecta
 * a4_target pelo radar e dispara um ( GuidedMissile ) contra ele. Os numeros medidos citados nas
 * notas (t=67.1s disparo, ~35m aproximacao, t=93.9s remocao) sao os do README daquele cenario,
 * nao inventados aqui.
 *
 * O ponto do exercicio inteiro: MOSTRAR onde o MIXR emite (ou deixa de emitir) evento de verdade
 * dentro do proprio disparo -- nao so' "o que a classe faz", mas o INSTANTE em que ela
 * efetivamente chama event()/BEGIN_RECORD_DATA_SAMPLE, e o que fica faltando quando ninguem do
 * lado do cenario liga o flag certo (killRemoval) ou inclui o token certo no enabledList.
 * ==================================================================================== */

const MISSILE_STAGES = [
  { n: 0, label: "contexto" },
  { n: 1, label: "decisão" },
  { n: 2, label: "liberação" },
  { n: 3, label: "transição" },
  { n: 4, label: "guiagem" },
  { n: 5, label: "detonação" },
  { n: 6, label: "epílogo" },
];

const MISSILE_SNIPPETS = {
  "LaunchEnvelopeCondition::tick": {
    file: "models/players/A-4/src/bt/nodes/LaunchEnvelopeCondition.cpp",
    line: 15,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Deliberadamente CONTATO DE VERDADE (snapshot().hasContact), nao",
      "// threatPolicy().engaged() -- ao contrario de ContactDetectedCondition, que",
      "// consulta a histerese justamente para NAO oscilar entre evadir e apoiar.",
      "// Disparar contra uma pista que ja sumiu (so' sobrevivendo no arrasto da",
      "// histerese) mandaria o missil atras de uma posicao velha.",
      "//------------------------------------------------------------------------------",
      "BT::NodeStatus LaunchEnvelopeCondition::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& snap = context_.behavior->snapshot();",
      "   if (!snap.weaponReady || !snap.hasContact) return BT::NodeStatus::FAILURE;",
      "",
      "   return domain::inLaunchEnvelope(context_.behavior->launchEnvelope(),",
      "                                   snap.contactRangeM, snap.contactRelBearingDeg)",
      "      ? BT::NodeStatus::SUCCESS",
      "      : BT::NodeStatus::FAILURE;",
      "}",
    ],
    trunc: false,
  },
  "LaunchMissileAction::tick": {
    file: "models/players/A-4/src/bt/nodes/LaunchMissileAction.cpp",
    line: 14,
    lines: [
      "//------------------------------------------------------------------------------",
      "// So marca o PEDIDO -- o comando de voo mantem rumo/altitude/velocidade",
      "// atuais (disparar nao e motivo pra mudar de trajetoria; o disparo em si",
      "// acontece na atuacao, ver ubf/FlightAction.cpp). Mesmo piso anti-CFIT que",
      "// RTB/SUPPORT/PATROL ja respeitam fora do ramo de evasao (ver",
      "// bt/DecisionContext.hpp::clampAltitudeToTerrain()).",
      "//",
      "// So' dispara UMA vez por engajamento sem precisar de estado proprio aqui:",
      "// depois do primeiro disparo, StoresMgr::available() cai (um cenario com",
      "// um so' missil no cabide vai a zero), snapshot().weaponReady vira false no",
      "// PROXIMO frame, e LaunchEnvelopeCondition passa a falhar sozinho -- a",
      "// mesma latencia de um frame entre decisao e percepcao que o resto deste",
      "// modelo ja tem (ver o comentario de domain::ThreatPolicy sobre",
      "// contactLive()/engaged()).",
      "//------------------------------------------------------------------------------",
      "BT::NodeStatus LaunchMissileAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& snap = context_.behavior->snapshot();",
      "   FlightDecision& decision{context_.behavior->decision()};",
      "",
      "   domain::FlightCommand cmd;",
      "   cmd.headingDeg = snap.headingDeg;",
      "   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(snap.altitudeM);",
      "   cmd.speedKts = snap.speedKts;",
      "   decision.take(cmd, \"LAUNCH\");",
      "",
      "   decision.launchRequested = true;",
      "   decision.launchTargetName = snap.contactName;",
      "",
      "   return BT::NodeStatus::SUCCESS;",
      "}",
    ],
    trunc: false,
  },
  "FlightAction::execute (lancamento)": {
    file: "models/players/A-4/src/ubf/FlightAction.cpp",
    line: 291,
    lines: [
      "   // Lancamento de missil -- o UNICO ponto deste modelo que toca um objeto",
      "   // MIXR de arma. Padrao idiomatico do proprio framework, nao invencao",
      "   // deste modelo: Player::getStoresManagement() -> StoresMgr::",
      "   // releaseOneMissile() (publico, PRE-REF'D, dynamic_cast<Missile*> por",
      "   // baixo -- casa QUALQUER subclasse de Missile, nativa ou de terceiro) ->",
      "   // AbstractWeapon::setTargetPlayer(alvo, /*posTrkEnb=*/true) -> unref().",
      "   // 'posTrkEnb=true' e' o que liga isGuidanceEnabled() do lado do missil",
      "   // (alem do proprio tof>=tsg) -- ver models/players/missile/docs/ARCHITECTURE.md.",
      "   if (launchRequested) {",
      "      launchRequested = false;   // um pedido so' vale para UM frame",
      "",
      "      auto* const world = player->getWorldModel();",
      "      const auto target = (world != nullptr)",
      "         ? dynamic_cast<models::Player*>(world->findPlayerByName(launchTargetName.c_str()))",
      "         : nullptr;",
      "",
      "      auto* const storesMgr = player->getStoresManagement();",
      "",
      "      if (target == nullptr) {",
      "         LOG(WARNING) << \"[FlightAction] \" << playerName",
      "                      << \": lancamento abortado -- alvo '\" << launchTargetName",
      "                      << \"' nao encontrado\";",
      "      } else if (storesMgr == nullptr || storesMgr->available() == 0) {",
      "         LOG(WARNING) << \"[FlightAction] \" << playerName",
      "                      << \": lancamento abortado -- cabide vazio\";",
      "      } else {",
      "         auto* const flyout = storesMgr->releaseOneMissile();",
      "         if (flyout != nullptr) {",
      "            flyout->setTargetPlayer(target, /*posTrkEnb=*/true);",
      "            LOG(INFO) << \"[FlightAction] \" << playerName",
      "                      << \": missil lancado contra \" << launchTargetName;",
      "            flyout->unref();   // releaseOneMissile() devolve pre-ref'd",
      "         } else {",
      "            LOG(WARNING) << \"[FlightAction] \" << playerName",
      "                         << \": lancamento abortado -- releaseOneMissile() devolveu nulo\";",
      "         }",
      "      }",
      "   }",
    ],
    trunc: true,
  },
  "SimpleStoresMgr::getNextMissileImp": {
    file: "contexts/src/mixr/src/models/system/SimpleStoresMgr.cpp",
    line: 168,
    lines: [
      "Missile* SimpleStoresMgr::getNextMissileImp()",
      "{",
      "   Missile* msl{};",
      "",
      "   base::PairStream* list{getWeapons()};",
      "   if (list != nullptr) {",
      "",
      "      // find the first free (inactive) missile",
      "      base::List::Item* item{list->getFirstItem()};",
      "      while (item != nullptr && msl == nullptr) {",
      "         const auto pair = static_cast<base::Pair*>(item->getValue());",
      "         const auto p = dynamic_cast<Missile*>(pair->object());",
      "         if (p != nullptr) {",
      "            if (p->isInactive() || p->isReleaseHold()) {",
      "               msl = static_cast<Missile*>(p->getPointer());",
      "            }",
      "         }",
      "         item = item->getNext();",
      "      }",
      "      list->unref();",
      "   }",
      "",
      "   return msl;",
      "}",
    ],
    trunc: false,
  },
  "Stores::releaseWeapon": {
    file: "contexts/src/mixr/src/models/system/Stores.cpp",
    line: 312,
    lines: [
      "// By weapon",
      "AbstractWeapon* Stores::releaseWeapon(AbstractWeapon* const wpn)",
      "{",
      "   AbstractWeapon* flyout{};",
      "",
      "   Player* own{getOwnship()};",
      "   if (wpn != nullptr && own != nullptr) {",
      "",
      "      // Release the weapon",
      "      wpn->setLaunchVehicle(own);",
      "      flyout = wpn->release();",
      "",
      "   }",
      "",
      "   return flyout;",
      "}",
    ],
    trunc: false,
  },
  "AbstractWeapon::release": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 560,
    lines: [
      "AbstractWeapon* AbstractWeapon::release()",
      "{",
      "   AbstractWeapon* flyout{};",
      "",
      "   // When this weapon isn't already released, blocked or jettisoned.",
      "   if ( !isReleased() && !isBlocked() && !isJettisoned() ) {",
      "",
      "      // and isn't flagged to be a hung store (i.e., failure mode),",
      "      if (!getWillHang()) {",
      "",
      "         // and we have a launching player and a simulation ...",
      "         Player* lplayer{getLaunchVehicle()};",
      "         const auto sim = static_cast<WorldModel*>( findContainerByType(typeid(WorldModel)) );",
      "         if ( lplayer != nullptr && sim != nullptr) {",
      "",
      "            // then release the weapon!",
      "",
      "            flyout = getFlyoutWeapon();",
      "            if (flyout != nullptr) {",
      "               // When we've already created a flyout weapon, which is on the",
      "               // player list in 'release hold' ...",
      "",
      "               // we'll just need to clear the \"release",
      "               // hold\" flag, which will let the flyout weapon go ACTIVE.",
      "               flyout->setReleased(true);",
      "               flyout->setReleaseHold(false);",
      "",
      "               // Set the initial weapon's mode flags to fully released.",
      "               AbstractWeapon* initWpn{getInitialWeapon()};",
      "               initWpn->setMode(Player::LAUNCHED);",
      "               initWpn->setReleased(true);",
      "               initWpn->setReleaseHold(false);",
      "               initWpn->unref();",
      "            } else {",
      "               // When we haven't already created a flyout then this is",
      "               // a direct release ...",
      "",
      "               // Get a release event",
      "               eventID = sim->getNewWeaponEventID();",
      "",
      "               // Next we'll clone ourself --",
      "               //  -- this will be the actual weapon player what will do the fly-out.",
      "               flyout = this->clone();",
      "",
      "               flyout->container( sim );",
      "               flyout->reset();",
      "",
      "               flyout->setFlyoutWeapon(flyout);",
      "               flyout->setInitialWeapon(this);",
      "               flyout->setID( sim->getNewReleasedWeaponID() );",
      "",
      "               flyout->setLaunchVehicle( lplayer );",
      "               flyout->setSide( lplayer->getSide() );",
      "",
      "               // and set the weapon prerelease",
      "               flyout->setMode(PRE_RELEASE);",
      "               flyout->setReleased(true);",
      "               flyout->setReleaseHold(false);",
      "",
      "               // Set our mode flags to fully released.",
      "               setFlyoutWeapon(flyout);",
      "               setInitialWeapon(this);",
      "               setMode(Player::LAUNCHED);",
      "               setReleased(true);",
      "               setReleaseHold(false);",
      "",
      "               // add it to the flyout weapon player list",
      "               char pname[32];",
      "               std::sprintf(pname,\"W%05d\", flyout->getID());",
      "               sim->addNewPlayer(pname,flyout);",
      "            }",
      "",
      "            BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_WEAPON_RELEASED )",
      "               SAMPLE_3_OBJECTS( flyout, getLaunchVehicle(), nullptr )  // weapon, shooter, target",
      "               SAMPLE_2_VALUES( 0, 0.0 )",
      "            END_RECORD_DATA_SAMPLE()",
      "",
      "         }",
      "",
      "      } else {",
      "         // We have a hung store",
      "         setHung(true);",
      "",
      "         BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_WEAPON_HUNG )",
      "            SAMPLE_3_OBJECTS( this, getLaunchVehicle(), nullptr )",
      "         END_RECORD_DATA_SAMPLE()",
      "",
      "      }",
      "   }",
      "",
      "   return flyout;",
      "}",
    ],
    trunc: false,
  },
  "Missile::setTargetPlayer": {
    file: "contexts/src/mixr/src/models/player/weapon/Missile.cpp",
    line: 250,
    lines: [
      "// setTargetPlayer() -- sets a pointer to the target player",
      "bool Missile::setTargetPlayer(Player* const tgt, const bool pt)",
      "{",
      "   // if our tgt has changed, reset ground truth vals for weaponGuidance's fuzing logic",
      "   if (tgt != nullptr && tgt != getTargetPlayer()) {",
      "      trngT = (tgt->getPosition()-getPosition()).length();",
      "      trdotT=0.0;",
      "   }",
      "   return BaseClass::setTargetPlayer(tgt, pt);",
      "}",
    ],
    trunc: false,
  },
  "AbstractWeapon::setTargetPlayer": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 1063,
    lines: [
      "// setTargetPlayer() -- sets a pointer to the target player",
      "bool AbstractWeapon::setTargetPlayer(Player* const tgt, const bool pt)",
      "{",
      "    tgtPlayer = tgt;",
      "    tgtTrack = nullptr;",
      "",
      "    // Track position?",
      "    posTrkEnb = (pt && tgt != nullptr);",
      "    positionTracking();",
      "    return true;",
      "}",
    ],
    trunc: false,
  },
  "AbstractWeapon::updateTC": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 222,
    lines: [
      "//------------------------------------------------------------------------------",
      "// updateTC() -- update time critical stuff here",
      "//------------------------------------------------------------------------------",
      "void AbstractWeapon::updateTC(const double dt)",
      "{",
      "   BaseClass::updateTC(dt);",
      "",
      "   unsigned int ph{getWorldModel()->phase()};",
      "",
      "   // Phase #0 -- Transition from pre-release to active at the end of dynamics",
      "   // phase (after the call to BaseClass), so that our position, which was",
      "   // relative to our launch vehicle, has been computed.",
      "   if (ph == 0 && isMode(PRE_RELEASE) && !isReleaseHold() ) {",
      "      atReleaseInit();",
      "      setMode(ACTIVE);",
      "   }",
      "",
      "   // Phase #3",
      "   if (ph == 3 && isActive() && isLocalPlayer() && !isJettisoned() && !isDummy()) {",
      "",
      "      // Simple function to get target coordinates",
      "      if (posTrkEnb) positionTracking();",
      "",
      "      // Update our Time-Of-Flight (TOF)",
      "      if (isMode(ACTIVE)) updateTOF(dt * 4.0);",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "Missile::atReleaseInit": {
    file: "contexts/src/mixr/src/models/player/weapon/Missile.cpp",
    line: 102,
    lines: [
      "//------------------------------------------------------------------------------",
      "// atReleaseInit() -- Init weapon data at release",
      "//------------------------------------------------------------------------------",
      "void Missile::atReleaseInit()",
      "{",
      "   // First the base class will setup the initial conditions",
      "   BaseClass::atReleaseInit();",
      "",
      "   if (getDynamicsModel() == nullptr) {",
      "      // set initial commands",
      "      cmdPitch = static_cast<double>(getPitch());",
      "      cmdHeading = static_cast<double>(getHeading());",
      "      cmdVelocity = vpMax;",
      "",
      "      if (getTargetTrack() != nullptr) {",
      "         // Set initial range and range dot",
      "         base::Vec3d los = getTargetTrack()->getPosition();",
      "         trng = los.length();",
      "         trngT = trng;",
      "      }",
      "      else if (getTargetPlayer() != nullptr) {",
      "         // Set initial range and range dot",
      "         base::Vec3d los = getTargetPosition();",
      "         trng = los.length();",
      "         trngT = trng;",
      "      }",
      "      else {",
      "         trng = 0.0;",
      "      }",
      "",
      "      // Range dot",
      "      trdot = 0.0;",
      "      trdotT = 0.0;",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "GuidedMissile::atReleaseInit": {
    file: "models/players/missile/src/xnative/GuidedMissile.cpp",
    line: 88,
    lines: [
      "//------------------------------------------------------------------------------",
      "// atReleaseInit() -- semeia cmdHeadingRad_/cmdPitchRad_/cmdSpeedMps_ com a",
      "// atitude/velocidade de LANCAMENTO (mesmo padrao de Missile::atReleaseInit()",
      "// nativo, que semeia cmdPitch/cmdHeading/cmdVelocity -- so' que aqueles sao",
      "// campos PROPRIOS de Missile, nunca lidos por GuidedMissile::weaponDynamics(),",
      "// que consome os campos abaixo).",
      "//",
      "// Sem este metodo (achado rodando, nao suposto -- ver o comentario de",
      "// weaponGuidance() sobre isGuidanceEnabled()): os tres campos ficam no",
      "// inicializador de classe (0.0) ate tof>=tsg, e weaponDynamics() ja roda",
      "// TODO frame independente do TSG -- o missil guina ativamente para",
      "// rumo/pitch GEOGRAFICO ZERO (Norte, nivelado) e desacelera em direcao a",
      "// ZERO m/s durante toda a janela do TSG (aqui, 1.0 s), a taxa/aceleracao",
      "// maxima (maxG/maxAccel). Medido no cenario sandbox/A4-6DOF-MISSILE: o",
      "// missil abre mao de ate ~66 graus de rumo e perde velocidade real antes de",
      "// a navegacao proporcional assumir -- o suficiente, em geometrias menos",
      "// favoraveis que a testada, para nao convergir dentro de maxBurstRng e",
      "// \"passar do lado\" do alvo sem detonar.",
      "//------------------------------------------------------------------------------",
      "void GuidedMissile::atReleaseInit()",
      "{",
      "   BaseClass::atReleaseInit();",
      "",
      "   cmdHeadingRad_ = getHeadingR();",
      "   cmdPitchRad_ = getPitchR();",
      "   cmdSpeedMps_ = getVpMax();",
      "}",
    ],
    trunc: false,
  },
  "AbstractWeapon::dynamics": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 250,
    lines: [
      "//------------------------------------------------------------------------------",
      "// dynamics() -- update vehicle dynamics",
      "//------------------------------------------------------------------------------",
      "void AbstractWeapon::dynamics(const double dt)",
      "{",
      "   if (isMode(PRE_RELEASE)) {",
      "      // Weapon is on the same side as the launcher",
      "      setSide( getLaunchVehicle()->getSide() );",
      "",
      "      // Launch vehicles rotational matrix",
      "      base::Matrixd lvM{getLaunchVehicle()->getRotMat()};",
      "",
      "      // Set weapon's position at launch",
      "      // 1) Weapon's position is its position relative to the launcher (launcher's body coordinates)",
      "      // 2) Rotate to earth coordinates",
      "      // 3) Add the launcher's position",
      "      const base::Vec2d ip{getInitPosition()};",
      "      const base::Vec3d pos0b(ip.x(), ip.y(), -getInitAltitude());",
      "      const base::Vec3d pos0e{pos0b * lvM}; // body to earth",
      "      const base::Vec3d lpos{getLaunchVehicle()->getPosition()};",
      "      const base::Vec3d pos1{lpos + pos0e};",
      "      setPosition( pos1 );",
      "",
      "      // Weapon's orientation at launch",
      "      const base::Vec3d ia{getInitAngles()};",
      "      base::Matrixd rr;",
      "      base::nav::computeRotationalMatrix( ia[0], ia[1], ia[2], &rr);",
      "      rr *= lvM;",
      "",
      "      setRotMat(rr);",
      "",
      "      // Set velocities are the same as the launcher",
      "      setVelocity( getLaunchVehicle()->getVelocity() );",
      "",
      "      // Not accelerations or angular velocities",
      "      setAcceleration( 0, 0, 0 );",
      "      setAngularVelocities( 0, 0, 0 );",
      "   } else if (!isJettisoned()) {",
      "",
      "      if (isLocalPlayer() && !isDummy() && getDynamicsModel() == nullptr) {",
      "         // Use our default (simple) weapon model",
      "         weaponGuidance(dt);",
      "         weaponDynamics(dt);",
      "      }",
      "      BaseClass::dynamics(dt);",
      "",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "GuidedMissile::weaponGuidance": {
    file: "models/players/missile/src/xnative/GuidedMissile.cpp",
    line: 116,
    lines: [
      "//------------------------------------------------------------------------------",
      "// weaponGuidance() -- le a posicao/velocidade do alvo DIRETO do Player*",
      "// (mesmo padrao de Missile::calculateVectors() nativo -- nao usa o cache",
      "// tgtPos/tgtVel de AbstractWeapon, que mistura convencao absoluta com",
      "// relativa entre os dois campos). Delega a lei de guiagem inteira para",
      "// domain::proportionalNavigation() (sem MIXR, testada isolada em",
      "// tests/domain/test_Guidance.cpp) e guarda o comando para",
      "// weaponDynamics() consumir no MESMO frame.",
      "//------------------------------------------------------------------------------",
      "void GuidedMissile::weaponGuidance(const double dt)",
      "{",
      "   const Player* const tgt{getTargetPlayer()};",
      "   if (tgt == nullptr || !tgt->isActive()) return;",
      "",
      "   const base::Vec3d& tgtPos{tgt->getPosition()};",
      "   const base::Vec3d& tgtVel{tgt->getVelocity()};",
      "   const base::Vec3d& ownPos{getPosition()};",
      "   const base::Vec3d& ownVel{getVelocity()};",
      "",
      "   const domain::Vec3 relPos{tgtPos.x() - ownPos.x(), tgtPos.y() - ownPos.y(), tgtPos.z() - ownPos.z()};",
      "   const domain::Vec3 relVel{tgtVel.x() - ownVel.x(), tgtVel.y() - ownVel.y(), tgtVel.z() - ownVel.z()};",
      "",
      "   if (isGuidanceEnabled()) {",
      "      const domain::GuidanceGains gains{kNavRatio, /*cruiseSpeedMps=*/getVpMax()};",
      "      const auto cmd{domain::proportionalNavigation(relPos, relVel, gains)};",
      "      cmdHeadingRad_ = cmd.cmdHeadingRad;",
      "      cmdPitchRad_ = cmd.cmdPitchRad;",
      "      cmdSpeedMps_ = cmd.cmdSpeedMps;",
      "   }",
      "",
      "   // Espoleta de proximidade -- roda independente de isGuidanceEnabled()",
      "   // (mesmo padrao do Missile nativo): usa o alcance/velocidade relativa de",
      "   // VERDADE, nao o comando -- um missil ainda sem guiagem ligada (tof <",
      "   // tsg) pode passar perto o bastante do alvo por trajetoria balistica.",
      "   if (!isDummy() && getTOF() > 2.0) {",
      "      const auto outcome{domain::proximityFuze(relPos, relVel, getMaxBurstRng(), fuzeState_)};",
      "      fuzeState_ = outcome.nextState;",
      "",
      "      if (outcome.closestApproachReached) {",
      "         setMode(DETONATED);",
      "",
      "         // Unico ponto de observabilidade do desfecho -- nem o Tacview (o",
      "         // token REID de detonacao fica de fora do enabledList, mesma",
      "         // armadilha ja documentada pro REID_WEAPON_RELEASED) nem o alvo",
      "         // (nenhum dano visivel: a4_target/GuidedMissile nao ligam",
      "         // checkDetonationEffect() a reacao nenhuma da aeronave) mostram",
      "         // isto sozinhos -- sem esta linha, um acerto e um erro parecem",
      "         // IDENTICOS na tela: o missil so' desaparece silenciosamente",
      "         // ~2s depois (kLingerSec).",
      "         const char* const tgtName = (tgt->getName() != nullptr) ? tgt->getName()->getString() : \"?\";",
      "         mixr::xlog::Stream(outcome.hit ? mixr::xlog::Level::INFO : mixr::xlog::Level::WARNING)",
      "            << \"[GuidedMissile] \" << (getName() != nullptr ? getName()->getString() : \"?\")",
      "            << \": \" << (outcome.hit ? \"ACERTO\" : \"FALHA\")",
      "            << \" contra \" << tgtName",
      "            << \" -- alcance de menor aproximacao \" << outcome.rangeAtEventM << \" m\"",
      "            << \" (burst \" << getMaxBurstRng() << \" m)\";",
      "",
      "         if (outcome.hit) {",
      "            setDetonationResults(DETONATE_ENTITY_IMPACT);",
      "            checkDetonationEffect();",
      "         } else {",
      "            // passou do ponto de menor aproximacao sem acertar --",
      "            // autodestruicao, mesmo comportamento do Missile nativo.",
      "            setDetonationResults(DETONATE_DETONATION);",
      "            setTargetPlayer(nullptr, false);",
      "            setTargetTrack(nullptr, false);",
      "         }",
      "      }",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "GuidedMissile::weaponDynamics": {
    file: "models/players/missile/src/xnative/GuidedMissile.cpp",
    line: 187,
    lines: [
      "//------------------------------------------------------------------------------",
      "// weaponDynamics() -- integra heading/pitch/velocidade em direcao ao",
      "// comando guardado por weaponGuidance(), limitado por taxa de giro (de",
      "// maxG, herdado de Missile) e por aceleracao (maxAccel, idem). NAO integra",
      "// posicao: confirmado lendo AbstractWeapon::dynamics() do fork vendorizado",
      "// -- ele chama weaponGuidance()+weaponDynamics() e, logo em seguida,",
      "// BaseClass::dynamics(dt) (Player::dynamics()), que SEMPRE chama",
      "// positionUpdate(dt), integrando a posicao a partir da velocidade que",
      "// acabamos de escrever aqui. Escrever a posicao nos dois lugares",
      "// duplicaria a integracao.",
      "//------------------------------------------------------------------------------",
      "void GuidedMissile::weaponDynamics(const double dt)",
      "{",
      "   const double speed{std::max(getTotalVelocity(), 1.0)};",
      "",
      "   // g em METROS/s^2 (base::ETHGM), nao base::ETHG (pes/s^2) -- este missil",
      "   // trabalha em m/s do inicio ao fim; a constante em pes daria uma taxa de",
      "   // giro maxima ~3,28x errada.",
      "   const double maxTurnRateRadPerS{(getMaxG() * base::ETHGM) / speed};",
      "",
      "   double dPitch{base::angle::aepcdRad(cmdPitchRad_ - getPitchR())};",
      "   dPitch = std::clamp(dPitch, -maxTurnRateRadPerS * dt, maxTurnRateRadPerS * dt);",
      "   const double newPitch{getPitchR() + dPitch};",
      "",
      "   double dHeading{base::angle::aepcdRad(cmdHeadingRad_ - getHeadingR())};",
      "   dHeading = std::clamp(dHeading, -maxTurnRateRadPerS * dt, maxTurnRateRadPerS * dt);",
      "   double newHeading{getHeadingR() + dHeading};",
      "   // (...) fecha o angulo em [0, 2*PI) e calcula o angulo de banco cosmetico",
      "",
      "   setEulerAngles(bankRad, newPitch, newHeading);",
      "",
      "   double dSpeed{cmdSpeedMps_ - getTotalVelocity()};",
      "   dSpeed = std::clamp(dSpeed, -getMaxAccel() * dt, getMaxAccel() * dt);",
      "   const double newSpeed{getTotalVelocity() + dSpeed};",
      "",
      "   const double cosPitch{std::cos(newPitch)};",
      "   const double vN{newSpeed * cosPitch * std::cos(newHeading)};",
      "   const double vE{newSpeed * cosPitch * std::sin(newHeading)};",
      "   const double vD{-newSpeed * std::sin(newPitch)};",
      "   setVelocity(vN, vE, vD);",
      "}",
    ],
    trunc: true,
  },
  "AbstractWeapon::checkDetonationEffect": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 380,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Check local players for the effects of the detonation -- did we hit anyone?",
      "//------------------------------------------------------------------------------",
      "void AbstractWeapon::checkDetonationEffect()",
      "{",
      "   WorldModel* s{getWorldModel()};",
      "   if (s != nullptr) {",
      "      // Only local players within 10X max burst range",
      "      double maxRng{10.0 * getMaxBurstRng()};",
      "",
      "      // Find our target (if any)",
      "      const Player* tgt{getTargetPlayer()};",
      "      if (tgt == nullptr) {",
      "         const Track* trk{getTargetTrack()};",
      "         if (trk != nullptr) tgt = trk->getTarget();",
      "      }",
      "",
      "      base::PairStream* plist{s->getPlayers()};",
      "      if (plist != nullptr) {",
      "         base::List::Item* item{plist->getFirstItem()};",
      "",
      "         // Process the detonation for all local, in-range players",
      "         bool finished{};",
      "         while (item != nullptr && !finished) {",
      "            base::Pair* pair{static_cast<base::Pair*>(item->getValue())};",
      "            Player* p{static_cast<Player*>(pair->object())};",
      "            finished = p->isNetworkedPlayer();  // local only",
      "            if (!finished && (p != this) ) {",
      "               base::Vec3d dpos{p->getPosition() - getPosition()};",
      "               const double rng{dpos.length()};",
      "               if ( (rng <= maxRng) || (p == tgt) ) p->processDetonation(rng, this);",
      "            }",
      "            item = item->getNext();",
      "         }",
      "",
      "         // cleanup",
      "         plist->unref();",
      "         plist = nullptr;",
      "      }",
      "",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "Player::processDetonation": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 2312,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Process weapon detonation",
      "//------------------------------------------------------------------------------",
      "void Player::processDetonation(const double detRange, AbstractWeapon* const wpn)",
      "{",
      "   if (!isKillOverride()) {",
      "",
      "      // Weapon, launcher & range info",
      "      Player* launcher{};",
      "      double rng{detRange};",
      "      double blastRange{500.0};    // burst range (meters)",
      "      double lethalRange{50.0};    // lethal range  (meters)",
      "      if (wpn != nullptr) {",
      "         launcher = wpn->getLaunchVehicle();",
      "         blastRange = wpn->getMaxBurstRng();",
      "         lethalRange  = wpn->getLethalRange();",
      "         if (this == wpn->getTargetPlayer()) {",
      "            // If we're the target -- use the weapon's detonation range",
      "            rng = wpn->getDetonationRange();",
      "         }",
      "      }",
      "",
      "      // Very close?",
      "      if (rng < lethalRange) {",
      "         // and like horseshoes -- being close does matter",
      "         event(KILL_EVENT, launcher);",
      "      }",
      "",
      "      // Near by?",
      "      else if (rng <= blastRange) {",
      "         // use distance to compute amount of damage",
      "         double damageRng{blastRange - lethalRange};",
      "         if (damageRng <= 1.0) damageRng = 1.0;",
      "         double newDamage{1.0 - ( (rng - lethalRange) / damageRng )};",
      "         setDamage(newDamage + getDamage());",
      "         setFlames( getDamage() - 0.25 );",
      "         setSmoke( getDamage() + 0.25 );",
      "         if ( isDestroyed() ) {",
      "            event(KILL_EVENT, launcher);",
      "         }",
      "      }",
      "",
      "   }",
      "",
      "   // record EVERYTHING that had the potential to cause damage, even if killOverride",
      "",
      "   BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_DAMAGED )",
      "      SAMPLE_2_OBJECTS( this, wpn )",
      "   END_RECORD_DATA_SAMPLE()",
      "",
      "}",
    ],
    trunc: false,
  },
  "Player::event (KILL_EVENT)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 175,
    lines: [
      "BEGIN_EVENT_HANDLER(Player)",
      "",
      "   // We're just killed by 'Player'",
      "   ON_EVENT_OBJ(KILL_EVENT, killedNotification, Player)",
      "",
      "   // We're just killed by unknown player",
      "   ON_EVENT(KILL_EVENT, killedNotification)",
      "",
      "   // We just collided 'Player'",
      "   ON_EVENT_OBJ(CRASH_EVENT, collisionNotification, Player)",
      "",
      "   // We just crashed",
      "   ON_EVENT(CRASH_EVENT,crashNotification)",
      "",
      "   // (... mais 15 tokens no mesmo despacho: RF_EMISSION, DATALINK_MESSAGE, ...)",
    ],
    trunc: true,
  },
  "Player::killedNotification": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 2364,
    lines: [
      "//------------------------------------------------------------------------------",
      "// killedNotification() -- We were just killed by a weapon from player 'p'",
      "//------------------------------------------------------------------------------",
      "bool Player::killedNotification(Player* const p)",
      "{",
      "   if (!isKillOverride()) {",
      "      // When not in 'kill override' mode ...",
      "",
      "      // Let all of our subcomponents know that we were just killed",
      "      {",
      "         base::PairStream* subcomponents{getComponents()};",
      "         if (subcomponents != nullptr) {",
      "            for (base::List::Item* item = subcomponents->getFirstItem(); item != nullptr; item = item->getNext()) {",
      "               base::Pair* pair{static_cast<base::Pair*>(item->getValue())};",
      "               base::Component* sc{static_cast<base::Component*>(pair->object())};",
      "               sc->event(KILL_EVENT, p);",
      "            }",
      "            subcomponents->unref();",
      "            subcomponents = nullptr;",
      "         }",
      "      }",
      "",
      "      setDamage(1.0);",
      "      setSmoke(1.0);",
      "      setFlames(1.0);",
      "",
      "      // Set our status",
      "      if (killRemoval && isLocalPlayer()) {",
      "",
      "         justKilled = true;",
      "         setMode(KILLED);",
      "",
      "         if (p != nullptr) killedBy = p->getID();",
      "         else killedBy = 0;",
      "      }",
      "",
      "   }",
      "",
      "   // record kill, even if killOverride",
      "   BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_KILLED )",
      "      SAMPLE_2_OBJECTS( this, p )",
      "   END_RECORD_DATA_SAMPLE()",
      "",
      "   return true;",
      "}",
    ],
    trunc: false,
  },
  "Player::updateTC (AGL<0)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 2808,
    lines: [
      "      // ---",
      "      // Check for ground collisions",
      "      // ---",
      "      if (getAltitudeAgl() < 0.0 && isLocalPlayer() && isMajorType(AIR_VEHICLE | WEAPON | SPACE_VEHICLE)) {",
      "         // We're below the ground!",
      "         this->event(CRASH_EVENT,nullptr);",
      "      }",
    ],
    trunc: true,
  },
  "GuidedMissile::updateTC": {
    file: "models/players/missile/src/xnative/GuidedMissile.cpp",
    line: 65,
    lines: [
      "//------------------------------------------------------------------------------",
      "// updateTC() -- so' acrescenta o timer de remocao pos-detonacao; o resto",
      "// (TOF, positionTracking(), a transicao PRE_RELEASE->ACTIVE) continua",
      "// inteiramente a cargo de AbstractWeapon::updateTC(), via BaseClass.",
      "//------------------------------------------------------------------------------",
      "void GuidedMissile::updateTC(const double dt)",
      "{",
      "   BaseClass::updateTC(dt);",
      "",
      "   if (isLocalPlayer() && isMode(DETONATED)) {",
      "      // MESMO gating de fase que AbstractWeapon::updateTC() usa para o TOF",
      "      // (fase 3, dt*4.0 -- o frame de tempo critico inteiro, nao so' o",
      "      // quarto que corresponde a esta fase).",
      "      const unsigned int ph{getWorldModel()->phase()};",
      "      if (ph == 3) {",
      "         detonatedLingerSec_ += dt * 4.0;",
      "         if (detonatedLingerSec_ >= kLingerSec) {",
      "            setMode(DELETE_REQUEST);",
      "         }",
      "      }",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "EDL: stores GuidedMissile": {
    file: "sandbox/A4-6DOF-MISSILE/configs/scenario_a4_6dof_missile.edl.in",
    line: 196,
    lang: "edl",
    lines: [
      "               stores: ( StoresMgr",
      "                  numStations: 1",
      "                  stores: {",
      "                     1: ( GuidedMissile",
      "                        id: 501",
      "                        side: blue",
      "                        type: \"AIM-X\"",
      "                        signature: ( SigSphere radius: 0.2 )",
      "                        dataLogTime: ( Seconds 0.1 )",
      "                        maxTOF: ( Seconds 60 )",
      "                        lethalRange: ( Meters 30 )",
      "                        maxBurstRng: ( Meters 150 )",
      "                     )",
      "                  }",
    ],
    trunc: true,
  },
  "EDL: behavior launchEnvelope": {
    file: "sandbox/A4-6DOF-MISSILE/configs/scenario_a4_6dof_missile.edl.in",
    line: 217,
    lang: "edl",
    lines: [
      "                  behavior: ( BtBehavior",
      "                     treeFile: \"./dist/share/mixr-plugins/A-4/flight_tree_missile_demo.xml\"",
      "                     patrolHeading:  ( Degrees 90 )",
      "                     legTime:        ( Seconds 120 )",
      "                     legTurn:        ( Degrees 90 )",
      "                     patrolAltitude: ( Meters 2000 )",
      "                     patrolSpeed:    250.0",
      "                     rtbAltitude:    ( Meters 2100 )",
      "                     rtbSpeed:       260.0",
      "                     arrivalRadius:  ( NauticalMiles 2.0 )",
      "                     fuelReserve:    0.35",
      "                     breakTurn:      ( Degrees 110 )",
      "                     evadeClimb:     ( Meters 400 )",
      "                     evadeSpeed:     280.0",
      "                     supportSpeed:   260.0",
      "                     evadeHold:      ( Seconds 30 )",
      "                     terrainClearance: ( Meters 0 )     // sem terreno neste cenario -- piso absoluto (200 m) so",
      "                     // Envelope de disparo (domain/LaunchPolicy.hpp) --",
      "                     // dentro da distancia inicial de 15 NM entre os dois",
      "                     // players, alcancado por volta de t=75..115s.",
      "                     launchMinRange: ( NauticalMiles 0.3 )",
      "                     launchMaxRange: ( NauticalMiles 5.0 )",
      "                     launchCone:     ( Degrees 45 )",
      "                  )",
    ],
    trunc: true,
  },
  "BtBehavior::configurePlans (launchEnvelope)": {
    file: "models/players/A-4/src/ubf/BtBehavior.cpp",
    line: 148,
    lines: [
      "   rtb.configure(0.0, 0.0, tune.arrivalRadiusM, tune.rtbAltitudeM, tune.rtbSpeedKts);",
      "",
      "   launchEnvelope_.minRangeM = tune.launchMinRangeM;",
      "   launchEnvelope_.maxRangeM = tune.launchMaxRangeM;",
      "   launchEnvelope_.coneDeg = tune.launchConeDeg;",
      "",
      "   domain::EvasionLimits limits;",
    ],
    trunc: true,
  },
  "domain::inLaunchEnvelope": {
    file: "models/players/A-4/src/domain/LaunchPolicy.cpp",
    line: 1,
    lines: [
      "#include \"domain/LaunchPolicy.hpp\"",
      "",
      "#include \"domain/geometry.hpp\"",
      "",
      "#include <cmath>",
      "",
      "namespace domain {",
      "",
      "bool inLaunchEnvelope(const LaunchEnvelope& env, const double rangeM, const double relBearingDeg)",
      "{",
      "   if (rangeM < env.minRangeM || rangeM > env.maxRangeM) return false;",
      "",
      "   // wrap180() por defesa -- contactRelBearingDeg ja chega em (-180,180] da",
      "   // percepcao, mas a funcao nao deveria depender disso pra estar correta.",
      "   return std::fabs(wrap180(relBearingDeg)) <= env.coneDeg;",
      "}",
      "",
      "} // namespace domain",
    ],
    trunc: false,
  },
  "AbstractPlayer::Mode (enum)": {
    file: "contexts/src/mixr/include/mixr/simulation/AbstractPlayer.hpp",
    line: 34,
    lines: [
      "   // Player mode",
      "   enum Mode {",
      "      INACTIVE,         // Player is not being updated and is not being sent to the networks",
      "      ACTIVE,           // Player is being updated and is being sent to the networks",
      "      KILLED,           // Player was killed   (One of the dead conditions)",
      "      CRASHED,          // Player crashed      (One of the dead conditions)",
      "      DETONATED,        // Weapon player has detonated (One of the dead conditions) (Original & flyout weapons)",
      "      PRE_RELEASE,      // Weapon player is created but not released (Flyout weapons only)",
      "      LAUNCHED,         // Weapon player has been launched (Original weapons only)",
      "      DELETE_REQUEST    // Request player removal from the active player list",
      "   };",
    ],
    trunc: false,
  },
  "Stores::isWeaponAvailable": {
    file: "contexts/src/mixr/src/models/system/Stores.cpp",
    line: 174,
    lines: [
      "// Default weapon availability function",
      "bool Stores::isWeaponAvailable(const unsigned int s) const",
      "{",
      "   // Map 's' to a station array index",
      "   int idx{mapSta2Idx(s)};",
      "",
      "   // get the weapon",
      "   bool isAvail{};",
      "   if (idx >= 0 && weaponTbl[idx] != nullptr) {",
      "      const AbstractWeapon* wpn{weaponTbl[idx]->getPointer()};",
      "",
      "      // Reasons why the weapon may not be available ...",
      "      bool notAvail{wpn->isReleased() || wpn->isBlocked() || wpn->isJettisoned() || wpn->isFailed() || wpn->isHung()};",
      "",
      "      // and it is if it is not not ;-)",
      "      isAvail = !notAvail;",
      "",
      "      wpn->unref();",
      "   }",
      "   return isAvail;",
      "}",
    ],
    trunc: false,
  },
  "domain::proportionalNavigation": {
    file: "models/players/missile/src/domain/Guidance.cpp",
    line: 14,
    lines: [
      "GuidanceCommand proportionalNavigation(const Vec3& relPos, const Vec3& relVel, const GuidanceGains& gains)",
      "{",
      "   const double range2d = std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e);",
      "   const double range3d = std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e + relPos.d * relPos.d);",
      "",
      "   const double losAz = std::atan2(relPos.e, relPos.n);",
      "   const double losEl = std::atan2(-relPos.d, range2d);",
      "",
      "   // taxa de azimute da LOS: d(atan2(e,n))/dt = (n*ve - e*vn) / (n^2+e^2).",
      "   double losAzRate{0.0};",
      "   if (range2d > kMinRangeForLosRateM) {",
      "      losAzRate = (relPos.n * relVel.e - relPos.e * relVel.n) / (range2d * range2d);",
      "   }",
      "",
      "   // taxa de elevacao da LOS: el = atan2(h, r), h=-relPos.d, r=range2d.",
      "   // d(el)/dt = (r*dh/dt - h*dr/dt) / (r^2+h^2), dr/dt = (n*vn+e*ve)/r.",
      "   double losElRate{0.0};",
      "   if (range3d > kMinRangeForLosRateM && range2d > kMinRangeForLosRateM) {",
      "      const double h{-relPos.d};",
      "      const double dRange2dDt{(relPos.n * relVel.n + relPos.e * relVel.e) / range2d};",
      "      const double dhDt{-relVel.d};",
      "      losElRate = (range2d * dhDt - h * dRange2dDt) / (range3d * range3d);",
      "   }",
      "",
      "   GuidanceCommand cmd{};",
      "   cmd.cmdHeadingRad = losAz + gains.navRatio * losAzRate;",
      "   cmd.cmdPitchRad = losEl + gains.navRatio * losElRate;",
      "   cmd.cmdSpeedMps = gains.cruiseSpeedMps;",
      "   return cmd;",
      "}",
    ],
    trunc: false,
  },
  "domain::proximityFuze": {
    file: "models/players/missile/src/domain/Guidance.cpp",
    line: 45,
    lines: [
      "FuzeOutcome proximityFuze(const Vec3& relPos, const Vec3& relVel, const double burstRangeM, const FuzeState& prev)",
      "{",
      "   const double range{std::sqrt(relPos.n * relPos.n + relPos.e * relPos.e + relPos.d * relPos.d)};",
      "",
      "   // taxa de alcance: d(range)/dt = (relPos . relVel) / range. Negativa =",
      "   // aproximando (alcance diminuindo).",
      "   double rangeRate{0.0};",
      "   if (range > kMinRangeForLosRateM) {",
      "      rangeRate = (relPos.n * relVel.n + relPos.e * relVel.e + relPos.d * relVel.d) / range;",
      "   }",
      "   const bool approaching{rangeRate < 0.0};",
      "",
      "   FuzeOutcome out{};",
      "   out.nextState = FuzeState{/*hasSample=*/true, approaching};",
      "",
      "   if (prev.hasSample && prev.wasApproaching && !approaching) {",
      "      out.closestApproachReached = true;",
      "      out.rangeAtEventM = range;",
      "      out.hit = (range <= burstRangeM);",
      "   }",
      "",
      "   return out;",
      "}",
    ],
    trunc: false,
  },
  "AbstractWeapon::updateTOF": {
    file: "contexts/src/mixr/src/models/player/weapon/AbstractWeapon.cpp",
    line: 691,
    lines: [
      "//------------------------------------------------------------------------------",
      "// updateTOF -- default time of flight",
      "//------------------------------------------------------------------------------",
      "void AbstractWeapon::updateTOF(const double dt)",
      "{",
      "   // As long as we're active ...",
      "   if (isMode(ACTIVE)) {",
      "",
      "      // update time of flight,",
      "      setTOF( getTOF() + dt );",
      "",
      "      // and check for the end of the flight",
      "      if (getTOF() >= getMaxTOF()) {",
      "         setMode(DETONATED);",
      "         setDetonationResults( DETONATE_DETONATION );",
      "",
      "         BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_WEAPON_DETONATION )",
      "            SAMPLE_3_OBJECTS( this, getLaunchVehicle(), getTargetPlayer() )",
      "            SAMPLE_2_VALUES( DETONATE_DETONATION, 0.0 )",
      "         END_RECORD_DATA_SAMPLE()",
      "",
      "         return;",
      "      }",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "Missile::Missile (construtor)": {
    file: "contexts/src/mixr/src/models/player/weapon/Missile.cpp",
    line: 51,
    lines: [
      "Missile::Missile()",
      "{",
      "   STANDARD_CONSTRUCTOR()",
      "",
      "   static base::String generic(\"GenericMissile\");",
      "   setType(&generic);",
      "",
      "   setMaxTOF(60.0);",
      "   setLethalRange(30.0f);",
      "   setMaxBurstRng(150.0f);",
      "   setTSG(1.0);",
      "   setSOBT(0.0f);",
      "   setEOBT(60.0f);",
      "",
      "   setVpMin(0.0);",
      "   setVpMax(800.0f);",
      "   setVpMaxG(800.0f);",
      "   setMaxG(4.0);",
      "   setMaxAccel(50.0);",
      "}",
    ],
    trunc: false,
  },
  "Missile::weaponGuidance (native)": {
    file: "contexts/src/mixr/src/models/player/weapon/Missile.cpp",
    line: 275,
    lines: [
      "void Missile::weaponGuidance(const double dt)",
      "{",
      "   // ---",
      "   // Control velocity:  During burn time, accel to max velocity,",
      "   //  after burn time, deaccelerate to min velocity.",
      "   // ---",
      "   if (isEngineBurnEnabled()) cmdVelocity = vpMax;",
      "   else cmdVelocity = vpMin;",
      "",
      "   // ---",
      "   // If the target's already dead,",
      "   //    then don't go away mad, just go away.",
      "   // ---",
      "   const Player* tgt = getTargetPlayer();",
      "   const Track* trk = getTargetTrack();",
      "   if (trk != nullptr) tgt = trk->getTarget();",
      "",
      "   if (tgt != nullptr && !tgt->isActive()) return;",
      "",
      "   base::Vec3d los; // Target Line of Sight",
      "   base::Vec3d vel; // Target velocity",
      "",
      "   // ---",
      "   // Basic guidance",
      "   // ---",
      "   {",
      "      // ---",
      "      // Get position and velocity vectors from the target/track",
      "      // ---",
      "      base::Vec3d posx;",
      "      calculateVectors(tgt, trk, &los, &vel, &posx);",
      "",
      "      // compute range to target",
      "      const double trng0 = trng;",
      "      trng = los.length();",
      "",
      "      // compute range rate,",
      "      //double trdot0 = trdot;",
      "      if (dt > 0)",
      "         trdot = (trng - trng0)/dt;",
      "      else",
      "         trdot = 0.0;",
      "",
      "      // Target total velocity",
      "      const double totalVel = vel.length();",
      "",
      "      // compute target velocity parallel to LOS,",
      "      const double vtplos = (los * vel/trng);",
      "",
      "      // ---",
      "      // guidance - fly to intercept point",
      "      // ---",
      "",
      "      // if we have guidance ...",
      "      if ( isGuidanceEnabled() && trng > 0) {",
      "",
      "         // get missile velocity (must be faster than target),",
      "         double v = vpMax;",
      "         if (v < totalVel) v = totalVel + 1;",
      "",
      "         // compute target velocity normal to LOS squared,",
      "         const double tgtVp = totalVel;",
      "         const double vtnlos2 = tgtVp*tgtVp - vtplos*vtplos;",
      "",
      "         // and compute missile velocity parallex to LOS.",
      "         const double vmplos = std::sqrt( v*v - vtnlos2 );",
      "",
      "         // Now, use both velocities parallel to LOS to compute",
      "         //  closure rate.",
      "         const double vclos = vmplos - vtplos;",
      "",
      "         // Use closure rate and range to compute time to intercept.",
      "         double dt1 = 0;",
      "         if (vclos > 0) dt1 = trng/vclos;",
      "",
      "         // Use time to intercept to extrapolate target position.",
      "         base::Vec3d p1 = (los + (vel * dt1));",
      "",
      "         // Compute missile commanded heading and",
      "         cmdHeading = std::atan2(p1.y(),p1.x());",
      "",
      "         // commanded pitch.",
      "         const double grng = std::sqrt(p1.x()*p1.x() + p1.y()*p1.y());",
      "         cmdPitch = -std::atan2(p1.z(),grng);",
      "",
      "      }",
      "   }",
      "",
      "   // ---",
      "   // fuzing logic  (let's see if we've scored a hit)",
      "   //  (compute range at closest point and compare to max burst radius)",
      "   //  (use target truth data)",
      "   // ---",
      "   {",
      "      // ---",
      "      // Get position and velocity vectors from the target (truth)",
      "      // (or default to the values from above)",
      "      // ---",
      "      if (tgt != nullptr) {",
      "         calculateVectors(tgt, nullptr, &los, &vel, nullptr);",
      "      }",
      "",
      "      // compute range to target",
      "      const double trng0 = trngT;",
      "      trngT = los.length();",
      "",
      "      // compute range rate,",
      "      double trdot0 = trdotT;",
      "      if (dt > 0)",
      "         trdotT = (trngT - trng0)/dt;",
      "      else",
      "         trdotT = 0;",
      "",
      "      // when we've just passed the target ...",
      "      if (trdotT > 0 && trdot0 < 0 && !isDummy() && getTOF() > 2.0) {",
      "         bool missed = true;   // assume the worst",
      "",
      "         // compute relative velocity vector.",
      "         const base::Vec3d velRel = (vel - getVelocity());",
      "",
      "         // compute missile velocity squared,",
      "         double vm2 = velRel.length2();",
      "         if (vm2 > 0) {",
      "",
      "            // relative range (dot) relative velocity",
      "            const double rdv = los * velRel;",
      "",
      "            // interpolate back to closest point",
      "            const double ndt = -rdv/vm2;",
      "            const base::Vec3d p0 = los + (velRel*ndt);",
      "",
      "            // range squared at closest point",
      "            const double r2 = p0.length2();",
      "",
      "            // compare to burst radius squared",
      "            if (r2 <= (getMaxBurstRng()*getMaxBurstRng()) ) {",
      "",
      "               // We've detonated",
      "               missed = false;",
      "               setMode(DETONATED);",
      "               setDetonationResults( DETONATE_ENTITY_IMPACT );",
      "",
      "               // compute location of the detonation relative to the target",
      "               base::Vec3d p0n = -p0;",
      "               if (tgt != nullptr) p0n = tgt->getRotMat() * p0n;",
      "               setDetonationLocation(p0n);",
      "",
      "               // Did we hit anyone?",
      "               checkDetonationEffect();",
      "",
      "               // Log the event",
      "               const double detRange = getDetonationRange();",
      "               if (isMessageEnabled(MSG_INFO)) {",
      "                  std::cout << \"DETONATE_ENTITY_IMPACT rng = \" << detRange << std::endl;",
      "               }",
      "",
      "               BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_WEAPON_DETONATION )",
      "                  SAMPLE_3_OBJECTS( this, getLaunchVehicle(), getTargetPlayer() )",
      "                  SAMPLE_2_VALUES( DETONATE_ENTITY_IMPACT, detRange )",
      "               END_RECORD_DATA_SAMPLE()",
      "",
      "            }",
      "         }",
      "",
      "         // Did we miss the target?",
      "         if (missed) {",
      "            // We've detonated ...",
      "            setMode(DETONATED);",
      "            setDetonationResults( DETONATE_DETONATION );",
      "",
      "            // because we've just missed the target",
      "            setTargetPlayer(nullptr,false);",
      "            setTargetTrack(nullptr,false);",
      "",
      "            // Log the event",
      "            const double detRange = trngT;",
      "            if (isMessageEnabled(MSG_INFO)) {",
      "               std::cout << \"DETONATE_OTHER rng = \" << detRange << std::endl;",
      "            }",
      "",
      "            BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_WEAPON_DETONATION )",
      "               SAMPLE_3_OBJECTS( this, getLaunchVehicle(), getTargetPlayer() )",
      "               SAMPLE_2_VALUES( DETONATE_DETONATION, detRange )",
      "            END_RECORD_DATA_SAMPLE()",
      "",
      "         }",
      "",
      "      }",
      "   }",
      "}",
    ],
    trunc: false,
  },
  "Missile::weaponDynamics (native)": {
    file: "contexts/src/mixr/src/models/player/weapon/Missile.cpp",
    line: 469,
    lines: [
      "void Missile::weaponDynamics(const double dt)",
      "{",
      "   static const double g = base::ETHG;              // Acceleration of Gravity",
      "",
      "   // ---",
      "   // Max turning G (Missiles: Use Gmax)",
      "   // ---",
      "   const double gmax = maxG;",
      "",
      "   // ---",
      "   // Computer max turn rate, max/min pitch rates",
      "   // ---",
      "",
      "   // Turn rate base on vp and g,s",
      "   const double ra_max = gmax * g / getTotalVelocity();",
      "",
      "   // Set max (pull up) pitch rate same as turn rate",
      "   const double qa_max = ra_max;",
      "",
      "   // Set min (push down) pitch rate",
      "   const double qa_min = -qa_max;",
      "",
      "   // ---",
      "   // Get old angular values",
      "   // ---",
      "   const base::Vec3d oldRates = getAngularVelocities();",
      "   //double pa1 = oldRates[IROLL];",
      "   const double qa1 = oldRates[IPITCH];",
      "   const double ra1 = oldRates[IYAW];",
      "",
      "   // ---",
      "   // Find pitch rate and update pitch",
      "   // ---",
      "   double qa = base::angle::aepcdRad(cmdPitch - static_cast<double>(getPitchR()));",
      "   if(qa > qa_max) qa = qa_max;",
      "   if(qa < qa_min) qa = qa_min;",
      "",
      "   // Using Pitch rate, integrate pitch",
      "   const double newTheta = static_cast<double>(getPitch() + (qa + qa1) * dt / 2.0);",
      "",
      "   // Find turn rate",
      "   double ra = base::angle::aepcdRad(cmdHeading - static_cast<double>(getHeadingR()));",
      "   if(ra > ra_max) ra = ra_max;",
      "   if(ra < -ra_max) ra = -ra_max;",
      "",
      "   // Use turn rate integrate heading",
      "   double newPsi = static_cast<double>(getHeading() + (ra + ra1) * dt / 2.0);",
      "   if(newPsi > 2.0f*base::PI) newPsi -= static_cast<double>(2.0*base::PI);",
      "   if(newPsi < 0.0f) newPsi += static_cast<double>(2.0*base::PI);",
      "",
      "   // Roll angle proportional to max turn rate - filtered",
      "   double pa = 0.0;",
      "   const double newPhi = static_cast<double>( 0.98 * getRollR() + 0.02 * ((ra / ra_max) * (base::angle::D2RCC * 60.0)) );",
      "",
      "   // Sent angular values",
      "   setEulerAngles(newPhi, newTheta, newPsi);",
      "   setAngularVelocities(pa, qa, ra);",
      "",
      "   // Find Acceleration",
      "   double vpdot = (cmdVelocity - getTotalVelocity());",
      "   if(vpdot > maxAccel)  vpdot = maxAccel;",
      "   if(vpdot < -maxAccel) vpdot = -maxAccel;",
      "",
      "   // Set acceleration vector",
      "   base::Vec3d aa(vpdot, 0.0, 0.0);",
      "   base::Vec3d ae = aa * getRotMat();",
      "   setAcceleration(ae);",
      "",
      "   // Compute new velocity",
      "   const double newVP = getTotalVelocity() + vpdot * dt;",
      "",
      "   // Set acceleration vector",
      "   //base::Vec3 ve0 = getVelocity();",
      "   const base::Vec3d va(newVP, 0.0, 0.0);",
      "   const base::Vec3d ve1 = va * getRotMat();",
      "   setVelocity(ve1);",
      "   setVelocityBody(newVP, 0.0, 0.0);",
      "}",
    ],
    trunc: false,
  },
};

// Mesma cadeia de fallback de flightSnip() logo acima -- FlightAgentTC::controller
// e Agent::controller ja estao extraidos de verdade (FLIGHT_SNIPPETS/SNIPPETS), a
// trilha do missil so precisou preencher o que faltava (MISSILE_SNIPPETS).
const missileSnip = (key) => (key ? MISSILE_SNIPPETS[key] || FLIGHT_SNIPPETS[key] || SNIPPETS[key] || null : null);

/* Cada passo: {id, stage, cls, method, call, src, hl, tag, flag, note}
 *  - cls/method   -- cabecalho mostrado acima do codigo ("ClasseDona::metodo").
 *  - call         -- descricao curta de UMA linha ("quem chama quem").
 *  - src/hl       -- chave em MISSILE_SNIPPETS + janela [inicio,fim] (indices EM 'lines', 0-based).
 *  - tag          -- selo curto (token/valor do evento nativo, ou null se nao emite nada).
 *  - flag         -- null | "info" | "gotcha" -- controla a cor do selo/borda no detalhe.
 *  - note         -- array de paragrafos (string) explicando a nuance deste passo.
 */
const MISSILE_TRACE = [
  {
    id: "edl-stores",
    stage: 0,
    cls: null,
    method: null,
    title: "EDL -- o cabide do atirador",
    call: "dado -- o cabide do atirador antes de qualquer C++ rodar",
    src: "EDL: stores GuidedMissile",
    hl: [3, 12],
    tag: null,
    flag: null,
    note: [
      "Ponto de partida: o bloco stores: do a4_shooter, em sandbox/A4-6DOF-MISSILE/configs/scenario_a4_6dof_missile.edl.in. UMA estação, UM ( GuidedMissile ) -- é essa contagem que faz o segundo disparo ser impossível: StoresMgr::available() (ver mais abaixo) só volta a contar quando há OUTRA arma livre no cabide, e não há nenhuma.",
      "maxTOF/lethalRange/maxBurstRng aqui são valores do CENÁRIO, sobrescrevendo os defaults do construtor de GuidedMissile (60 s / 30 m / 150 m -- por coincidência, os mesmos números; o cenário só está sendo explícito). id/side/type são slots comuns de qualquer Player -- nada específico de arma.",
    ],
  },
  {
    id: "edl-behavior",
    stage: 0,
    cls: null,
    method: null,
    title: "EDL -- o envelope de disparo",
    call: "dado -- o envelope de disparo, três slots a mais na MESMA árvore de tuning que já existia",
    src: "EDL: behavior launchEnvelope",
    hl: [17, 22],
    tag: null,
    flag: null,
    note: [
      "launchMinRange/launchMaxRange/launchCone são TRÊS slots novos de BtBehavior, ao lado de dezenas de outros já existentes (patrolHeading, breakTurn, evadeHold, ...) -- é a MESMA classe que já orquestra patrulha/evasão/RTB, só ganhando mais três números. Não há um 'behavior de lançamento' separado.",
      "0,3-5,0 NM (556-9260 m) é DIFERENTE do default do domain::LaunchEnvelope (500-9000 m, ver o próximo passo) -- o cenário está deliberadamente estreitando a janela superior um pouco. Nada aqui impede o autor de um cenário de esquecer estes três slots: sem eles, os defaults do struct C++ valem, silenciosamente.",
    ],
  },
  {
    id: "btbehavior-configureplans",
    stage: 0,
    cls: "BtBehavior",
    method: "configurePlans() -- trecho",
    call: "roda uma vez, no reset() do player -- é aqui que os três slots acima viram o struct domain::LaunchEnvelope",
    src: "BtBehavior::configurePlans (launchEnvelope)",
    hl: [2, 4],
    tag: null,
    flag: null,
    note: [
      "tune (um domain::BtTuning já preenchido a partir dos slots de EDL) é só copiado campo a campo para launchEnvelope_ -- nenhuma conversão de unidade aqui (NauticalMiles/Degrees já viraram double em metros/graus na própria fronteira do slot, setSlotLaunchMinRange() etc. em BtBehaviorSlots.cpp, fora desta trilha).",
      "launchEnvelope_ é um MEMBRO do BtBehavior, lido por LaunchEnvelopeCondition::tick() via context_.behavior->launchEnvelope() (ver o primeiro passo desta trilha) -- o nó de árvore nunca lê o EDL diretamente, só esse getter.",
    ],
  },
  {
    id: "flightagenttc-controller",
    stage: 0,
    cls: "FlightAgentTC",
    method: "controller(dt) -- de onde tudo começa, a cada frame",
    call: "roda na fase 3 do frame de tempo crítico, uma vez por aeronave, no pool T/C",
    src: "FlightAgentTC::controller",
    hl: [0, 24],
    tag: null,
    flag: null,
    note: [
      "Este é o ponto de entrada real de TODA a cadeia desta trilha -- os 23 passos seguintes só acontecem porque este método roda, a até 50 Hz, para o a4_shooter. world->phase() != 3 filtra as outras três fases do frame (dynamics/transmit/receive) -- decisão só na fase 3, mesmo padrão documentado na seção 'O modelo MIXR em uma tela' do CLAUDE.md raiz.",
      "BaseClass::controller(dt * 4.0) é quem desce para base::ubf::Agent::controller() (próximo passo) -- FlightAgentTC não decide nada sozinho, só publica no xboard (thread, contagem) e repassa pro framework UBF genérico.",
    ],
  },
  {
    id: "agent-controller",
    stage: 0,
    cls: "Agent",
    method: "controller(dt) -- framework UBF NATIVO, não deste modelo",
    call: "state->updateState(actor) primeiro, depois getBehavior()->genAction(state, dt)",
    src: "Agent::controller",
    hl: [0, 15],
    tag: null,
    flag: null,
    note: [
      "mixr::base::ubf::Agent -- classe NATIVA do MIXR, genérica: não sabe nada sobre BtBehavior, míssil ou A-4. getState()->updateState(actor) (próximo passo) primeiro, sempre -- a percepção é atualizada ANTES de qualquer decisão no mesmo frame.",
      "getBehavior()->genAction(state, dt) é o polimorfismo que alcança BtBehavior::genAction() (fora desta trilha -- percorre a árvore de comportamento, tickRoot(), e é lá dentro que o Fallback/Sequence chega em LaunchEnvelopeCondition/LaunchMissileAction, os dois próximos passos). action->execute(actor) (se genAction() devolveu algo) é o ponto que, para uma decisão de lançamento, alcança ubf::FlightAction::execute() -- o mesmo método que aparece mais abaixo nesta trilha fazendo o disparo de verdade.",
    ],
  },
  {
    id: "flightstate-updatestate",
    stage: 0,
    cls: "FlightState",
    method: "updateState(actor) -- de onde vem snap.weaponReady",
    call: "chamado pelo passo anterior, ANTES de qualquer nó de árvore ticar neste frame",
    src: "FlightState::updateState",
    hl: [78, 84],
    tag: null,
    flag: null,
    note: [
      "Responde a pergunta que o primeiro passo desta trilha deixou em aberto: de onde vem snap.weaponReady. air->getStoresManagement() é o MESMO Player::getStoresManagement() que ubf::FlightAction::execute() usa para disparar de verdade -- a percepção e a atuação leem o MESMO StoresMgr, só em momentos diferentes do frame.",
      "storesMgr->available() > 0 -- StoresMgr é OPCIONAL (nem todo player de produção declara stores:); sem ele, storesMgr é nullptr e weaponReady fica false para sempre, sem erro nenhum. isWeaponAvailable() por baixo de available() é o passo 'Stores::isWeaponAvailable' mais adiante nesta trilha.",
    ],
  },
  {
    id: "abstractplayer-mode",
    stage: 0,
    cls: "AbstractPlayer",
    method: "enum Mode -- o vocabulário que o resto desta trilha inteira usa",
    call: "referência -- não é uma chamada, é a declaração que dá nome a cada estado citado adiante",
    src: "AbstractPlayer::Mode (enum)",
    hl: [1, 10],
    tag: null,
    flag: null,
    note: [
      "Todo mode: citado nos passos seguintes (PRE_RELEASE, ACTIVE, DETONATED, KILLED, CRASHED, LAUNCHED, DELETE_REQUEST) vem desta ÚNICA declaração, em mixr::simulation::AbstractPlayer -- a classe-base que mixr::models::Player estende. Os comentários originais (em inglês, preservados aqui) já dizem o essencial: KILLED e CRASHED são duas das 'dead conditions', DETONATED é a terceira (mísseis/flyouts); PRE_RELEASE e LAUNCHED só existem para arma (o objeto ORIGINAL no cabide vira LAUNCHED, o CLONE que voa nasce PRE_RELEASE -- ver o passo AbstractWeapon::release() mais adiante).",
      "Note o que NÃO existe: não há COLLIDED nem TIMEOUT nem HIT/MISS -- um acerto e um erro de míssil são ambos DETONATED (diferenciados só por getDetonationResults(), um campo separado, não pelo mode). O mode é sobre CICLO DE VIDA do objeto, não sobre o resultado do que aconteceu a ele.",
    ],
  },
  {
    id: "envelope",
    stage: 1,
    cls: "LaunchEnvelopeCondition",
    method: "tick()",
    call: "condição da árvore (BT.CPP) -- roda na fase 3, thread do POOL T/C do atirador",
    src: "LaunchEnvelopeCondition::tick",
    hl: [7, 18],
    tag: null,
    flag: null,
    note: [
      "Primeiro nó do ramo de disparo (Sequence \"launch_sequence\" de flight_tree_missile_demo.xml). Nenhum objeto MIXR de arma é tocado aqui -- é uma condição pura de domínio sobre um SNAPSHOT já colhido (WorldView), a mesma disciplina que o resto da árvore deste modelo já segue.",
      "snap.weaponReady vem de domain::WorldView, preenchido no início do frame a partir de StoresMgr::available() > 0 -- ou seja, este nó nunca chama available() ele mesmo; lê um valor que já pode ter até um frame de atraso (mesma latência já documentada para contactLive()/engaged() em domain::ThreatPolicy).",
    ],
  },
  {
    id: "inlaunchenvelope",
    stage: 1,
    cls: "domain",
    method: "inLaunchEnvelope(env, rangeM, relBearingDeg)",
    call: "chamado pelo passo anterior -- a geometria pura por trás do SUCCESS/FAILURE",
    src: "domain::inLaunchEnvelope",
    hl: [8, 15],
    tag: null,
    flag: null,
    note: [
      "Função LIVRE (não é método de classe nenhuma), sem MIXR, sem BehaviorTree.CPP -- só dois double e um struct de entrada. Mesmo alcance mínimo (556 m) e cone (45°) de tune.launchMinRange/launchCone, já convertidos para metros/graus antes de chegar aqui (a conversão de unidade acontece na fronteira do slot, não nesta função).",
      "wrap180(relBearingDeg) é defesa, não necessidade medida: o comentário do próprio código admite que contactRelBearingDeg já chega em (-180,180] da percepção -- a chamada aqui é para a função nunca DEPENDER dessa garantia externa para estar correta, o mesmo tipo de rigor defensivo que domain::ThreatPolicy já demonstra em outro lugar deste modelo.",
    ],
  },
  {
    id: "launch-action",
    stage: 1,
    cls: "LaunchMissileAction",
    method: "tick()",
    call: "segundo nó da Sequence -- só executa se LaunchEnvelope já teve SUCCESS neste tick",
    src: "LaunchMissileAction::tick",
    hl: [15, 32],
    tag: null,
    flag: null,
    note: [
      "Ainda nenhum objeto MIXR de arma é tocado. O nó só marca um PEDIDO -- decision.launchRequested = true e decision.launchTargetName = snap.contactName -- dois campos de FlightDecision, uma estrutura própria deste modelo, não do MIXR.",
      "Quem de fato libera o míssil é a ATUAÇÃO (o próximo passo), não este nó de árvore. É a mesma separação decisão/atuação que o resto do modelo já usa (domain/bt decidem, xnative::FlightAction executa) -- aqui só chega mais longe: até tocar um Player::getStoresManagement() de verdade.",
    ],
  },
  {
    id: "flightaction-find",
    stage: 2,
    cls: "FlightAction",
    method: "execute()",
    call: "atuação -- acha o Player-alvo por nome e confere o cabide antes de liberar",
    src: "FlightAction::execute (lancamento)",
    hl: [8, 26],
    tag: null,
    flag: null,
    note: [
      "PRIMEIRO ponto deste modelo que toca um objeto MIXR de ARMA -- tudo antes disso (os dois nós de árvore) era domínio puro. O alvo é resolvido por NOME (WorldModel::findPlayerByName), não por ponteiro guardado -- a árvore só sabia o nome da pista (snap.contactName), nunca um Player* (o snapshot não guarda ponteiro nenhum de framework).",
      "storesMgr->available() == 0 aborta o lançamento em silêncio funcional (só um LOG(WARNING) -- nenhuma exceção, nenhum crash). Num cabide de UM só míssil (como sandbox/A4-6DOF-MISSILE), isto é exatamente o que impede o SEGUNDO disparo: available() cai a zero assim que o primeiro sai, e LaunchEnvelopeCondition já falha no frame seguinte -- sem nenhum estado extra escrito por este modelo.",
    ],
  },
  {
    id: "isweaponavailable",
    stage: 2,
    cls: "Stores",
    method: "isWeaponAvailable(s) -- por que o SEGUNDO disparo nunca sai",
    call: "chamado por available(), que roda a cada frame dentro de FlightState::updateState() (ver o prólogo desta trilha)",
    src: "Stores::isWeaponAvailable",
    hl: [8, 15],
    tag: null,
    flag: null,
    note: [
      "StoresMgr::available() (herdado, não sobrescrito -- é este mesmo Stores::available(), não mostrado à parte por ser um laço trivial de UMA linha somando isWeaponAvailable(s) para s de 1 a ns) conta quantas estações têm uma arma 'disponível'. Esta função é onde 'disponível' vira uma definição concreta: nem released, nem blocked, nem jettisoned, nem failed, nem hung.",
      "isReleased() é o campo que o passo AbstractWeapon::release() (mais adiante) vai LIGAR no míssil original assim que o disparo acontecer -- é essa mudança, sozinha, que faz available() cair de 1 para 0 no PRÓXIMO frame, sem nenhum contador escrito por este modelo. wpn->getPointer() (a mesma função pré-ref'd já vista em SimpleStoresMgr::getNextMissileImp) e wpn->unref() logo depois fecham o ciclo de referência dentro da PRÓPRIA checagem -- nada vaza mesmo rodando a até 50 Hz.",
    ],
  },
  {
    id: "getnextmissile",
    stage: 2,
    cls: "SimpleStoresMgr",
    method: "getNextMissileImp()",
    call: "storesMgr->releaseOneMissile() (chamado no passo anterior) desce até aqui",
    src: "SimpleStoresMgr::getNextMissileImp",
    hl: [0, 22],
    tag: null,
    flag: "info",
    note: [
      "\"StoresMgr\" no .edl deste cenário NÃO constrói a classe abstrata StoresMgr (cujo releaseOneMissile() nativo é só `{ return nullptr; }`) -- constrói SimpleStoresMgr, que registra o nome de fábrica \"StoresMgr\" (IMPLEMENT_SUBCLASS(SimpleStoresMgr, \"StoresMgr\")). Mesma armadilha já documentada no CLAUDE.md para o cenário 'player máximo'.",
      "getNextMissileImp() acha o míssil por dynamic_cast<Missile*> em CADA item de stores -- casa QUALQUER subclasse de Missile, nativa (Aam/Agm/Sam) ou de terceiro (GuidedMissile deste repo). Pega o PRIMEIRO cujo isInactive()||isReleaseHold() seja true -- \"o primeiro livre\", não \"o mais adequado\" (não há lógica de seleção por alcance/tipo de alvo).",
      "p->getPointer() é o nascimento da referência PRÉ-REF'D: devolve flyoutWpn (se já existir) ou faz this->ref() e devolve this. É essa referência extra que o chamador (FlightAction::execute()) tem a obrigação de desfazer com unref() mais adiante -- ver o passo \"seta o alvo\".",
    ],
  },
  {
    id: "releaseweapon",
    stage: 2,
    cls: "Stores",
    method: "releaseWeapon(AbstractWeapon*)",
    call: "SimpleStoresMgr::releaseOneMissile() repassa o Missile* achado",
    src: "Stores::releaseWeapon",
    hl: [0, 14],
    tag: null,
    flag: null,
    note: [
      "Duas linhas, nenhuma decisão: fixa o lançador (setLaunchVehicle(own)) e delega tudo -- clonagem, troca de mode, registro no DataRecorder -- para AbstractWeapon::release() (próximo passo). \"own\" aqui é o Player dono do StoresMgr (a4_shooter), obtido por getOwnship().",
    ],
  },
  {
    id: "abstractweapon-release",
    stage: 2,
    cls: "AbstractWeapon",
    method: "release()",
    call: "clona a si mesmo -- o CLONE é quem de fato voa; o objeto original (no cabide) fica \"LAUNCHED\"",
    src: "AbstractWeapon::release",
    hl: [67, 90],
    tag: "REID_WEAPON_RELEASED (61)",
    flag: "gotcha",
    note: [
      "flyout = this->clone() -- o míssil que sobrevoa a partir daqui NÃO é o objeto declarado em stores: no .edl; é uma CÓPIA dele, inserida na lista de players via sim->addNewPlayer(\"W%05d\", flyout) (a mesma convenção de nome \"W00501\" etc. que aparece no Tacview/dump de qualquer flyout deste framework). O original vira setMode(Player::LAUNCHED) -- \"disparado\", não removido -- e nunca mais voa.",
      "flyout->setMode(PRE_RELEASE) -- o clone nasce em PRE_RELEASE, não ACTIVE: ainda vai levar UM frame inteiro preso à posição/atitude do lançador (ver o próximo passo, AbstractWeapon::dynamics()) antes de ganhar vida própria.",
      "GOTCHA medido neste projeto: este método grava BEGIN_RECORD_DATA_SAMPLE(..., REID_WEAPON_RELEASED) incondicionalmente -- mas o `dataRecorder:` de sandbox/A4-6DOF-MISSILE declara `enabledList: [ 43 42 ]` (REID_PLAYER_DATA + REID_PLAYER_REMOVED), o mesmo workaround já documentado no CLAUDE.md para a poc/09 (token 61 tem histórico de derrubar o processo em outro handler nativo). Resultado: o evento de LANÇAMENTO É emitido pelo C++ nativo, mas nunca chega ao Tacview/arquivo desta demonstração -- é filtrado hoje pelo `isDataEnabled()` do lado do gravador (allowlist, não denylist: com enabledList não-vazio, SÓ os IDs listados passam).",
    ],
  },
  {
    id: "settarget",
    stage: 2,
    cls: "FlightAction",
    method: "execute() -- continuação",
    call: "de volta à atuação: mira o flyout recém-liberado e devolve a referência extra",
    src: "FlightAction::execute (lancamento)",
    hl: [26, 31],
    tag: null,
    flag: null,
    note: [
      "flyout->setTargetPlayer(target, /*posTrkEnb=*/true) -- o segundo (e último) objeto MIXR de arma que este modelo toca diretamente. 'target' é o Player* do a4_target, resolvido por nome dois passos atrás.",
      "flyout->unref() -- fecha o ciclo de referência aberto em getPointer() (passo \"getNextMissileImp\"): releaseOneMissile() devolve pré-ref'd, e este é o unref() correspondente. Sem ele, o míssil vazaria uma referência a cada disparo -- o padrão \"pega pré-ref'd, usa, unref()\" é o mesmo já usado em qualquer outro ponteiro pré-ref'd deste framework (ex.: getPilotByType() duas linhas acima, no início de execute()).",
    ],
  },
  {
    id: "settargetplayer-native",
    stage: 2,
    cls: "Missile",
    method: "setTargetPlayer(Player*, bool) -- override",
    call: "GuidedMissile NÃO sobrescreve este método -- quem roda é o de Missile (que chama o de AbstractWeapon por baixo)",
    src: "Missile::setTargetPlayer",
    hl: [0, 9],
    tag: null,
    flag: "info",
    note: [
      "A ÚNICA coisa que Missile acrescenta sobre o AbstractWeapon::setTargetPlayer() de base (mostrado no próximo passo) é resetar trngT/trdotT -- o alcance/taxa 'ground truth' que o FUZING NATIVO de Missile::weaponGuidance() usaria. GuidedMissile não usa esses dois campos (tem a própria espoleta em domain::proximityFuze) -- eles ficam escritos e nunca lidos neste modelo, herança inofensiva.",
      "posTrkEnb=true aqui não é o gate de isGuidanceEnabled() (que olha isTargetPositionValid()/tgtPosValid) -- é o que decide se AbstractWeapon::updateTC() (fase 3) vai RECONTINUAMENTE reatualizar a posição do alvo a cada tick (positionTracking(), 'fake it and just follow the target' no comentário nativo) em vez de congelar no valor lido uma única vez.",
    ],
  },
  {
    id: "settargetplayer-base",
    stage: 2,
    cls: "AbstractWeapon",
    method: "setTargetPlayer(Player*, bool)",
    call: "Missile::setTargetPlayer() chama BaseClass::setTargetPlayer(tgt, pt) no final",
    src: "AbstractWeapon::setTargetPlayer",
    hl: [0, 10],
    tag: null,
    flag: null,
    note: [
      "tgtPlayer é base::safe_ptr<Player> -- a atribuição tgtPlayer = tgt já cuida do ref-count sozinha, sem ref()/unref() escrito à mão aqui.",
      "positionTracking() (chamado na última linha) já popula tgtPos/tgtVel NA HORA, via setTargetPosition() -- que também liga tgtPosValid=true. É esse flag, não posTrkEnb, que entra em isGuidanceEnabled() (ver o próximo estágio).",
    ],
  },
  {
    id: "phase0-transition",
    stage: 3,
    cls: "AbstractWeapon",
    method: "updateTC(dt)",
    call: "frame SEGUINTE, fase 0 do pool T/C -- roda pra TODO player ativo, não só o míssil",
    src: "AbstractWeapon::updateTC",
    hl: [11, 15],
    tag: null,
    flag: "info",
    note: [
      "Fase 0 é dinâmica -- por que a transição PRE_RELEASE→ACTIVE mora exatamente aqui, e não na liberação: o comentário do próprio fonte (linhas 231-233 do arquivo) explica que a posição do míssil em PRE_RELEASE ainda é RELATIVA ao lançador (calculada em AbstractWeapon::dynamics(), próximo estágio) -- só depois que BaseClass::updateTC(dt) já rodou essa dinâmica é que a posição absoluta existe, e só então faz sentido soltar o míssil sozinho no mundo.",
      "!isReleaseHold() -- neste projeto sempre true (release() já chama setReleaseHold(false) na liberação direta), mas é o gate que faz o padrão 'prerelease() primeiro, release() depois' (usado por outras classes deste framework, ex. estações com tempo de preparo) funcionar sem duplicar este método.",
    ],
  },
  {
    id: "atreleaseinit-native",
    stage: 3,
    cls: "Missile",
    method: "atReleaseInit()",
    call: "atReleaseInit() chamado pelo passo anterior -- Missile faz a parte NATIVA, depois GuidedMissile sobrescreve",
    src: "Missile::atReleaseInit",
    hl: [8, 33],
    tag: null,
    flag: null,
    note: [
      "getDynamicsModel() == nullptr aqui é o MESMO gate que decide, em AbstractWeapon::dynamics() (dois estágios à frente), se o míssil usa os hooks cinemáticos -- ambos checam a mesma condição, cada um na sua própria classe.",
      "Este método semeia cmdPitch/cmdHeading/cmdVelocity -- campos PRÓPRIOS de Missile. GuidedMissile nunca os lê (usa cmdHeadingRad_/cmdPitchRad_/cmdSpeedMps_, campos próprios dele) -- é herança que roda e não faz mal, mas também não faz nada útil aqui: quem de fato importa é o override do próximo passo.",
    ],
  },
  {
    id: "atreleaseinit-custom",
    stage: 3,
    cls: "GuidedMissile",
    method: "atReleaseInit() -- override",
    call: "chamado no MESMO ponto do passo anterior (é BaseClass::atReleaseInit(); primeira linha do override)",
    src: "GuidedMissile::atReleaseInit",
    hl: [19, 26],
    tag: null,
    flag: "gotcha",
    note: [
      "Sem este override (achado RODANDO, não suposto -- o comentário do próprio arquivo documenta a medição): os campos cmdHeadingRad_/cmdPitchRad_/cmdSpeedMps_ ficam no valor de inicialização de classe (0.0) até tof>=tsg -- mas weaponDynamics() já roda TODO frame, independente do TSG. Resultado medido: o míssil guina ativamente para rumo/pitch GEOGRÁFICO ZERO (Norte, nivelado) e desacelera para 0 m/s durante toda a janela do TSG (aqui, 1.0 s) -- até ~66° de rumo perdidos antes da navegação proporcional assumir.",
      "getHeadingR()/getPitchR()/getVpMax() no instante da chamada são a atitude/velocidade de LANÇAMENTO -- copiadas do lançador em AbstractWeapon::dynamics() (isMode(PRE_RELEASE), estágio anterior), então o comando inicial é 'continue fazendo o que já estava fazendo' até a guiagem ligar de verdade.",
    ],
  },
  {
    id: "dynamics-dispatch",
    stage: 4,
    cls: "AbstractWeapon",
    method: "dynamics(dt)",
    call: "TODO frame, fase 0 -- decide o caminho CINEMÁTICO vs. dynamicsModel de verdade",
    src: "AbstractWeapon::dynamics",
    hl: [37, 46],
    tag: null,
    flag: "info",
    note: [
      "getDynamicsModel() == nullptr é a ÚNICA condição que decide se weaponGuidance(dt)/weaponDynamics(dt) rodam -- exatamente esta linha é a razão de GuidedMissile nunca declarar dynamicsModel: nenhum no .edl (nem JSBSimModel, nem qualquer outro): declarar um desligaria os dois hooks silenciosamente, sem erro nenhum, e o míssil voaria reto pela inércia do release() até bater no maxTOF.",
      "BaseClass::dynamics(dt) roda LOGO DEPOIS, incondicionalmente -- é Player::dynamics(), que sempre chama positionUpdate(dt): a POSIÇÃO nunca é escrita pelos hooks cinemáticos, só velocidade/atitude (ver o próximo estágio). Integrar posição nos dois lugares duplicaria a conta -- por isso GuidedMissile::weaponDynamics() nunca chama setPosition().",
    ],
  },
  {
    id: "weaponguidance-nav",
    stage: 4,
    cls: "GuidedMissile",
    method: "weaponGuidance(dt) -- navegação",
    call: "chamado pelo passo anterior -- lê Player* do alvo DIRETO (não usa o cache tgtPos/tgtVel herdado)",
    src: "GuidedMissile::weaponGuidance",
    hl: [22, 28],
    tag: null,
    flag: null,
    note: [
      "isGuidanceEnabled() = (getTOF() >= tsg) && ((getCategory() & GUIDED) != 0) && isTargetPositionValid() -- vem de AbstractWeapon, mas o framework NÃO impõe esse gate automaticamente sobre uma weaponGuidance() sobrescrita: quem escreveu este modelo teve que chamar isGuidanceEnabled() explicitamente (esta mesma linha). Sem ela, o míssil guiaria desde o frame 1, ignorando o TSG por completo.",
      "domain::proportionalNavigation() é função PURA (sem MIXR, testada isolada) -- soma perseguição pura (mira no ângulo atual da linha de visada) com uma correção proporcional à TAXA dessa linha, o termo clássico N'×λ̇ da navegação proporcional de verdade. Contraste: o Missile::weaponGuidance() NATIVO (usado por AamMissile/Sam/etc. quando ninguém sobrescreve) faz guiagem por PONTO DE INTERCEPTAÇÃO -- reage só a um snapshot geométrico do instante, não à taxa da LOS.",
    ],
  },
  {
    id: "proportionalnavigation",
    stage: 4,
    cls: "domain",
    method: "proportionalNavigation(relPos, relVel, gains)",
    call: "chamado pelo passo anterior -- a lei de guiagem inteira, função pura",
    src: "domain::proportionalNavigation",
    hl: [24, 28],
    tag: null,
    flag: null,
    note: [
      "Perseguição pura (losAz/losEl -- mira direto no ângulo atual da linha de visada) SOMADA a uma correção proporcional à TAXA dessa linha (losAzRate/losElRate) -- o termo clássico N'×λ̇ da navegação proporcional de verdade, aqui em heading/pitch comandados em vez de aceleração cartesiana (que é o que weaponDynamics(), com seu limitador de taxa de giro, já sabe integrar direto).",
      "Contraste com o Missile::weaponGuidance() NATIVO (usado por AamMissile/Sam/etc. quando ninguém sobrescreve): aquele calcula um PONTO DE INTERCEPTAÇÃO -- extrapola a posição do alvo pelo tempo estimado de fechamento e mira nesse ponto futuro -- reagindo só a um snapshot geométrico do instante, nunca à taxa da LOS. Testado (não só em unidade): tests/domain/test_Guidance.cpp::ConvergeContraAlvoEmCruzamento mede um míssil a 280 m/s fechando para menos de 200 m de alcance mínimo contra um alvo cruzando a 60 m/s a 8,5 km.",
    ],
  },
  {
    id: "proximityfuze",
    stage: 4,
    cls: "domain",
    method: "proximityFuze(relPos, relVel, burstRangeM, prev)",
    call: "também chamado pelo passo anterior, no MESMO frame -- a espoleta, função pura",
    src: "domain::proximityFuze",
    hl: [10, 18],
    tag: null,
    flag: null,
    note: [
      "Detecta a TRANSIÇÃO 'aproximando → não mais aproximando' comparando o SINAL da taxa de alcance contra o do frame anterior (prev.wasApproaching) -- é por isso que o estado (FuzeState) tem que sobreviver entre frames: um instante isolado não diz se o alcance ACABOU de parar de diminuir, só um segundo ponto de comparação diz.",
      "hit = (range <= burstRangeM) -- readonly booleano, calculado no MESMO instante da transição, nunca antes nem depois. É esta comparação, e não lethalRange, que decide 'ACERTO' vs. 'FALHA' no próximo passo desta trilha (ver a nota de weaponguidance-hit sobre por que ~35 m ainda conta como acerto contra um burst de 150 m).",
    ],
  },
  {
    id: "weapondynamics",
    stage: 4,
    cls: "GuidedMissile",
    method: "weaponDynamics(dt)",
    call: "chamado logo após weaponGuidance() no MESMO frame -- consome cmdHeadingRad_/cmdPitchRad_/cmdSpeedMps_",
    src: "GuidedMissile::weaponDynamics",
    hl: [12, 18],
    tag: null,
    flag: null,
    note: [
      "maxTurnRateRadPerS vem de maxG (slot herdado de Missile, aqui 15G -- 'míssil vira muito mais apertado que uma aeronave tripulada', comentário do construtor) convertido com base::ETHGM (METROS/s², não base::ETHG em pés/s² -- usar a constante errada daria uma taxa de giro ~3,28× errada; este míssil trabalha em m/s do início ao fim).",
      "setEulerAngles()/setVelocity() são as ÚNICAS escritas deste método -- nenhuma escrita de posição (ver a nota do passo 'dynamics-dispatch' sobre por quê).",
    ],
  },
  {
    id: "weaponguidance-hit",
    stage: 5,
    cls: "GuidedMissile",
    method: "weaponGuidance(dt) -- ramo de acerto",
    call: "MESMO método do estágio anterior -- agora no frame em que a espoleta detecta o ponto de menor aproximação",
    src: "GuidedMissile::weaponGuidance",
    hl: [38, 67],
    tag: "setMode(DETONATED)",
    flag: "gotcha",
    note: [
      "domain::proximityFuze() (função pura, mesmo espírito de proportionalNavigation()) detecta a TRANSIÇÃO \"alcance diminuindo → alcance aumentando\" comparando o sinal da taxa de alcance contra o frame anterior -- por isso precisa de estado (fuzeState_, membro privado do GuidedMissile) entre frames: um único frame não basta para saber se HOUVE transição.",
      "outcome.hit compara contra getMaxBurstRng() (150 m neste cenário), NÃO contra getLethalRange() (30 m) -- README mede ~35 m de aproximação mínima, dentro do burst e portanto 'ACERTO', mesmo estando acima do alcance letal nominal.",
      "GOTCHA: este ramo chama setDetonationResults(DETONATE_ENTITY_IMPACT) + checkDetonationEffect() -- mas NUNCA chama setLocationOfDetonation() antes. Os caminhos NATIVOS de detonação (collisionNotification()/crashNotification() do lado da ARMA, e o Missile::weaponGuidance() nativo) sempre chamam setLocationOfDetonation() logo antes de checkDetonationEffect() -- é essa chamada que calcula detonationRange a partir da posição relativa real. Sem ela, detonationRange fica no valor de inicialização de AbstractWeapon (double detonationRange {}, ou seja 0.0) -- consequência no próximo passo.",
    ],
  },
  {
    id: "checkdetonationeffect",
    stage: 5,
    cls: "AbstractWeapon",
    method: "checkDetonationEffect()",
    call: "herdado sem override -- varre TODOS os players LOCAIS, não só o alvo designado",
    src: "AbstractWeapon::checkDetonationEffect",
    hl: [17, 33],
    tag: null,
    flag: "info",
    note: [
      "NÃO calcula probabilidade de dano nenhuma -- só resolve o alvo (getTargetPlayer(), ou o Player por trás de getTargetTrack() se o alvo tiver sido perdido) e delega TUDO para Player::processDetonation() de cada player elegível, no próximo passo.",
      "'Elegível' = local (para de varrer no primeiro networked, p->isNetworkedPlayer() -- a lista é assumida ordenada com locais primeiro) E (dentro de 10×maxBurstRng OU é o próprio alvo designado). Ou seja: NESTE cenário, a4_shooter também seria verificado se estivesse a menos de 1500 m (10×150m) do ponto de detonação -- não medido neste README, mas o mecanismo não distingue 'quem eu mirei' de 'quem está perto', exceto pelo `|| (p == tgt)` que GARANTE o alvo designado mesmo se ele já estiver longe.",
    ],
  },
  {
    id: "processdetonation",
    stage: 5,
    cls: "Player",
    method: "processDetonation(detRange, wpn) -- do lado do a4_target",
    call: "chamado uma vez por player elegível, dentro do laço do passo anterior",
    src: "Player::processDetonation",
    hl: [12, 27],
    tag: "KILL_EVENT (1304)",
    flag: "gotcha",
    note: [
      "A NUANCE CENTRAL desta trilha inteira: para o alvo DESIGNADO (this == wpn->getTargetPlayer()), rng NÃO é a distância recém-calculada por checkDetonationEffect() -- é sobrescrito por wpn->getDetonationRange(). Como o passo anterior nunca chamou setLocationOfDetonation(), esse valor é 0.0 (o default de inicialização de AbstractWeapon).",
      "0.0 < lethalRange (30 m, o default de GuidedMissile) é SEMPRE verdadeiro -- então o a4_target designado recebe um event(KILL_EVENT, launcher) INCONDICIONAL e IMEDIATO, pulando inteiramente o cálculo probabilístico de dano (o ramo 'Near by?' logo abaixo, que É o caminho que qualquer OUTRO player dentro de 10×burst usaria). Em outras palavras: contra o alvo que o míssil de fato mirou, 'entrar no burst' (150 m) e 'acertar em cheio' (0 m) são, neste código, INDISTINGUÍVEIS -- os dois resultam no mesmo event(KILL_EVENT).",
      "launcher = wpn->getLaunchVehicle() -- é o a4_shooter que aparece como 'quem matou' (P2 do REID_PLAYER_KILLED daqui a dois passos), não o míssil em si.",
    ],
  },
  {
    id: "event-dispatch",
    stage: 5,
    cls: "Player",
    method: "event(KILL_EVENT, launcher) -- despacho nativo",
    call: "macro BEGIN_EVENT_HANDLER(Player) -- a MESMA tabela usada por CRASH_EVENT (terreno) e por qualquer outro token",
    src: "Player::event (KILL_EVENT)",
    hl: [0, 12],
    tag: null,
    flag: "info",
    note: [
      "ON_EVENT_OBJ(KILL_EVENT, killedNotification, Player) casa a sobrecarga com objeto (event(KILL_EVENT, launcher) -- o caso daqui); ON_EVENT(KILL_EVENT, killedNotification) casaria event(KILL_EVENT) ou event(KILL_EVENT, nullptr). As DUAS chamam o MESMO método, Player::killedNotification(Player*) -- é despacho por assinatura, não por token duplicado.",
      "Não existe COLLISION_EVENT nem DETONATION_EVENT como tokens NATIVOS separados (mixr::base::eventTokens.hpp: só KILL_EVENT=1304, CRASH_EVENT=1305, JETTISON_EVENT=1306, entre outros). Quem 'colide em pleno voo' também usa CRASH_EVENT (via mixr::models::CollisionDetect, um system component OPCIONAL com sendCrashEvents default false -- não usado nesta demonstração) -- e quem bate no terreno usa o MESMO CRASH_EVENT, disparado direto por Player::updateTC() quando AGL<0 (ver o passo à parte, mais abaixo). A detonação de ARMA nunca passa por CRASH_EVENT -- só por KILL_EVENT, através de processDetonation().",
    ],
  },
  {
    id: "killednotification",
    stage: 5,
    cls: "Player",
    method: "killedNotification(Player* p)",
    call: "despachado pelo passo anterior -- é AQUI que 'abater' vira (ou não vira) algo visível",
    src: "Player::killedNotification",
    hl: [26, 34],
    tag: "REID_PLAYER_KILLED (47)",
    flag: "gotcha",
    note: [
      "SEMPRE roda (independente de killRemoval): propaga KILL_EVENT para CADA subcomponente do a4_target (sc->event(KILL_EVENT, p) -- Autopilot, JSBSimModel, tudo que estiver em components:), e faz setDamage(1.0); setSmoke(1.0); setFlames(1.0). Nenhum desses três setters tem efeito colateral -- são clamp 0..1 puro sobre um double, sem consumidor nativo que os torne visíveis (fork headless, sem sistema gráfico de fumaça/chama).",
      "GOTCHA -- a linha que decide se o avião 'cai': if (killRemoval && isLocalPlayer()) { setMode(KILLED); ... }. killRemoval é um slot do Player, default false ('If true destroyed players are set to KILLED and are eventually removed (default: false)') -- e o bloco a4_target de sandbox/A4-6DOF-MISSILE NÃO declara killRemoval: true. Resultado medido: mode nunca sai de ACTIVE. O Autopilot do a4_target continua com os três hold modes ligados, o JSBSimModel continua integrando voo normalmente -- o avião simplesmente continua voando reto e nivelado, como se nada tivesse acontecido.",
      "BEGIN_RECORD_DATA_SAMPLE(..., REID_PLAYER_KILLED) grava INCONDICIONALMENTE (fora do if(killRemoval)) -- mas, como REID_WEAPON_RELEASED no passo 'AbstractWeapon::release', o token 47 também não está no enabledList: [ 43 42 ] deste cenário. O evento nativo aconteceu de ponta a ponta -- KILL_EVENT despachado, subcomponentes notificados, damage=1.0 -- só não aparece em lugar NENHUM que o operador da simulação esteja olhando.",
    ],
  },
  {
    id: "aside-crash",
    stage: 5,
    cls: "Player",
    method: "updateTC() -- o caminho que NÃO roda aqui",
    call: "aside: por que um míssil nunca aciona crashNotification()/collisionNotification() do alvo",
    src: "Player::updateTC (AGL<0)",
    hl: [0, 6],
    tag: "CRASH_EVENT (1305)",
    flag: "info",
    note: [
      "crashNotification() só é disparado por ESTA linha -- Player::updateTC() testando getAltitudeAgl() < 0.0 (colisão com o TERRENO), fora de qualquer laço de detonação de arma. collisionNotification(Player*) só é disparado por mixr::models::CollisionDetect (varredura par-a-par de distância entre players em pleno voo, sendCrashEvents default false) -- também sem relação com míssil.",
      "Não há PONTE nativa entre 'um míssil detonou perto de mim' e 'eu colidi'/'eu bati no chão': são três funções (killedNotification/collisionNotification/crashNotification) com três gatilhos totalmente distintos, e a única que a cadeia de detonação de arma aciona é a primeira. Se um dia se quisesse que um abate por míssil derrubasse o alvo do MESMO jeito visual que um impacto no terreno, isso exigiria código NOVO -- o framework não faz essa ligação sozinho.",
    ],
  },
  {
    id: "linger",
    stage: 6,
    cls: "GuidedMissile",
    method: "updateTC(dt) -- timer pós-detonação",
    call: "roda todo frame depois de DETONATED, até completar kLingerSec (2.0 s)",
    src: "GuidedMissile::updateTC",
    hl: [9, 19],
    tag: "setMode(DELETE_REQUEST)",
    flag: null,
    note: [
      "Nada NATIVO tira um Missile de DETONATED para DELETE_REQUEST -- confirmado lendo AbstractWeapon::updateTOF() e Missile::weaponGuidance(): os dois só chamam setMode(DETONATED), nunca DELETE_REQUEST. Sem este timer, o míssil detonado ficaria PARA SEMPRE na lista de players e no Tacview.",
      "É este timer, não a detonação em si, que faz o objeto W-alguma-coisa sumir da gravação (~2 s depois, medido em sandbox/A4-6DOF-MISSILE: detonação e remoção ~t=91.8s / ~t=93.9s). Do ponto de vista de quem só olha o Tacview, o SUMIÇO do míssil é o único sinal visível de que algo aconteceu -- o a4_target continua lá, voando, sem nenhuma marca.",
    ],
  },
  {
    id: "updatetof-timeout",
    stage: 6,
    cls: "AbstractWeapon",
    method: "updateTOF(dt) -- aside: e se o míssil nunca acertasse?",
    call: "chamado toda fase 3 por AbstractWeapon::updateTC() (ver o passo phase0-transition) enquanto isMode(ACTIVE)",
    src: "AbstractWeapon::updateTOF",
    hl: [12, 20],
    tag: "REID_WEAPON_DETONATION (63)",
    flag: "info",
    note: [
      "Contraste com o desfecho principal desta trilha: se domain::proximityFuze() NUNCA detectasse a transição de aproximação (alvo evadindo com sucesso, guiagem perdendo o alvo, etc.), o míssil não ficaria voando para sempre -- updateTOF() força DETONATED assim que getTOF() >= getMaxTOF() (60 s neste cenário), incondicionalmente.",
      "GOTCHA confirmado lendo o fonte: este caminho NUNCA chama checkDetonationEffect() -- nem aqui, nem em lugar nenhum chamado a partir daqui. Um míssil que 'morre de velhice' por fim de tempo de voo grava REID_WEAPON_DETONATION (o mesmo token do acerto, só o DetonationResults muda para DETONATE_DETONATION) mas NUNCA aciona Player::processDetonation() em ninguém -- sem risco de dano, mesmo que por coincidência geométrica o míssil esteja fisicamente perto de algum player no instante do timeout.",
      "GuidedMissile::updateTC() (o timer de remoção pós-DETONATED, passo anterior desta trilha) trata os dois casos IGUAL: tanto o DETONATED por acerto/falha da espoleta quanto o DETONATED por timeout de TOF entram no mesmo linger de 2 s antes de DELETE_REQUEST -- do ponto de vista da limpeza da lista de players, não importa qual dos dois caminhos chegou lá.",
    ],
  },
  {
    id: "synthesis",
    stage: 6,
    cls: null,
    method: null,
    title: "o que esta trilha prova",
    call: "síntese -- o que esta trilha prova, linha a linha",
    src: null,
    hl: null,
    tag: null,
    flag: "gotcha",
    note: [
      "O \"abate\" aconteceu por inteiro no nível de EVENTO: KILL_EVENT foi emitido, despachado pela tabela nativa de Player, propagado a cada subcomponente do a4_target, e REID_PLAYER_KILLED foi gravado no recorder -- tudo isso são chamadas de verdade, não simulado por este texto. O que NÃO aconteceu foi visível: mode nunca saiu de ACTIVE (falta killRemoval: true no .edl do alvo) e os dois REID relevantes (WEAPON_RELEASED=61, PLAYER_KILLED=47) nunca chegaram ao Tacview (fora do enabledList do cenário).",
      "As duas lacunas são CONFIGURAÇÃO, não bug: killRemoval é um slot público de Player, e enabledList aceita qualquer lista de tokens. sandbox/A4-6DOF-MISSILE deliberadamente não liga nenhum dos dois -- o próprio objetivo daquele cenário é isolar disparo→guiagem→detonação, e o README já é honesto sobre 'nem o alvo mostra dano visível'. Esta trilha existe para mostrar EXATAMENTE onde, no C++ nativo, essa escolha se materializa.",
      "Um terceiro gotcha, independente dos dois acima e não configurável por EDL: o alvo DESIGNADO sempre recebe KILL_EVENT garantido (rng=0.0 via getDetonationRange() nunca setado), não uma avaliação real de distância -- corrigível só com código (GuidedMissile chamando setLocationOfDetonation() antes de checkDetonationEffect(), como os caminhos nativos já fazem).",
    ],
  },
];
/* ---------------------------- step-by-step (missil) ----------------------------- */

function MissileTrace({ onOpenCatalog }) {
  const [idx, setIdx] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [speed, setSpeed] = useState(2200);

  const step = MISSILE_TRACE[idx];
  const snip = missileSnip(step.src);
  const win = useMemo(() => (snip ? windowLines(snip.lines, step.hl, 24) : null), [snip, step.hl]);
  // snip.lang === "edl" pula o tokenizer C++ -- o heuristico e' calibrado pra
  // C++ e coloriria sintaxe EDL (parenteses de classe, slots) errado.
  const cppTokens = useMemo(() => (snip && snip.lang !== "edl" ? cppTokenizeLines(snip.lines) : null), [snip]);

  const move = useCallback((d) => {
    setPlaying(false);
    setIdx((p) => Math.max(0, Math.min(MISSILE_TRACE.length - 1, p + d)));
  }, []);

  useEffect(() => {
    if (!playing) return;
    const t = setTimeout(() => {
      setIdx((p) => (p + 1 >= MISSILE_TRACE.length ? (setPlaying(false), p) : p + 1));
    }, speed);
    return () => clearTimeout(t);
  }, [playing, idx, speed]);

  useEffect(() => {
    const h = (e) => {
      if (e.target.tagName === "INPUT" || e.target.tagName === "SELECT") return;
      if (e.key === "ArrowRight") move(1);
      else if (e.key === "ArrowLeft") move(-1);
    };
    window.addEventListener("keydown", h);
    return () => window.removeEventListener("keydown", h);
  }, [move]);

  const flagColor = (f) => (f === "gotcha" ? "var(--rf)" : f === "info" ? "var(--bgc)" : "var(--muted)");
  const inCatalog = step.cls ? !!MODEL[step.cls] : false;

  return (
    <>
      <div className="mx-body" style={{ paddingBottom: 110 }}>
        <p style={{ fontSize: 12.5, color: "var(--muted)", maxWidth: 900, marginBottom: 12 }}>
          Do <b style={{ color: "var(--ink)" }}>tick()</b> que decide disparar até o <b style={{ color: "var(--ink)" }}>event(KILL_EVENT, ...)</b> do outro lado --
          cada passo é código de verdade (modelo A-4/míssil ou fonte nativo do MIXR, <code className="mx-mono">contexts/src/mixr/</code>), na ordem em que roda,
          com o instante exato em que um evento nativo é (ou não é) emitido. Cenário de referência: <code className="mx-mono">sandbox/A4-6DOF-MISSILE</code>.
        </p>

        <div style={{ display: "flex", gap: 6, flexWrap: "wrap", marginBottom: 12 }}>
          {MISSILE_STAGES.map((s) => {
            const on = step.stage === s.n;
            const done = step.stage > s.n;
            return (
              <div key={s.n} style={{
                padding: "3px 10px", borderRadius: 2, fontSize: 11.5, letterSpacing: "0.02em",
                background: on ? "var(--ink)" : "var(--panel)",
                color: on ? "var(--paper)" : "var(--muted)",
                opacity: done || on ? 1 : 0.55,
              }}>
                {s.n + 1}. {s.label}
              </div>
            );
          })}
        </div>

        <div style={{ display: "flex", gap: 16, alignItems: "flex-start", flexWrap: "wrap" }}>
          {/* -------- coluna 1: lista de passos, clicavel -------- */}
          <div style={{ width: 300, flexShrink: 0, border: "1px solid var(--rule)", borderRadius: 3, maxHeight: 620, overflowY: "auto" }}>
            {MISSILE_TRACE.map((s, k) => {
              const active = k === idx;
              return (
                <div key={s.id} onClick={() => { setPlaying(false); setIdx(k); }}
                  style={{
                    padding: "7px 10px", cursor: "pointer", borderBottom: "1px solid var(--rule)",
                    background: active ? "var(--active-bg)" : "transparent",
                    borderLeft: `3px solid ${active ? "var(--hot)" : s.flag ? flagColor(s.flag) : "transparent"}`,
                  }}>
                  <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)" }}>{k + 1}/{MISSILE_TRACE.length} · {MISSILE_STAGES[s.stage].label}</div>
                  <div style={{ fontSize: 12.5, fontWeight: active ? 700 : 500, color: "var(--ink)" }}>
                    {s.cls ? <span className="mx-mono">{s.cls}</span> : s.title}{s.method ? <span style={{ color: "var(--muted)" }}> :: {s.method}</span> : ""}
                  </div>
                  {s.tag && (
                    <span className="mx-chip" style={{ marginTop: 3, borderColor: flagColor(s.flag), color: flagColor(s.flag) }}>{s.tag}</span>
                  )}
                </div>
              );
            })}
          </div>

          {/* -------- coluna 2: detalhe do passo ativo -------- */}
          <div style={{ flex: 1, minWidth: 340 }}>
            <div className="mx-lbl">
              <span>Passo {idx + 1} de {MISSILE_TRACE.length}</span>
              <span>{step.call}</span>
            </div>
            <div style={{ display: "flex", alignItems: "baseline", gap: 8, flexWrap: "wrap", marginBottom: 8 }}>
              <span className="mx-mono mx-step-title" style={{ fontSize: 15, fontWeight: 700 }}>
                {step.cls ? `${step.cls}::${step.method}` : step.title}
              </span>
              {step.tag && (
                <span className="mx-chip" style={{ borderColor: flagColor(step.flag), color: flagColor(step.flag), fontSize: 11.5 }}>{step.tag}</span>
              )}
              {inCatalog && onOpenCatalog && (
                <button className="mx-btn" style={{ fontSize: 11 }} onClick={() => onOpenCatalog(step.cls)}>Ver classe completa no Catálogo →</button>
              )}
            </div>

            {win && (
              <div style={{ marginBottom: 10 }}>
                <div className="mx-lbl">
                  <span className="mx-mono">{snip.file}</span>
                  <span>{snip.lang === "edl" ? "EDL (cenário)" : snip.trunc ? "excerto -- ver arquivo completo" : "C++"}</span>
                </div>
                <div className="mx-code">
                  {win.cutBefore && <div className="mx-codecut">⋯ {win.offset} linha{win.offset === 1 ? "" : "s"} acima ⋯</div>}
                  {win.lines.map((ln, k) => {
                    const abs = k + win.offset;
                    const on = step.hl && abs >= step.hl[0] && abs <= step.hl[1];
                    return <div key={abs} className="mx-cl" data-on={on ? 1 : 0}><span className="mx-num">{snip.line + abs}</span><span className="mx-src">{renderCppSrc(cppTokens && cppTokens[abs], ln)}</span></div>;
                  })}
                  {win.cutAfter && <div className="mx-codecut">⋯ {snip.lines.length - win.offset - win.lines.length} linhas abaixo ⋯</div>}
                </div>
              </div>
            )}

            {step.note.map((p, k) => (
              <p key={k} className={step.flag === "gotcha" && k === step.note.length - 1 ? "mx-warn" : ""}
                 style={{ fontSize: 12.5, lineHeight: 1.55, color: step.flag === "gotcha" && k === step.note.length - 1 ? undefined : "var(--ink)", marginBottom: 8 }}>
                {p}
              </p>
            ))}
          </div>
        </div>
      </div>

      <div className="mx-transport">
        <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>{playing ? "Pausar" : "Reproduzir"}</button>
        <button className="mx-btn" onClick={() => move(-1)}>←</button>
        <button className="mx-btn" onClick={() => move(1)}>→</button>
        <button className="mx-btn" onClick={() => { setPlaying(false); setIdx(0); }}>Início</button>
        <div className="mx-tl" role="slider" aria-label="Linha do tempo" aria-valuenow={idx} aria-valuemin={0} aria-valuemax={MISSILE_TRACE.length - 1} tabIndex={0}
             onKeyDown={(e) => { if (e.key === "ArrowRight") move(1); if (e.key === "ArrowLeft") move(-1); }}>
          {MISSILE_TRACE.map((s, k) => (
            <div key={k} onClick={() => { setPlaying(false); setIdx(k); }} title={`${s.cls || s.title} :: ${s.method || ""}`}
              style={{ background: k === idx ? "var(--hot)" : s.flag ? flagColor(s.flag) : "var(--rule)", height: k === idx ? "100%" : "45%", opacity: k <= idx ? 1 : 0.4 }} />
          ))}
        </div>
        <span className="mx-mono" style={{ fontSize: 11.5, color: "var(--muted)", minWidth: 52 }}>{idx + 1}/{MISSILE_TRACE.length}</span>
        <select className="mx-input" value={speed} onChange={(e) => setSpeed(Number(e.target.value))} aria-label="Velocidade">
          <option value={3200}>Lento</option><option value={2200}>Normal</option><option value={1100}>Rápido</option>
        </select>
      </div>
    </>
  );
}

/* ====================== aba "Referência" -- enciclopédia de classes built-in =======
 * Diferença para o Catálogo: o Catálogo é FLAT e automático (as 225 classes nativas,
 * slots/herança extraídos por tools/generate_manual_catalog.py, sem opinião nenhuma
 * sobre qual classe merece mais espaço). Referência é CURADA e PROFUNDA -- poucas
 * classes, escolhidas a dedo, cada uma com recursos didáticos que o Catálogo não
 * tenta oferecer (animação, gráfico, diagrama de estados). Começa com uma única
 * classe -- Missile -- e nasce desenhada para crescer (REF_CLASSES é uma lista, o
 * layout já tem barra lateral).
 *
 * A animação de guiagem NÃO é enfeite: ela roda a MESMA fórmula de
 * Missile::weaponGuidance()/weaponDynamics() (o par de hooks que
 * AbstractWeapon::dynamics() chama quando getDynamicsModel()==nullptr -- ver a aba
 * step-by-step), reimplementada em JS puro sobre um alvo em linha reta. Não é uma
 * simulação de física livre: é a tradução literal do C++ (mesmas fórmulas, mesmas
 * constantes nativas), pré-computada quadro a quadro e reproduzida com os mesmos
 * controles de transporte do resto do app.
 * ==================================================================================== */

const REF_CLASSES = [
  { key: "Missile", label: "Missile", sub: "míssil ar-ar genérico -- guiagem por ponto de interceptação" },
  { key: "Steerpoint", label: "Steerpoint / Route", sub: "waypoint + o sequenciador que anda entre eles" },
  { key: "Navigation", label: "Navigation", sub: "agregador de dados de navegação -- Ins/Gps são a mesma classe" },
  { key: "Autopilot", label: "Autopilot", sub: "piloto automático nativo -- navMode e os limites que não limitam" },
  { key: "Player", label: "Player", sub: "base de todo player -- despacho de fase, os 10 papéis, local vs. rede" },
  { key: "System", label: "System", sub: "base de todo subsistema anexado a um player -- ownship e o freeze em cascata" },
  { key: "Gimbal", label: "Gimbal / Antenna", sub: "apontamento -- Gimbal, ScanGimbal, StabilizingGimbal, Antenna" },
  { key: "RfSensor", label: "RfSensor / Radar / Rwr / Sar / Jammer", sub: "detecção RF -- RfSystem, RfSensor e as 5 subclasses" },
  { key: "TrackManager", label: "TrackManager", sub: "o que acontece com uma detecção -- 6 gerenciadores de pista + Track" },
  { key: "RfSignature", label: "RfSignature", sub: "assinatura de RCS -- 7 formas + o ciclo Emission completo" },
];

const REF_MISSILE_CONST = {
  VP_MAX: 800,        // m/s -- Missile::Missile(), setVpMax(800.0f)
  MAX_G: 4.0,         // g's -- setMaxG(4.0)
  MAX_ACCEL: 50.0,    // m/s/s -- setMaxAccel(50.0)
  G_FTS2: 32.16,      // base::ETHG -- PÉS/s^2, usado LITERAL (ver nota didática)
  MAX_BURST_RNG: 150, // m -- setMaxBurstRng(150.0f)
  LETHAL_RANGE: 30,   // m -- setLethalRange(30.0f)
  TSG: 1.0,           // s -- setTSG(1.0)
  MAX_TOF: 60,        // s -- setMaxTOF(60.0)
  SEPARATION_M: 5000, // geometria do DEMO -- não é slot nenhum do MIXR
  DT: 0.1,
  SIM_CEILING_S: 25,
  LINGER_S: 1.2,
};

function refWrapPi(a) {
  let x = a % (2 * Math.PI);
  if (x > Math.PI) x -= 2 * Math.PI;
  if (x < -Math.PI) x += 2 * Math.PI;
  return x;
}
function refWrap2pi(a) {
  let x = a % (2 * Math.PI);
  if (x < 0) x += 2 * Math.PI;
  return x;
}
function refClamp(x, lo, hi) { return Math.max(lo, Math.min(hi, x)); }

/* ------------------------------------------------------------------------------
 * simulateNativeMissileIntercept() -- reimplementação LITERAL, em JS puro, de
 * Missile::weaponGuidance()/weaponDynamics() (contexts/src/mixr/src/models/player/
 * weapon/Missile.cpp) sobre um alvo em linha reta a velocidade constante. 2D
 * (plano N/E) -- a formula 3D nativa usa p1.z()/grng pra cmdPitch, mas com os dois
 * players na MESMA altitude (z=0 o tempo todo) isso já sai 0 sozinho, então nada
 * foi simplificado ou omitido: é o MESMO cálculo, só que com uma entrada que nunca
 * ativa o eixo vertical.
 *
 * NÃO modela track (getTargetTrack()) -- só Player direto (getTargetPlayer()),
 * o caminho mais comum. Sem track, o bloco de "fuzing logic" nativo usa a MESMA
 * geometria já calculada no bloco de guiagem (não há posição de track separada
 * pra recalcular) -- é exatamente esse caminho que este simulador reproduz.
 * ------------------------------------------------------------------------------ */
function simulateNativeMissileIntercept(targetBearingDeg, targetSpeedMps) {
  const C = REF_MISSILE_CONST;
  const bearingRad = (targetBearingDeg * Math.PI) / 180;
  const tVelN = targetSpeedMps * Math.cos(bearingRad);
  const tVelE = targetSpeedMps * Math.sin(bearingRad);
  const tN0 = C.SEPARATION_M;
  const tE0 = 0;

  let mN = 0, mE = 0;
  let mHeading = Math.atan2(tE0 - mE, tN0 - mN); // atReleaseInit(): comeca apontado pro alvo inicial
  let mSpeed = C.VP_MAX;
  let prevRa = 0;
  let trngPrev = null, trdotPrev = null;
  let detonated = false, hit = null, detFrameIndex = null;
  let cmdHeading = mHeading;

  const frames = [];
  let tof = 0;
  while (tof <= C.SIM_CEILING_S) {
    const tN = tN0 + tVelN * tof;
    const tE = tE0 + tVelE * tof;
    const losN = tN - mN, losE = tE - mE;
    const trng = Math.hypot(losN, losE);
    const trdot = trngPrev === null ? 0 : (trng - trngPrev) / C.DT;

    let justDetonated = false;
    if (!detonated && tof > 2.0 && trdotPrev !== null && trdotPrev < 0 && trdot > 0) {
      const mVelN = mSpeed * Math.cos(mHeading), mVelE = mSpeed * Math.sin(mHeading);
      const velRelN = tVelN - mVelN, velRelE = tVelE - mVelE;
      const vm2 = velRelN * velRelN + velRelE * velRelE;
      let r2 = Infinity;
      if (vm2 > 0) {
        const rdv = losN * velRelN + losE * velRelE;
        const ndt = -rdv / vm2;
        const p0N = losN + velRelN * ndt, p0E = losE + velRelE * ndt;
        r2 = p0N * p0N + p0E * p0E;
      }
      detonated = true;
      hit = r2 <= C.MAX_BURST_RNG * C.MAX_BURST_RNG;
      justDetonated = true;
    }

    const totalVel = targetSpeedMps;
    const vtplos = trng > 0 ? (losN * tVelN + losE * tVelE) / trng : 0;
    const guidanceEnabled = !detonated && tof >= C.TSG && trng > 0;
    if (guidanceEnabled) {
      let v = C.VP_MAX;
      if (v < totalVel) v = totalVel + 1;
      const vtnlos2 = Math.max(0, totalVel * totalVel - vtplos * vtplos);
      const vmplos = Math.sqrt(Math.max(0, v * v - vtnlos2));
      const vclos = vmplos - vtplos;
      const dt1 = vclos > 0 ? trng / vclos : 0;
      const p1N = losN + tVelN * dt1, p1E = losE + tVelE * dt1;
      cmdHeading = Math.atan2(p1E, p1N);
    }

    frames.push({
      t: tof, mN, mE, mHeadingDeg: (refWrap2pi(mHeading) * 180) / Math.PI, mSpeed,
      tN, tE, range: trng, trdot,
      cmdHeadingDeg: (refWrap2pi(cmdHeading) * 180) / Math.PI, guidanceEnabled,
      detonated, hit, justDetonated,
    });
    if (justDetonated) detFrameIndex = frames.length - 1;
    if (detonated && tof - frames[detFrameIndex].t >= C.LINGER_S) break;

    if (!detonated) {
      const raMax = (C.MAX_G * C.G_FTS2) / Math.max(mSpeed, 1);
      let ra = refWrapPi(cmdHeading - mHeading);
      ra = refClamp(ra, -raMax, raMax);
      const newHeading = refWrap2pi(mHeading + ((ra + prevRa) / 2) * C.DT);

      const cmdSpeed = C.VP_MAX; // sobt=0 / eobt=60 -- motor "queimando" a janela toda deste demo
      const vpdot = refClamp(cmdSpeed - mSpeed, -C.MAX_ACCEL, C.MAX_ACCEL);
      const newSpeed = mSpeed + vpdot * C.DT;

      // Integração de POSIÇÃO fica fora de weaponDynamics() no C++ real -- é
      // Player::dynamics() (via positionUpdate()) quem faz isso, logo depois,
      // no MESMO frame. Reproduzido aqui como um Euler simples.
      const velN = newSpeed * Math.cos(newHeading), velE = newSpeed * Math.sin(newHeading);
      mN += velN * C.DT;
      mE += velE * C.DT;
      mHeading = newHeading;
      mSpeed = newSpeed;
      prevRa = ra;
    }

    trngPrev = trng;
    trdotPrev = trdot;
    tof += C.DT;
  }

  return { frames, hit, detFrameIndex };
}

function refFmt(n, d) { return Number.isFinite(n) ? n.toFixed(d != null ? d : 0) : "--"; }

function MissileStat({ label, value }) {
  return (
    <div className="mx-stat">
      <div className="mx-stat-label">{label}</div>
      <div className="mx-stat-value">{value}</div>
    </div>
  );
}

function MissileGuidanceLab({ onModeChange }) {
  const [bearing, setBearing] = useState(200);
  const [speed, setSpeed] = useState(230);
  const [idx, setIdx] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [playSpeed, setPlaySpeed] = useState(60);

  const sim = useMemo(() => simulateNativeMissileIntercept(bearing, speed), [bearing, speed]);
  const frames = sim.frames;
  const clampedIdx = Math.min(idx, frames.length - 1);
  const frame = frames[clampedIdx] || frames[0];

  useEffect(() => { setIdx(0); setPlaying(false); }, [bearing, speed]);
  useEffect(() => {
    if (!playing) return;
    const t = setTimeout(() => {
      setIdx((p) => (p + 1 >= frames.length ? (setPlaying(false), p) : p + 1));
    }, playSpeed);
    return () => clearTimeout(t);
  }, [playing, idx, playSpeed, frames.length]);
  useEffect(() => { if (onModeChange) onModeChange(frame.detonated ? "DETONATED" : "ACTIVE"); }, [frame.detonated, onModeChange]);

  // viewBox auto-ajustado ao envelope INTEIRO da trajetoria -- nunca sai da tela,
  // qualquer que seja o rumo/velocidade do alvo escolhidos.
  const bounds = useMemo(() => {
    let minN = 0, maxN = 0, minE = 0, maxE = 0;
    frames.forEach((f) => {
      minN = Math.min(minN, f.mN, f.tN); maxN = Math.max(maxN, f.mN, f.tN);
      minE = Math.min(minE, f.mE, f.tE); maxE = Math.max(maxE, f.mE, f.tE);
    });
    const pad = Math.max(300, (maxN - minN) * 0.12, (maxE - minE) * 0.12);
    return { minN: minN - pad, maxN: maxN + pad, minE: minE - pad, maxE: maxE + pad };
  }, [frames]);

  const W = 520, H = 380;
  const spanN = Math.max(1, bounds.maxN - bounds.minN);
  const spanE = Math.max(1, bounds.maxE - bounds.minE);
  const scale = Math.min((W - 20) / spanE, (H - 20) / spanN);
  const px = (e) => (e - bounds.minE) * scale + (W - spanE * scale) / 2;
  const py = (n) => H - ((n - bounds.minN) * scale + (H - spanN * scale) / 2);

  const trailUpTo = (getN, getE) => frames.slice(0, clampedIdx + 1).map((f) => `${px(getE(f))},${py(getN(f))}`).join(" ");
  const missileTrail = trailUpTo((f) => f.mN, (f) => f.mE);
  const targetTrail = trailUpTo((f) => f.tN, (f) => f.tE);

  const detFrame = sim.detFrameIndex != null ? frames[sim.detFrameIndex] : null;
  const showBurst = detFrame && clampedIdx >= sim.detFrameIndex;

  const move = useCallback((d) => { setPlaying(false); setIdx((p) => refClamp(p + d, 0, frames.length - 1)); }, [frames.length]);

  const maxRange = Math.max(...frames.map((f) => f.range), 1);
  const chartW = 520, chartH = 100, chartPad = 30;
  const lastT = Math.max(frames[frames.length - 1].t, 0.001);
  const chartX = (t) => chartPad + (t / lastT) * (chartW - chartPad - 8);
  const chartY = (r) => chartH - 16 - (r / maxRange) * (chartH - 28);

  const status = !frame.detonated
    ? { text: "EM VOO", bg: "var(--panel)", fg: "var(--muted)" }
    : frame.hit
      ? { text: "ACERTO", bg: "var(--ok)", fg: "var(--paper)" }
      : { text: "FALHA -- passou do alvo", bg: "var(--rf)", fg: "var(--paper)" };

  return (
    <div className="mx-card">
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: 8, marginBottom: 10 }}>
        <span style={{ fontSize: 13, fontWeight: 700 }}>Laboratório de guiagem</span>
        <span className="mx-pill" style={{ background: status.bg, color: status.fg }}>● {status.text}</span>
      </div>

      <div style={{ display: "flex", gap: 18, flexWrap: "wrap", alignItems: "flex-start" }}>
        {/* -------- coluna principal: animacao + leitura + grafico -------- */}
        <div style={{ flex: "1 1 540px", minWidth: 320 }}>
          <svg viewBox={`0 0 ${W} ${H}`} style={{ width: "100%", display: "block", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
            <polyline points={targetTrail} fill="none" stroke="var(--rf)" strokeWidth="1.4" opacity="0.55" />
            <polyline points={missileTrail} fill="none" stroke="var(--hot)" strokeWidth="1.6" opacity="0.8" />
            <line x1={px(frame.mE)} y1={py(frame.mN)} x2={px(frame.tE)} y2={py(frame.tN)} stroke="var(--muted)" strokeWidth="1" strokeDasharray="3 3" />
            <g transform={`translate(${px(frame.tE)},${py(frame.tN)})`}>
              <rect x="-5" y="-5" width="10" height="10" transform="rotate(45)" fill="var(--rf)" />
            </g>
            <g transform={`translate(${px(frame.mE)},${py(frame.mN)}) rotate(${frame.mHeadingDeg})`}>
              <polygon points="0,-8 5,7 -5,7" fill="var(--hot)" />
            </g>
            {showBurst && (
              <g transform={`translate(${px(detFrame.tE)},${py(detFrame.tN)})`} opacity="0.9">
                <circle r="10" fill="none" stroke={detFrame.hit ? "var(--ok)" : "var(--rf)"} strokeWidth="2.4" />
                <circle r="3" fill={detFrame.hit ? "var(--ok)" : "var(--rf)"} />
              </g>
            )}
          </svg>
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", fontSize: 11, color: "var(--muted)", margin: "5px 0 12px" }}>
            <span><span style={{ color: "var(--hot)" }}>▲</span> míssil</span>
            <span><span style={{ color: "var(--rf)" }}>◆</span> alvo</span>
            <span style={{ marginLeft: "auto" }}>Missile::weaponGuidance()/weaponDynamics() -- ver Código-fonte</span>
          </div>

          <div style={{ display: "flex", gap: 8, alignItems: "center", flexWrap: "wrap", marginBottom: 10 }}>
            <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>{playing ? "Pausar" : "Reproduzir"}</button>
            <button className="mx-btn" onClick={() => move(-1)}>←</button>
            <button className="mx-btn" onClick={() => move(1)}>→</button>
            <button className="mx-btn" onClick={() => { setPlaying(false); setIdx(0); }}>Início</button>
            <input type="range" min={0} max={frames.length - 1} value={clampedIdx}
                   onChange={(e) => { setPlaying(false); setIdx(Number(e.target.value)); }}
                   style={{ flex: 1, minWidth: 120 }} />
            <span className="mx-mono" style={{ fontSize: 11, color: "var(--muted)", minWidth: 82, textAlign: "right" }}>{refFmt(frame.t, 1)}s ({clampedIdx + 1}/{frames.length})</span>
          </div>

          <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginBottom: 14 }}>
            <MissileStat label="alcance" value={`${refFmt(frame.range)} m`} />
            <MissileStat label="taxa (trdot)" value={`${refFmt(frame.trdot)} m/s`} />
            <MissileStat label="veloc. míssil" value={`${refFmt(frame.mSpeed)} m/s`} />
            <MissileStat label="rumo atual" value={`${refFmt(frame.mHeadingDeg)}°`} />
            <MissileStat label="rumo comandado" value={frame.guidanceEnabled ? `${refFmt(frame.cmdHeadingDeg)}°` : "tof < tsg"} />
          </div>

          <div className="mx-lbl">
            <span>Alcance × tempo -- a detonação é onde trdot cruza de negativo pra positivo</span>
            <span>maxBurstRng = {REF_MISSILE_CONST.MAX_BURST_RNG} m</span>
          </div>
          <svg viewBox={`0 0 ${chartW} ${chartH}`} style={{ width: "100%", display: "block", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
            <line x1={chartPad} y1={chartH - 16} x2={chartW - 6} y2={chartH - 16} stroke="var(--rule)" />
            <polyline fill="none" stroke="var(--bgc)" strokeWidth="1.6" points={frames.map((f) => `${chartX(f.t)},${chartY(f.range)}`).join(" ")} />
            {sim.detFrameIndex != null && (
              <circle cx={chartX(frames[sim.detFrameIndex].t)} cy={chartY(frames[sim.detFrameIndex].range)} r="3.4"
                      fill={frames[sim.detFrameIndex].hit ? "var(--ok)" : "var(--rf)"} />
            )}
            <line x1={chartX(frame.t)} y1="4" x2={chartX(frame.t)} y2={chartH - 16} stroke="var(--hot)" strokeWidth="1" opacity="0.6" />
          </svg>
        </div>

        {/* -------- coluna lateral: controles + ciclo de vida ao vivo -------- */}
        <div style={{ flex: "0 1 240px", minWidth: 210 }}>
          <div className="mx-lbl"><span>Geometria do alvo</span></div>
          <label style={{ display: "block", fontSize: 11.5, marginBottom: 10 }}>
            <div style={{ display: "flex", justifyContent: "space-between" }}><span>rumo</span><span className="mx-mono">{bearing}°</span></div>
            <input type="range" min={0} max={359} value={bearing} onChange={(e) => setBearing(Number(e.target.value))} style={{ width: "100%" }} />
          </label>
          <label style={{ display: "block", fontSize: 11.5, marginBottom: 12 }}>
            <div style={{ display: "flex", justifyContent: "space-between" }}><span>velocidade</span><span className="mx-mono">{speed} m/s ({refFmt(speed * 1.94384, 0)} kt)</span></div>
            <input type="range" min={50} max={420} step={10} value={speed} onChange={(e) => setSpeed(Number(e.target.value))} style={{ width: "100%" }} />
          </label>
          <select className="mx-input" value={playSpeed} onChange={(e) => setPlaySpeed(Number(e.target.value))} style={{ width: "100%", marginBottom: 14 }}>
            <option value={140}>Reprodução lenta</option>
            <option value={60}>Reprodução normal</option>
            <option value={25}>Reprodução rápida</option>
          </select>

          <div className="mx-lbl"><span>Ciclo de vida -- ao vivo</span></div>
          <MissileLifecycleDiagram mode={frame.detonated ? "DETONATED" : "ACTIVE"} compact />

          <p style={{ fontSize: 11, lineHeight: 1.5, color: "var(--muted)", marginTop: 10 }}>
            O míssil sai da origem apontado para a posição INICIAL do alvo (5000 m) -- imitando
            <code className="mx-mono"> atReleaseInit()</code>. Nos primeiros {REF_MISSILE_CONST.TSG.toFixed(1)} s
            (tsg) ele mantém esse rumo fixo; a guiagem só liga depois.
          </p>
        </div>
      </div>
    </div>
  );
}

function MissileLifecycleDiagram({ mode, compact }) {
  const boxes = compact
    ? [
        { id: "ACTIVE", x: 8, y: 8, w: 90, h: 36 },
        { id: "DETONATED", x: 112, y: 8, w: 100, h: 36 },
      ]
    : [
        { id: "INACTIVE", x: 10, y: 10, w: 92, h: 40, note: "nunca lançado" },
        { id: "PRE_RELEASE", x: 132, y: 10, w: 110, h: 40, note: "clone recém-criado" },
        { id: "ACTIVE", x: 272, y: 10, w: 84, h: 40, note: "voando, guiando" },
        { id: "DETONATED", x: 386, y: 10, w: 104, h: 40, note: "acertou OU errou" },
      ];
  const activeIdx = boxes.findIndex((b) => b.id === mode);
  const vb = compact ? "0 0 224 52" : "0 0 620 96";
  return (
    <svg viewBox={vb} style={{ width: "100%", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
      {boxes.map((b, i) => (
        <g key={b.id}>
          <rect x={b.x} y={b.y} width={b.w} height={b.h} rx="3"
                fill={i === activeIdx ? "var(--hot)" : "var(--panel)"}
                stroke={i === activeIdx ? "var(--hot)" : "var(--rule)"} strokeWidth="1.4" />
          <text x={b.x + b.w / 2} y={compact ? b.y + 22 : b.y + 17} textAnchor="middle" className="mx-mono"
                style={{ fontSize: compact ? 10 : 10.5, fontWeight: 700, fill: i === activeIdx ? "var(--paper)" : "var(--ink)" }}>{b.id}</text>
          {!compact && (
            <text x={b.x + b.w / 2} y={b.y + 31} textAnchor="middle"
                  style={{ fontSize: 8.6, fill: i === activeIdx ? "var(--running-fg)" : "var(--muted)" }}>{b.note}</text>
          )}
        </g>
      ))}
      {!compact && (
        <>
          <g transform="translate(508, 10)">
            <rect x="0" y="0" width="108" height="40" rx="3" fill="none" stroke="var(--rf)" strokeWidth="1.2" strokeDasharray="4 3" />
            <text x="54" y="17" textAnchor="middle" className="mx-mono" style={{ fontSize: 10.5, fontWeight: 700, fill: "var(--rf)" }}>DELETE_REQUEST</text>
            <text x="54" y="31" textAnchor="middle" style={{ fontSize: 8.2, fill: "var(--rf)" }}>não nativo (ver Visão geral)</text>
          </g>
          {[["release()", 0, 1], ["AbstractWeapon::updateTC() fase 0", 1, 2], ["weaponGuidance() -- hit/miss/timeout", 2, 3]].map(([label, a, b], i) => {
            const bf = boxes[a], bt = boxes[b];
            const x1 = bf.x + bf.w, x2 = bt.x;
            return (
              <g key={i}>
                <line x1={x1} y1="30" x2={x2 - 2} y2="30" stroke="var(--muted)" strokeWidth="1.2" markerEnd="url(#refArrow)" />
                <text x={(x1 + x2) / 2} y="25" textAnchor="middle" className="mx-mono" style={{ fontSize: 7.6, fill: "var(--muted)" }}>{label}</text>
              </g>
            );
          })}
          <line x1="490" y1="30" x2="506" y2="30" stroke="var(--rf)" strokeWidth="1.2" strokeDasharray="2 2" markerEnd="url(#refArrowDashed)" />
          <defs>
            <marker id="refArrow" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="var(--muted)" /></marker>
            <marker id="refArrowDashed" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="var(--rf)" /></marker>
          </defs>
        </>
      )}
    </svg>
  );
}

const REF_DETONATION_ENUM = [
  ["DETONATE_OTHER", "0", "default -- nunca setado por Missile"],
  ["DETONATE_ENTITY_IMPACT", "1", "acertou (r² dentro de maxBurstRng²)"],
  ["DETONATE_ENTITY_PROXIMATE_DETONATION", "2", "não usado por Missile"],
  ["DETONATE_GROUND_IMPACT", "3", "não usado -- é o caminho de crashNotification()"],
  ["DETONATE_GROUND_PROXIMATE_DETONATION", "4", "não usado por Missile"],
  ["DETONATE_DETONATION", "5", "passou do ponto de menor aproximação sem acertar"],
  ["DETONATE_NONE", "6", "reset() inicial, antes de qualquer detonação"],
];

const REF_SLOT_DOCS = {
  minSpeed: ["m/s", "cmdVelocity após o fim da queima (isEngineBurnEnabled()==false) -- vpMin"],
  maxSpeed: ["m/s", "cmdVelocity durante a queima, E a velocidade assumida do PRÓPRIO míssil no cálculo do ponto de interceptação (v, em weaponGuidance()) -- vpMax"],
  speedMaxG: ["m/s", "NUNCA lido em weaponGuidance()/weaponDynamics() -- grep confirma. Slot morto para efeito de comportamento nesta classe -- vpMaxG"],
  maxg: ["g's", "taxa de giro máxima (ra_max), uma CONSTANTE -- não escala com a velocidade atual apesar do nome parecido com speedMaxG -- maxG"],
  maxAccel: ["m/s/s", "limite de aceleração longitudinal (vpdot) -- maxAccel"],
  cmdPitch: ["rad", "comando de arfagem -- sobrescrito a cada frame por weaponGuidance() quando a guiagem está ligada"],
  cmdHeading: ["rad", "comando de rumo -- idem, sobrescrito toda vez que isGuidanceEnabled()"],
  cmdSpeed: ["m/s", "campo cmdVelocity -- também recalculado toda vez em weaponGuidance() (burn/no-burn)"],
};

function renderMissileSnippet(key) {
  const snip = missileSnip(key);
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

function MissileReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Missile"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Missile</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "Missile"</span>
          <span className="mx-chip">MISSILE | GUIDED</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Missile")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          Míssil ar-ar GENÉRICO (nickname/descrição nativa: "AAM") -- é o que qualquer subclasse concreta
          (`( AamMissile )`, `( Sam )`) herda quando não sobrescreve a guiagem. Guia por{" "}
          <b>ponto de interceptação</b>: a cada frame extrapola onde o alvo VAI estar e mira nesse ponto
          futuro -- não onde ele ESTÁ agora. Cinemático por padrão (sem dynamicsModel), como o{" "}
          <code className="mx-mono">( GuidedMissile )</code> deste repositório (aba step-by-step) -- só a
          LEI DE GUIAGEM muda: aqui é ponto de interceptação, lá é navegação proporcional.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Missile → AbstractWeapon → Player → AbstractPlayer → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Missile" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "lab"} data-on={tab === "lab" ? 1 : 0} onClick={() => setTab("lab")}>Laboratório de guiagem</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Como a guiagem decide, em 4 passos</div>
              <ol style={{ margin: 0, paddingLeft: 18, fontSize: 12, lineHeight: 1.7 }}>
                <li>Mede o alcance (<code className="mx-mono">trng</code>) e sua taxa de variação (<code className="mx-mono">trdot</code>) até o alvo.</li>
                <li>Se <code className="mx-mono">isGuidanceEnabled()</code> (TOF ≥ tsg), estima o tempo até o encontro e extrapola a posição FUTURA do alvo.</li>
                <li>Comanda rumo/arfagem para esse ponto futuro -- <code className="mx-mono">weaponDynamics()</code> integra até lá, limitado por G/aceleração.</li>
                <li>A cada frame, uma espoleta separada verifica se o alcance PAROU de diminuir -- é esse instante, não um limiar de distância, que decide a detonação.</li>
              </ol>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>enum Detonation -- o resultado, separado do mode</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 8px" }}>
                O <code className="mx-mono">mode</code> só diz DETONATED -- nunca "por quê". A razão fica aqui.
              </p>
              <div className="mx-slotgrid">
                {REF_DETONATION_ENUM.map(([k, v, d]) => (
                  <div className="mx-enumrow" key={k}>
                    <div className="mx-enumrow-name">{k} <span style={{ color: "var(--muted)", fontWeight: 400 }}>={v}</span></div>
                    <div className="mx-enumrow-desc">{d}</div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {tab === "lab" && <MissileGuidanceLab />}

        {tab === "slots" && (
          <div className="mx-card">
            <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Missile</div>
            <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>
              {entry ? entry.own : 8} slots próprios. Mais os herdados de AbstractWeapon (maxTOF, tsg, maxBurstRng,
              lethalRange, sobt, eobt, dummy, jettisonable...) -- ver Catálogo para a lista completa da cadeia.
            </p>
            <div className="mx-slotgrid">
              {(entry ? entry.sl : Object.keys(REF_SLOT_DOCS)).map((s) => (
                <div className="mx-slot" key={s}>
                  <span>{s}{REF_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                  <span style={{ maxWidth: 360 }}>{REF_SLOT_DOCS[s] ? REF_SLOT_DOCS[s][1] : ""}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Missile::Missile() -- construtor, os defaults usados no laboratório</span><span>C++</span></div>
              {renderMissileSnippet("Missile::Missile (construtor)")}
            </div>

            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Missile::weaponGuidance(dt) -- ponto de interceptação + espoleta, na íntegra</span><span>C++</span></div>
              {renderMissileSnippet("Missile::weaponGuidance (native)")}
              <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 8, marginBottom: 0 }}>
                <code className="mx-mono">v = max(vpMax, |alvo|+1)</code>: o míssil sempre se assume mais rápido que o
                alvo por pelo menos 1 m/s -- sem essa garantia, <code className="mx-mono">vtnlos2</code> poderia superar{" "}
                <code className="mx-mono">v²</code> e <code className="mx-mono">vmplos</code> (a raiz) sairia de um
                número negativo. Não é defesa contra caso extremo -- é uma pré-condição que o próprio código impõe
                antes da raiz.
              </p>
            </div>

            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Missile::weaponDynamics(dt) -- integra rumo/arfagem/velocidade limitados por G/aceleração</span><span>C++</span></div>
              {renderMissileSnippet("Missile::weaponDynamics (native)")}
              <p className="mx-warn" style={{ marginTop: 10, marginBottom: 0 }}>
                Observação, NÃO medida rodando (uma leitura, não uma confirmação):{" "}
                <code className="mx-mono">g = base::ETHG</code> vale 32,16 -- em PÉS/s²
                (<code className="mx-mono">contexts/src/mixr/include/mixr/base/util/constants.hpp</code>).
                Os próprios comentários do slot table, duas seções acima, documentam a velocidade em METROS/s
                ("Minimum Velocity (m/s)"). Se os dois se misturam sem conversão em{" "}
                <code className="mx-mono">ra_max = gmax * g / getTotalVelocity()</code>, a taxa de giro nativa sai
                ~3,28× (1/0,3048) maior do que o autor provavelmente pretendia -- a mesma razão que levou o{" "}
                <code className="mx-mono">( GuidedMissile )</code> deste repositório a usar{" "}
                <code className="mx-mono">base::ETHGM</code> (já convertida) em vez de <code className="mx-mono">base::ETHG</code>.
                O laboratório reproduz o valor LITERAL do código (32,16 sem conversão) -- é por isso que a taxa de
                giro ali é o que é.
              </p>
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ====================== Referência -- Steerpoint / Route / Navigation / Autopilot ================
 * Segunda leva da enciclopédia (a primeira foi só Missile). Mesmo padrão: hero + sub-abas
 * (Visão geral / Laboratório / Slots / Código-fonte), tudo fundado em código real com
 * citação de arquivo:linha -- nada aqui foi suposto. A cadeia coberta é
 * Steerpoint (dado do waypoint) -> Route (sequenciador) -> Navigation (agregador,
 * repassa o que o Route já calculou) -> Autopilot (consumidor, navMode).
 * ==================================================================================== */

const NAV_SNIPPETS = {
  "Steerpoint::compute (geodesia)": {
    file: "contexts/src/mixr/src/models/navigation/Steerpoint.cpp",
    line: 611,
    trunc: true,
    lines: [
      "bool Steerpoint::compute(const Navigation* const nav, const Steerpoint* const from)",
      "{",
      "    bool ok{};",
      "    if (nav != nullptr) {",
      "",
      "        // ---",
      "        // Update Mag Var (if needed)",
      "        // ---",
      "        if (haveInitMagVar) {",
      "            magvar = initMagVar;",
      "        } else {",
      "            magvar = static_cast<double>(nav->getMagVarDeg());",
      "        }",
      "",
      "        // ---",
      "        // Make sure we have a position vector and compute lat/lon, if needed",
      "        // ---",
      "        if ( !isLatLonValid() && isPosVecValid() ) {",
      "            // Compute our lat/lon when we only have the Pos Vec",
      "            double elev = 0.0;",
      "            base::nav::convertPosVec2LL(nav->getRefLatitude(), nav->getRefLongitude(), posVec, &latitude, &longitude, &elev);",
      "            elevation  = static_cast<double>(elev);",
      "            needLL = false;",
      "        }",
      "        if ( isLatLonValid() && !isPosVecValid() ) {",
      "            // Compute our Pos Vec when we only have the lat/lon",
      "            base::nav::convertLL2PosVec(nav->getRefLatitude(), nav->getRefLongitude(), latitude, longitude, elevation, &posVec);",
      "            needPosVec = false;",
      "        }",
      "",
      "        // ## Note: at this point we need a valid lat/lon position",
      "",
      "        if (isLatLonValid()) {",
      "",
      "            // ---",
      "            // Compute 'direct-to' bearing,  distance & time",
      "            // ---",
      "            double toBrg{};",
      "            double toDist{};",
      "            double toTTG{};",
      "            base::nav::gll2bd(nav->getLatitude(), nav->getLongitude(), getLatitude(), getLongitude(), &toBrg, &toDist);",
      "",
      "            setTrueBrgDeg( static_cast<double>(toBrg) );",
      "            setDistNM( static_cast<double>(toDist) );",
      "            setMagBrgDeg( base::angle::aepcdDeg( getTrueBrgDeg() - getMagVarDeg() ) );",
    ],
  },
  "Route::autoSequencer+triggerAction (native)": {
    file: "contexts/src/mixr/src/models/navigation/Route.cpp",
    line: 163,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Auto Sequence through Steerpoints",
      "//------------------------------------------------------------------------------",
      "void Route::autoSequencer(const double, const Navigation* const nav)",
      "{",
      "   if (isAutoSequence() && to != nullptr && nav != nullptr) {",
      "      Steerpoint* toSP{static_cast<Steerpoint*>(to->object())};",
      "      if (toSP->getDistNM() <= autoSeqDistNM) {",
      "         // We're within range of the steerpoint, so compute our relative",
      "         // to see if we just passed it.",
      "         const double rbrg{base::angle::aepcdDeg(toSP->getTrueBrgDeg() - nav->getHeadingDeg())};",
      "         if ( std::fabs(rbrg) >= 90.0) {",
      "            // We're within range and we're going away from it, so ...",
      "            triggerAction();",
      "            incStpt();",
      "         }",
      "      }",
      "   }",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// trigger the 'to' steerpoint's action (if any)",
      "//------------------------------------------------------------------------------",
      "void Route::triggerAction()",
      "{",
      "   // ---",
      "   // find and start the current 'to' steerpoint action",
      "   // ---",
      "   Player* own{static_cast<Player*>(findContainerByType(typeid(Player)))};",
      "   if (to != nullptr && own != nullptr) {",
      "      Steerpoint* toSP{static_cast<Steerpoint*>(to->object())};",
      "      Action* toAction{toSP->getAction()};",
      "      if (toAction != nullptr) {",
      "         OnboardComputer* obc{own->getOnboardComputer()};",
      "         if (obc != nullptr) obc->triggerAction(toAction);",
      "      }",
      "   }",
      "}",
    ],
  },
  "Navigation::process (fase 3)": {
    file: "contexts/src/mixr/src/models/navigation/Navigation.cpp",
    line: 191,
    trunc: false,
    lines: [
      "void Navigation::process(const double dt)",
      "{",
      "   BaseClass::process(dt);",
      "",
      "   // ---",
      "   // Update our position, attitude and velocities",
      "   // ---",
      "   if (getOwnship() != nullptr) {",
      "      velValid = updateSysVelocity();",
      "      posValid = updateSysPosition();",
      "      attValid = updateSysAttitude();",
      "      magVarValid = updateMagVar();",
      "   }",
      "   else {",
      "      posValid = false;",
      "      attValid = false;",
      "      velValid = false;",
      "      magVarValid = false;",
      "   }",
      "",
      "   // Update UTC",
      "   double v {utc + dt};",
      "   if (v >= base::time::D2S) {",
      "      v = (v - base::time::D2S);",
      "   }",
      "   setUTC(v);",
      "",
      "   // ---",
      "   // Update our primary route",
      "   // ---",
      "   if (priRoute != nullptr) priRoute->tcFrame(dt);",
      "",
      "   // Update our bullseye",
      "   if (bull != nullptr) bull->compute(this);",
      "",
      "   // ---",
      "   // Update our navigational steering data",
      "   // ---",
      "   updateNavSteering();",
      "}",
    ],
  },
  "Navigation::updateNavSteering (native)": {
    file: "contexts/src/mixr/src/models/navigation/Navigation.cpp",
    line: 700,
    trunc: false,
    lines: [
      "// (default) Nav steering function (pull data from the 'to' steerpoint)",
      "bool Navigation::updateNavSteering()",
      "{",
      "   if (getPriRoute() != nullptr) {",
      "      const Steerpoint* to{getPriRoute()->getSteerpoint()};",
      "      if (to != nullptr) {",
      "         if (to->isNavDataValid()) {",
      "            setTrueBrgDeg( to->getTrueBrgDeg() );",
      "            setMagBrgDeg( to->getMagBrgDeg() );",
      "            setDistNM( to->getDistNM()) ;",
      "            setTrueCrsDeg( to->getTrueCrsDeg() );",
      "            setMagCrsDeg( to->getMagCrsDeg() );",
      "            setTTG( to->getTTG() );",
      "            setETA( to->getETA() );",
      "            setCrossTrackErrorNM( to->getCrossTrackErrNM() );",
      "            setNavSteeringValid( true );",
      "         } else {",
      "            setNavSteeringValid( false );",
      "         }",
      "      }",
      "   }",
      "   return isNavSteeringValid();",
      "}",
    ],
  },
  "Autopilot::modeManager (native)": {
    file: "contexts/src/mixr/src/models/system/Autopilot.cpp",
    line: 200,
    trunc: false,
    lines: [
      "bool Autopilot::modeManager()",
      "{",
      "   // ---",
      "   // Re-latch the modes -- (just command the previous mode.)",
      "   //  If the mode was off, it still is.",
      "   //  If the mode was on, the 'is' functions will make sure that all",
      "   //  prerequisite are still met.",
      "   // ---",
      "   setNavMode( isNavModeOn() );",
      "   if (!isLoiterModeOn()) {",
      "      //loiterState = 0;",
      "      loiterEntryMode = PREENTRY;",
      "      loiterEntryPhase = 0;",
      "   }",
      "",
      "   // ---",
      "   // Follow our leader mode",
      "   // ---",
      "   if ( isFollowTheLeadModeOn() ) {",
      "      processModeFollowTheLead();",
      "   }",
      "",
      "   // ---",
      "   // Loiter Mode",
      "   // ---",
      "   else if ( isLoiterModeOn() ) {",
      "      processModeLoiter();",
      "   }",
      "",
      "   // ---",
      "   // Navigation (e.g., waypoint follow) Mode",
      "   // ---",
      "   else if ( isNavModeOn() ) {",
      "      processModeNavigation();",
      "   }",
      "",
      "   return true;",
      "}",
    ],
  },
  "Autopilot::processModeNavigation (native)": {
    file: "contexts/src/mixr/src/models/system/Autopilot.cpp",
    line: 242,
    trunc: false,
    lines: [
      "bool Autopilot::processModeNavigation()",
      "{",
      "   bool ok{};",
      "",
      "   const Navigation* nav{getOwnship()->getNavigation()};",
      "",
      "   if (nav != nullptr) {",
      "      // Do we have valid NAV steering data?",
      "      if (nav->isNavSteeringValid()) {",
      "         const double a{nav->getTrueBrgDeg()};",
      "         setCommandedHeadingD( a );",
      "      }",
      "",
      "      // Do we have NAV commanded altitude?",
      "      const Route* route{nav->getPriRoute()};",
      "      if (route != nullptr) {",
      "         const Steerpoint* sp{route->getSteerpoint()};",
      "         if (sp != nullptr) {",
      "            if (sp->isCmdAltValid()) {",
      "               setCommandedAltitudeFt( sp->getCmdAltitudeFt() );",
      "            }",
      "            {",
      "               const double spd{sp->getCmdAirspeedKts()};",
      "               if (spd > 0) {",
      "                  setCommandedVelocityKts( spd );",
      "               }",
      "            }",
      "         }",
      "      }",
      "      ok = true;",
      "   }",
      "",
      "   if (!ok) setNavMode( false );",
      "   return ok;",
      "}",
    ],
  },
  "Autopilot::setNavMode (native)": {
    file: "contexts/src/mixr/src/models/system/Autopilot.cpp",
    line: 1039,
    trunc: false,
    lines: [
      "bool Autopilot::setNavMode(const bool flag)",
      "{",
      "   bool navModeOn1{navModeOn};",
      "",
      "   // Set NAV mode",
      "   navModeOn = flag && isRollSasOn() && isPitchSasOn();",
      "   if (navModeOn) {",
      "      setHeadingHoldMode(true);",
      "      setAltitudeHoldMode(true);",
      "      setVelocityHoldMode(true);",
      "   }",
      "",
      "   // If Nav mode was just turned off,",
      "   // set commanded heading and altitude to our current values",
      "   if ( !navModeOn && navModeOn1 ) {",
      "      Player* pv{getOwnship()};",
      "      if (pv != nullptr) {",
      "        const double hdg{pv->getHeadingD()};",
      "        setCommandedHeadingD(hdg);",
      "        setCommandedAltitudeFt(pv->getAltitudeFt());",
      "        setCommandedVelocityKts( pv->getTotalVelocityKts() );",
      "      }",
      "   }",
      "",
      "   return (flag == navModeOn);",
      "}",
    ],
  },
  "Autopilot::headingController (native)": {
    file: "contexts/src/mixr/src/models/system/Autopilot.cpp",
    line: 877,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Heading/roll controller --",
      "//------------------------------------------------------------------------------",
      "bool Autopilot::headingController()",
      "{",
      "   // Re-latch the mode",
      "   setHeadingHoldMode( isHeadingHoldOn() );",
      "",
      "   Player* pv{getOwnship()};",
      "   if (pv != nullptr) {",
      "      DynamicsModel* md{pv->getDynamicsModel()};",
      "      if (md != nullptr) {",
      "         // why mess with the player?  All it does is send it to the dynamics model anyways!  Skip the middle man!",
      "         if ( isHeadingHoldOn() || isNavModeOn() ) {",
      "            const int ihdg10{static_cast<int>( getCommandedHeadingD() * 10.0f )};",
      "            const double hdg{static_cast<double>(ihdg10) / 10.0};",
      "            md->setCommandedHeadingD(hdg, maxTurnRateDps, maxBankAngleDegs);",
      "            md->setHeadingHoldOn( true );",
      "         } else {",
      "            md->setHeadingHoldOn( false );",
      "            md->setControlStickRollInput( getControlStickRollInput() );",
      "         }",
      "      }",
      "   }",
      "   return true;",
      "}",
    ],
  },
  "RacModel::setCommandedHeadingD (native)": {
    file: "contexts/src/mixr/src/models/dynamics/RacModel.cpp",
    line: 128,
    trunc: false,
    lines: [
      "// setCommandedHeadingD() --   Sets commanded heading (true: degs)",
      "bool RacModel::setCommandedHeadingD(const double degs, const double, const double)",
      "{",
      "   cmdHeading = degs;",
      "   return true;",
      "}",
    ],
  },
  "JSBSimModel::setCommandedHeadingD (native)": {
    file: "contexts/src/mixr/src/models/dynamics/JSBSimModel.cpp",
    line: 925,
    trunc: false,
    lines: [
      "bool JSBSimModel::setHeadingHoldOn(const bool b)",
      "{",
      "    if (hasHeadingHold) {",
      "        headingHoldOn = b;",
      "    }",
      "    return hasHeadingHold;",
      "}",
      "",
      "bool JSBSimModel::setCommandedHeadingD(const double h, const double, const double)",
      "{",
      "    commandedHeadingDeg = h;",
      "    return hasHeadingHold;",
      "}",
    ],
  },
  "NavigateAction (constante)": {
    file: "models/players/A-4/src/bt/nodes/NavigateAction.cpp",
    line: 10,
    trunc: false,
    lines: [
      "namespace {",
      "",
      "// Taxa maxima do RUMO COMANDADO, deliberadamente mais apertada que o",
      "// maxRateOfTurnDps do Autopilot (6 deg/s nos cenarios desta poc): o limite",
      "// da AERONAVE nao e o que causa o problema (medido rodando: a divergencia",
      "// aparecia girando a ~1.6 deg/s, bem abaixo do teto da aeronave) -- o",
      "// limite tem de estar no COMANDO em si. Ver o comentario de tick().",
      "constexpr double kMaxHeadingRateDegPerSec{3.0};",
      "",
      "} // namespace",
    ],
  },
  "NavigateAction::tick (próprio deste projeto)": {
    file: "models/players/A-4/src/bt/nodes/NavigateAction.cpp",
    line: 61,
    trunc: false,
    lines: [
      "BT::NodeStatus NavigateAction::tick()",
      "{",
      "   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;",
      "",
      "   const auto& view = context_.behavior->snapshot();",
      "   if (!view.hasNavSteering) {",
      "      hasCommandedHeading_ = false;",
      "      return BT::NodeStatus::FAILURE;",
      "   }",
      "",
      "   if (!hasCommandedHeading_) {",
      "      // Primeiro tick com guiagem valida: nao ha rumo anterior para",
      "      // suavizar a partir dele -- comeca exatamente na marcacao.",
      "      commandedHeadingDeg_ = view.navTrueBrgDeg;",
      "      hasCommandedHeading_ = true;",
      "   } else {",
      "      const double dt{context_.behavior->getFrameDt()};",
      "      const double errorDeg{domain::wrap180(view.navTrueBrgDeg - commandedHeadingDeg_)};",
      "      const double maxStepDeg{kMaxHeadingRateDegPerSec * dt};",
      "      const double stepDeg{std::clamp(errorDeg, -maxStepDeg, maxStepDeg)};",
      "",
      "      commandedHeadingDeg_ = domain::wrap360(commandedHeadingDeg_ + stepDeg);",
      "   }",
      "",
      "   domain::FlightCommand cmd;",
      "   cmd.headingDeg = commandedHeadingDeg_;",
      "   cmd.altitudeM = context_.behavior->clampAltitudeToTerrain(",
      "      view.hasNavCmdAlt ? view.navCmdAltM : view.altitudeM);",
      "   cmd.speedKts = view.hasNavCmdSpeed ? view.navCmdSpeedKts : view.speedKts;",
      "",
      "   context_.behavior->decision().take(cmd, \"NAV\");",
      "   return BT::NodeStatus::SUCCESS;",
      "}",
    ],
  },
};

const navSnip = (key) => (key ? NAV_SNIPPETS[key] || null : null);

function renderNavSnippet(key) {
  const snip = navSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

/* ------------------------------------------------------------------------------
 * A rota REAL de tests/fixtures/full-systems-nav/configs/scenario_full_nav.edl.in
 * (linhas 293-336) -- os 4 únicos steerpoints deste repositório navegados de
 * verdade por Route/Steerpoint nativo (todo o resto pilota via árvore de
 * comportamento com navMode:false). autoSeqDistance/wrap também são os valores
 * reais do arquivo -- nada aqui foi inventado para o laboratório.
 * ------------------------------------------------------------------------------ */
const NAV_ROUTE_WPTS = [
  { n: 9290, e: 3000, altM: 1750, kt: 350, action: "ActionDecoyRelease", desc: "solta decoy" },
  { n: 6000, e: 7370, altM: 1900, kt: 370, action: "ActionImagingSar", desc: "imageamento SAR" },
  { n: 3230, e: 4000, altM: 1750, kt: 350, action: "ActionCamouflageType", desc: "troca de camuflagem" },
  { n: 6000, e: 1600, altM: 1600, kt: 360, action: "ActionWeaponRelease", desc: "libera arma" },
];
const NAV_AUTO_SEQ_DIST_M = 1.5 * 1852; // autoSeqDistance: ( NauticalMiles 1.5 )
const NAV_AC_MAX_TURN_DPS = 6.0;        // maxRateOfTurnDps tipico deste projeto -- limite FISICO
const NAV_CMD_MAX_TURN_DPS = 3.0;       // NavigateAction.cpp: kMaxHeadingRateDegPerSec -- limite do COMANDO
const NAV_DT = 0.5;
const NAV_KT2MPS = 0.514444;

/* ------------------------------------------------------------------------------
 * simulateRouteNavigation(mode) -- reimplementação em JS puro da MESMA cadeia
 * medida no fonte: Route::autoSequencer() decide QUANDO sequenciar (distância
 * <= autoSeqDistance E marcação já variou >=90° do rumo atual -- não um raio
 * simples), e o rumo comandado responde de duas formas possíveis:
 *
 *  - "native": Autopilot::processModeNavigation() -- setCommandedHeadingD(a
 *    marcação bruta), TODO frame, sem filtro nenhum. Perseguição pura.
 *  - "own": NavigateAction::tick() (bt/nodes/NavigateAction.cpp, este
 *    repositório) -- rampa a no máximo kMaxHeadingRateDegPerSec (3°/s) por
 *    frame.
 *
 * Não modela guinada em 3D nem JSBSim -- é um ponto de massa 2D (plano N/E)
 * cujo ÚNICO papel é tornar visível o SALTO DISCONTINUO no rumo comandado no
 * instante em que o Route sequencia, que é o que os dois trechos de código
 * acima de fato fazem (ou deixam de fazer). Reproduzir a divergência real de
 * ~200s medida com JSBSim 6-DOF (ver NavigateAction.cpp e
 * models/players/A-4/CLAUDE.md) exigiria a dinâmica acoplada de rolagem/guinada
 * real -- fora do que um modelo cinemático simples pode honestamente alegar.
 * ------------------------------------------------------------------------------ */
function simulateRouteNavigation(mode) {
  let n = NAV_ROUTE_WPTS[3].n, e = NAV_ROUTE_WPTS[3].e - 1000;
  let wpIdx = 0;
  const seedBrg = Math.atan2(NAV_ROUTE_WPTS[0].e - e, NAV_ROUTE_WPTS[0].n - n);
  let heading = seedBrg;
  let cmdHeading = seedBrg;

  const frames = [];
  const events = [];
  let t = 0, seqCount = 0;
  const MAXT = 400;
  while (t <= MAXT && seqCount < 8) {
    const wp = NAV_ROUTE_WPTS[wpIdx];
    const dn = wp.n - n, de = wp.e - e;
    const dist = Math.hypot(dn, de);
    const brg = Math.atan2(de, dn);
    const rel = Math.abs(refWrapPi(brg - heading)) * (180 / Math.PI);

    let sequenced = false;
    if (dist <= NAV_AUTO_SEQ_DIST_M && rel >= 90) {
      events.push({ t, wpIdx, action: wp.action, desc: wp.desc, distM: dist });
      wpIdx = (wpIdx + 1) % NAV_ROUTE_WPTS.length;
      seqCount++;
      sequenced = true;
    }

    frames.push({
      t, n, e, wpIdx, dist, sequenced,
      headingDeg: refWrap2pi(heading) * (180 / Math.PI),
      rawBrgDeg: refWrap2pi(brg) * (180 / Math.PI),
      cmdHeadingDeg: refWrap2pi(cmdHeading) * (180 / Math.PI),
    });

    if (mode === "native") {
      cmdHeading = brg; // Autopilot::processModeNavigation(): setCommandedHeadingD(nav->getTrueBrgDeg())
    } else {
      const err = refWrapPi(brg - cmdHeading);
      const maxStep = NAV_CMD_MAX_TURN_DPS * (Math.PI / 180) * NAV_DT;
      cmdHeading = refWrap2pi(cmdHeading + refClamp(err, -maxStep, maxStep)); // NavigateAction::tick()
    }

    const errH = refWrapPi(cmdHeading - heading);
    const maxStepH = NAV_AC_MAX_TURN_DPS * (Math.PI / 180) * NAV_DT;
    heading = refWrap2pi(heading + refClamp(errH, -maxStepH, maxStepH));

    const speed = wp.kt * NAV_KT2MPS;
    n += speed * Math.cos(heading) * NAV_DT;
    e += speed * Math.sin(heading) * NAV_DT;
    t += NAV_DT;
  }
  return { frames, events };
}

function AutopilotNavLab() {
  const [mode, setMode] = useState("native");
  const [idx, setIdx] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [playSpeed, setPlaySpeed] = useState(30);

  const sim = useMemo(() => simulateRouteNavigation(mode), [mode]);
  const frames = sim.frames;
  const clampedIdx = Math.min(idx, frames.length - 1);
  const frame = frames[clampedIdx] || frames[0];

  useEffect(() => { setIdx(0); setPlaying(false); }, [mode]);
  useEffect(() => {
    if (!playing) return;
    const tmr = setTimeout(() => {
      setIdx((p) => (p + 1 >= frames.length ? (setPlaying(false), p) : p + 1));
    }, playSpeed);
    return () => clearTimeout(tmr);
  }, [playing, idx, playSpeed, frames.length]);

  const bounds = useMemo(() => {
    let minN = 0, maxN = 0, minE = 0, maxE = 0;
    NAV_ROUTE_WPTS.forEach((wp) => { minN = Math.min(minN, wp.n); maxN = Math.max(maxN, wp.n); minE = Math.min(minE, wp.e); maxE = Math.max(maxE, wp.e); });
    frames.forEach((f) => { minN = Math.min(minN, f.n); maxN = Math.max(maxN, f.n); minE = Math.min(minE, f.e); maxE = Math.max(maxE, f.e); });
    const pad = Math.max(600, (maxN - minN) * 0.15, (maxE - minE) * 0.15);
    return { minN: minN - pad, maxN: maxN + pad, minE: minE - pad, maxE: maxE + pad };
  }, [frames]);

  const W = 520, H = 380;
  const spanN = Math.max(1, bounds.maxN - bounds.minN);
  const spanE = Math.max(1, bounds.maxE - bounds.minE);
  const scale = Math.min((W - 20) / spanE, (H - 20) / spanN);
  const px = (e) => (e - bounds.minE) * scale + (W - spanE * scale) / 2;
  const py = (n) => H - ((n - bounds.minN) * scale + (H - spanN * scale) / 2);

  const trail = frames.slice(0, clampedIdx + 1).map((f) => `${px(f.e)},${py(f.n)}`).join(" ");
  const move = useCallback((d) => { setPlaying(false); setIdx((p) => refClamp(p + d, 0, frames.length - 1)); }, [frames.length]);

  const chartW = 520, chartH = 130, chartPad = 30;
  const lastT = Math.max(frames[frames.length - 1].t, 0.001);
  const chartX = (tv) => chartPad + (tv / lastT) * (chartW - chartPad - 8);
  const chartY = (deg) => chartH - 14 - (deg / 360) * (chartH - 24);

  const passedEvents = sim.events.filter((ev) => ev.t <= frame.t);
  const lastEvent = passedEvents[passedEvents.length - 1];

  return (
    <div className="mx-card">
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", flexWrap: "wrap", gap: 8, marginBottom: 10 }}>
        <span style={{ fontSize: 13, fontWeight: 700 }}>Laboratório de navegação -- rumo comandado na sequência de steerpoints</span>
        <span className="mx-pill" style={{ background: mode === "native" ? "var(--rf)" : "var(--ok)", color: "var(--paper)" }}>
          ● {mode === "native" ? "Autopilot nativo (navMode)" : "NavigateAction (este projeto)"}
        </span>
      </div>

      <div style={{ display: "flex", gap: 18, flexWrap: "wrap", alignItems: "flex-start" }}>
        <div style={{ flex: "1 1 540px", minWidth: 320 }}>
          <svg viewBox={`0 0 ${W} ${H}`} style={{ width: "100%", display: "block", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
            {NAV_ROUTE_WPTS.map((wp, i) => (
              <g key={i}>
                <circle cx={px(wp.e)} cy={py(wp.n)} r={NAV_AUTO_SEQ_DIST_M * scale} fill="none" stroke="var(--rule)" strokeDasharray="3 3" opacity="0.5" />
                <circle cx={px(wp.e)} cy={py(wp.n)} r={i === frame.wpIdx ? 6 : 4} fill={i === frame.wpIdx ? "var(--hot)" : "var(--muted)"} />
                <text x={px(wp.e) + 8} y={py(wp.n) - 8} style={{ fontSize: 9.5, fill: "var(--muted)" }}>wp{i + 1}</text>
              </g>
            ))}
            <polyline points={trail} fill="none" stroke="var(--bgc)" strokeWidth="1.6" opacity="0.85" />
            <g transform={`translate(${px(frame.e)},${py(frame.n)}) rotate(${frame.headingDeg})`}>
              <polygon points="0,-8 5,7 -5,7" fill="var(--bgc)" />
            </g>
            <line x1={px(frame.e)} y1={py(frame.n)}
                  x2={px(frame.e) + Math.sin((frame.cmdHeadingDeg * Math.PI) / 180) * 26}
                  y2={py(frame.n) - Math.cos((frame.cmdHeadingDeg * Math.PI) / 180) * 26}
                  stroke="var(--hot)" strokeWidth="2" markerEnd="url(#navCmdArrow)" />
            <defs>
              <marker id="navCmdArrow" markerWidth="6" markerHeight="6" refX="5" refY="3" orient="auto"><path d="M0,0 L6,3 L0,6 Z" fill="var(--hot)" /></marker>
            </defs>
          </svg>
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", fontSize: 11, color: "var(--muted)", margin: "5px 0 12px" }}>
            <span><span style={{ color: "var(--bgc)" }}>▲</span> aeronave (rumo real)</span>
            <span><span style={{ color: "var(--hot)" }}>→</span> rumo COMANDADO</span>
            <span style={{ marginLeft: "auto" }}>círculo tracejado = autoSeqDistance (1,5 NM)</span>
          </div>

          <div style={{ display: "flex", gap: 8, alignItems: "center", flexWrap: "wrap", marginBottom: 10 }}>
            <button className="mx-btn" data-primary="1" onClick={() => setPlaying((p) => !p)}>{playing ? "Pausar" : "Reproduzir"}</button>
            <button className="mx-btn" onClick={() => move(-1)}>←</button>
            <button className="mx-btn" onClick={() => move(1)}>→</button>
            <button className="mx-btn" onClick={() => { setPlaying(false); setIdx(0); }}>Início</button>
            <input type="range" min={0} max={frames.length - 1} value={clampedIdx}
                   onChange={(ev) => { setPlaying(false); setIdx(Number(ev.target.value)); }}
                   style={{ flex: 1, minWidth: 120 }} />
            <span className="mx-mono" style={{ fontSize: 11, color: "var(--muted)", minWidth: 90, textAlign: "right" }}>{refFmt(frame.t, 1)}s ({clampedIdx + 1}/{frames.length})</span>
          </div>

          <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginBottom: 14 }}>
            <MissileStat label="distância ao wp" value={`${refFmt(frame.dist)} m`} />
            <MissileStat label="marcação bruta" value={`${refFmt(frame.rawBrgDeg)}°`} />
            <MissileStat label="rumo comandado" value={`${refFmt(frame.cmdHeadingDeg)}°`} />
            <MissileStat label="rumo real" value={`${refFmt(frame.headingDeg)}°`} />
            <MissileStat label="steerpoint" value={`wp${frame.wpIdx + 1} -- ${NAV_ROUTE_WPTS[frame.wpIdx].desc}`} />
          </div>
          {lastEvent && (
            <p style={{ fontSize: 11, color: "var(--muted)", margin: "0 0 10px" }}>
              Último sequenciamento: t={refFmt(lastEvent.t, 1)}s, avançou para wp{((lastEvent.wpIdx + 1) % 4) + 1}
              {" "}(ação disparada em wp{lastEvent.wpIdx + 1}: <code className="mx-mono">{lastEvent.action}</code>).
            </p>
          )}

          <div className="mx-lbl">
            <span>Rumo comandado × tempo -- a LINHA VERTICAL marca cada sequenciamento do Route</span>
            <span>máx. salto: {mode === "native" ? "180° instantâneo" : `${NAV_CMD_MAX_TURN_DPS * NAV_DT}°/quadro`}</span>
          </div>
          <svg viewBox={`0 0 ${chartW} ${chartH}`} style={{ width: "100%", display: "block", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
            <line x1={chartPad} y1={chartH - 14} x2={chartW - 6} y2={chartH - 14} stroke="var(--rule)" />
            {sim.events.map((ev, i) => (
              <line key={i} x1={chartX(ev.t)} y1="4" x2={chartX(ev.t)} y2={chartH - 14} stroke="var(--rf)" strokeWidth="1" strokeDasharray="2 2" opacity="0.65" />
            ))}
            <polyline fill="none" stroke="var(--hot)" strokeWidth="1.4"
                       points={frames.map((f) => `${chartX(f.t)},${chartY(f.cmdHeadingDeg)}`).join(" ")} />
            <polyline fill="none" stroke="var(--muted)" strokeWidth="1" opacity="0.55" strokeDasharray="1 2"
                       points={frames.map((f) => `${chartX(f.t)},${chartY(f.rawBrgDeg)}`).join(" ")} />
            <line x1={chartX(frame.t)} y1="4" x2={chartX(frame.t)} y2={chartH - 14} stroke="var(--bgc)" strokeWidth="1" opacity="0.6" />
          </svg>
        </div>

        <div style={{ flex: "0 1 240px", minWidth: 210 }}>
          <div className="mx-lbl"><span>Comando de rumo</span></div>
          <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: 11.5, marginBottom: 6, cursor: "pointer" }}>
            <input type="radio" checked={mode === "native"} onChange={() => setMode("native")} /> Autopilot::processModeNavigation()
          </label>
          <label style={{ display: "flex", alignItems: "center", gap: 6, fontSize: 11.5, marginBottom: 14, cursor: "pointer" }}>
            <input type="radio" checked={mode === "own"} onChange={() => setMode("own")} /> NavigateAction::tick() (este projeto)
          </label>
          <select className="mx-input" value={playSpeed} onChange={(ev) => setPlaySpeed(Number(ev.target.value))} style={{ width: "100%", marginBottom: 14 }}>
            <option value={70}>Reprodução lenta</option>
            <option value={30}>Reprodução normal</option>
            <option value={12}>Reprodução rápida</option>
          </select>
          <p style={{ fontSize: 11, lineHeight: 1.5, color: "var(--muted)" }}>
            Os 4 waypoints, a distância de sequenciamento (1,5 NM) e as velocidades comandadas são os
            valores REAIS de <code className="mx-mono">tests/fixtures/full-systems-nav/</code> -- o único
            cenário deste repositório que voa por <code className="mx-mono">Route</code>/
            <code className="mx-mono">Steerpoint</code> nativo (os demais usam <code className="mx-mono">navMode: false</code> e
            a própria árvore de comportamento).
          </p>
          <p style={{ fontSize: 11, lineHeight: 1.5, color: "var(--muted)" }}>
            Alterne o modo e repare no gráfico: o modo nativo (linha tracejada = marcação bruta,
            sobreposta à linha cheia = comando) SALTA no instante do sequenciamento -- sem filtro
            nenhum. O modo próprio deste projeto rampa a no máximo 3°/s, visivelmente mais lento
            para acompanhar, mas contínuo.
          </p>
        </div>
      </div>
    </div>
  );
}

/* ---------------------------- Steerpoint / Route ---------------------------- */

const REF_STEERPOINT_SLOT_DOCS = {
  stptType: ["enum", "DEST/MARK/FIX/OAP/IP/TGT/TGT_GRP -- só rótulo; compute() não ramifica por ele"],
  latitude: ["LatLon|Number", "posição geodésica -- fonte primária do cálculo em compute() quando presente"],
  longitude: ["LatLon|Number", "idem, longitude"],
  xPos: ["m", "posição local (N), relativa ao ponto de referência do cenário -- convertida para lat/lon em compute() quando só ela está presente"],
  yPos: ["m", "posição local (E) -- idem"],
  elevation: ["m", "elevação de terreno no ponto -- só armazenada; nada nesta classe a lê de volta"],
  altitude: ["m", "altitude comandada -- lida por getCmdAltitudeFt()/M(), consumida por Autopilot::processModeNavigation() e por NavigateAction (este projeto)"],
  airspeed: ["kt", "velocidade comandada -- lida por getCmdAirspeedKts(), mesmo consumo"],
  pta: ["s", "hora planejada de chegada -- alimenta só o ELT (adiantado/atrasado); zero efeito de voo"],
  sca: ["ft", "altitude de segurança -- alimenta só isWarnSCA(), e NENHUM chamador (nesta classe, no fork ou neste projeto) lê esse getter -- grep confirma zero call sites. Slot morto para efeito de comportamento"],
  description: ["texto", "metadado puro -- nunca lido por compute()/autoSequencer()"],
  magvar: ["deg", "variação magnética override -- sem ele, vem de Navigation::getMagVarDeg()"],
  next: ["nome|índice", "NUNCA lido -- só escrito por setSlotNext(); grep no .cpp inteiro não acha outro uso do membro 'next'. Morto, apesar do comentário do slot table sugerir uma cadeia navegável"],
  action: ["Action", "disparada por Route::triggerAction() -- NÃO pela própria Steerpoint"],
};

const REF_ROUTE_SLOT_DOCS = {
  to: ["nome|índice", "steerpoint \"to\" inicial -- por Identifier (nome) ou Number (índice, 1-based)"],
  autoSequence: ["bool", "liga o avanço automático -- sem ele, só directTo()/incStpt() manuais mudam o \"to\""],
  autoSeqDistance: ["NM", "raio de teste em autoSequencer() -- é só METADE da condição real (ver Código-fonte)"],
  wrap: ["bool", "volta ao steerpoint 1 depois do último em incStpt()/decStpt(), em vez de travar na ponta"],
};

function SteerpointGeometryDiagram() {
  const from = { x: 40, y: 230 };
  const to = { x: 460, y: 60 };
  const own = { x: 210, y: 175 };
  // projecao do ownship sobre a reta from-to, para desenhar o erro de cross-track
  const dx = to.x - from.x, dy = to.y - from.y;
  const len2 = dx * dx + dy * dy;
  const tproj = ((own.x - from.x) * dx + (own.y - from.y) * dy) / len2;
  const proj = { x: from.x + dx * tproj, y: from.y + dy * tproj };
  return (
    <svg viewBox="0 0 520 260" style={{ width: "100%", background: "var(--graph-bg)", border: "1px solid var(--rule)", borderRadius: 3 }}>
      <line x1={from.x} y1={from.y} x2={to.x} y2={to.y} stroke="var(--muted)" strokeWidth="1.4" strokeDasharray="5 3" />
      <line x1={own.x} y1={own.y} x2={to.x} y2={to.y} stroke="var(--hot)" strokeWidth="1.6" />
      <line x1={own.x} y1={own.y} x2={proj.x} y2={proj.y} stroke="var(--rf)" strokeWidth="1.4" strokeDasharray="2 2" />
      <circle cx={from.x} cy={from.y} r="5" fill="var(--muted)" />
      <circle cx={to.x} cy={to.y} r="5" fill="var(--bgc)" />
      <circle cx={own.x} cy={own.y} r="5" fill="var(--hot)" />
      <text x={from.x - 6} y={from.y + 18} textAnchor="middle" style={{ fontSize: 9.5, fill: "var(--muted)" }}>from</text>
      <text x={to.x + 4} y={to.y - 10} textAnchor="start" style={{ fontSize: 9.5, fill: "var(--ink)" }}>to</text>
      <text x={own.x} y={own.y + 18} textAnchor="middle" style={{ fontSize: 9.5, fill: "var(--hot)" }}>navegação atual</text>
      <text x={(own.x + to.x) / 2 + 8} y={(own.y + to.y) / 2 - 4} style={{ fontSize: 8.6, fill: "var(--hot)" }}>trueBrgDeg (direct-to)</text>
      <text x={(from.x + to.x) / 2} y={(from.y + to.y) / 2 - 8} style={{ fontSize: 8.6, fill: "var(--muted)" }}>trueCrsDeg (leg, from→to)</text>
      <text x={(own.x + proj.x) / 2 + 6} y={(own.y + proj.y) / 2} style={{ fontSize: 8.6, fill: "var(--rf)" }}>crossTrackErrNM</text>
    </svg>
  );
}

function SteerpointReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Steerpoint"];
  const routeEntry = MODEL["Route"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Steerpoint</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "Steerpoint"</span>
          <span className="mx-chip">+ Route (sequenciador)</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Steerpoint")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          Um <b>Steerpoint</b> é um waypoint: posição (lat/lon OU N/E local -- as duas formas convivem,
          uma calcula a outra), mais altitude/velocidade COMANDADAS e uma <code className="mx-mono">Action</code>{" "}
          opcional. Sozinho, ele só sabe calcular sua PRÓPRIA geodésia (marcação, distância, cross-track)
          contra a posição atual -- quem decide QUANDO avançar para o próximo é a classe irmã{" "}
          <code className="mx-mono">Route</code>, que mantém a lista de steerpoints e o índice "to" atual.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Steerpoint → Component → Object"}
          {" "}·{" "}
          {routeEntry ? routeEntry.ch.join(" → ") : "Route → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Steerpoint" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Steerpoint::compute() -- a geodesia, em 3 números</div>
              <ol style={{ margin: 0, paddingLeft: 18, fontSize: 12, lineHeight: 1.7 }}>
                <li><code className="mx-mono">trueBrgDeg</code>/<code className="mx-mono">distNM</code> -- marcação e distância DIRETO daqui até este ponto (<code className="mx-mono">base::nav::gll2bd()</code>, geodésia WGS-84 real, não plana).</li>
                <li><code className="mx-mono">trueCrsDeg</code> -- o RUMO DA PERNA (from→to), só existe quando há um steerpoint "from" anterior; sem ele, é igual ao direct-to.</li>
                <li><code className="mx-mono">crossTrackErrNM</code> -- o desvio lateral em relação a essa perna, não à posição do waypoint em si.</li>
              </ol>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "8px 0 0" }}>
                É <code className="mx-mono">trueBrgDeg</code> (o direct-to, não o course da perna) que
                <code className="mx-mono"> Navigation::updateNavSteering()</code> repassa adiante -- ver a aba Navigation.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Geometria (diagrama)</div>
              <SteerpointGeometryDiagram />
              <p style={{ fontSize: 11, color: "var(--muted)", margin: "8px 0 0" }}>
                <code className="mx-mono">crossTrackErrNM</code> é negativo quando o rumo desejado (a perna) está à
                ESQUERDA da posição atual -- fórmula: <code className="mx-mono">distNM · sin(trueBrgDeg − trueCrsDeg)</code>.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Route::autoSequencer() -- o teste real é EM DUAS PARTES</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                Não é "cheguei perto, avanço": <code className="mx-mono">autoSequencer()</code> só avança quando as
                DUAS condições valem no MESMO frame -- <b>(1)</b> a distância ao steerpoint "to" já caiu para dentro
                de <code className="mx-mono">autoSeqDistance</code>, <b>E</b> <b>(2)</b> a marcação relativa a esse
                ponto (marcação menos rumo ATUAL) já passou de ±90° -- ou seja, a aeronave já está indo EMBORA dele,
                não só perto. Um sobrevoo tangencial que nunca chega a "virar as costas" para o ponto (rel &lt; 90°)
                nunca sequencia, mesmo dentro do raio.
              </p>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado, não redescobrir:</b> <code className="mx-mono">triggerAction()</code> é chamado{" "}
                <b>ANTES</b> de <code className="mx-mono">incStpt()</code> (Route.cpp, linha 176-177) -- a{" "}
                <code className="mx-mono">Action</code> do steerpoint atual dispara enquanto o "to" AINDA é esse
                mesmo steerpoint, só avançando um comando depois. Isso contradiz o comentário do próprio slot table de
                Steerpoint.hpp ("the 'to' steerpoint will have sequenced to the next steerpoint when action is
                triggered") -- a documentação nativa descreve a ordem TROCADA em relação ao código real.
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Steerpoint</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{entry ? entry.own : 14} slots próprios.</p>
              <div className="mx-slotgrid">
                {(entry ? entry.sl : Object.keys(REF_STEERPOINT_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_STEERPOINT_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_STEERPOINT_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 360 }}>{REF_STEERPOINT_SLOT_DOCS[s] ? REF_STEERPOINT_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Route</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{routeEntry ? routeEntry.own : 4} slots próprios -- o "container" que agrega os Steerpoint (via <code className="mx-mono">components:</code>, não um slot próprio).</p>
              <div className="mx-slotgrid">
                {(routeEntry ? routeEntry.sl : Object.keys(REF_ROUTE_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_ROUTE_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_ROUTE_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 300 }}>{REF_ROUTE_SLOT_DOCS[s] ? REF_ROUTE_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Steerpoint::compute(nav, from) -- geodésia real (WGS-84), não plana</span><span>C++</span></div>
              {renderNavSnippet("Steerpoint::compute (geodesia)")}
              <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 8, marginBottom: 0 }}>
                <code className="mx-mono">base::nav::gll2bd()</code> é a mesma família de utilitário geodésico que{" "}
                <code className="mx-mono">domain::pursuit()</code> (aba step-by-step) usa por trás -- great-circle
                sobre o elipsoide WGS-84, não uma aproximação de plano cartesiano local.
              </p>
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Route::autoSequencer() + Route::triggerAction() -- na íntegra</span><span>C++</span></div>
              {renderNavSnippet("Route::autoSequencer+triggerAction (native)")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ---------------------------- Navigation (Ins/Gps) ---------------------------- */

const REF_NAVIGATION_SLOT_DOCS = {
  route: ["Route", "a rota PRIMÁRIA agregada -- getPriRoute() -- Navigation só repassa o que ela já calculou"],
  utc: ["s", "hora do dia (UTC) -- avança +dt por frame em process(); usada só para setETA() dos steerpoints"],
  feba: ["[N E]", "linha de frente (forward edge of battle area) -- coordenadas puras; nenhum consumidor NESTA classe as lê de volta para navegação"],
  bullseye: ["Bullseye", "referência de reporte tático -- recomputada a cada process(), independente da rota"],
};

function NavigationReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Navigation"];
  const insEntry = MODEL["Ins"];
  const gpsEntry = MODEL["Gps"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Navigation</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models::System</span>
          <span className="mx-chip">factory: "Navigation"</span>
          <span className="mx-chip">Ins / Gps = mesma classe, sem override</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Navigation")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          <code className="mx-mono">Navigation</code> é um <code className="mx-mono">System</code> -- roda na{" "}
          <b>fase 3</b> do frame de tempo crítico, junto com sensores/decisão. Não CALCULA nada de novo: agrega um{" "}
          <code className="mx-mono">Route</code> e, a cada frame, copia para si o que o steerpoint "to" já calculou
          (<code className="mx-mono">Steerpoint::compute()</code>, que roda antes, no laço de{" "}
          <code className="mx-mono">Route::updateData()</code> em BACKGROUND). É a peça que{" "}
          <code className="mx-mono">Autopilot</code>/<code className="mx-mono">NavigateAction</code> de fato
          consultam via <code className="mx-mono">getTrueBrgDeg()</code>/<code className="mx-mono">isNavSteeringValid()</code>.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Navigation → System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Navigation" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>"Copiar, não calcular"</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">updateNavSteering()</code> faz oito atribuições
                (<code className="mx-mono">setTrueBrgDeg(to-&gt;getTrueBrgDeg())</code>, etc.) e nada mais -- toda a
                geodésia real já rodou dentro de <code className="mx-mono">Steerpoint::compute()</code>. Isso é
                deliberado: <code className="mx-mono">Ins</code>/<code className="mx-mono">Gps</code> existem
                justamente para permitir SOBRESCREVER este método com um sensor de verdade (com ruído, drift, taxa
                própria) sem mexer em quem consome os dados -- neste fork, porém, nenhuma delas sobrescreve nada.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Ins / Gps -- 0 slots, 0 overrides</div>
              <div className="mx-slotgrid">
                <div className="mx-slot"><span>Ins</span><span>{insEntry ? `${insEntry.own} slots próprios, ${insEntry.ov.length} overrides` : "0 slots próprios, 0 overrides"}</span></div>
                <div className="mx-slot"><span>Gps</span><span>{gpsEntry ? `${gpsEntry.own} slots próprios, ${gpsEntry.ov.length} overrides` : "0 slots próprios, 0 overrides"}</span></div>
              </div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "8px 0 0" }}>
                Neste fork, as três classes são <b>funcionalmente idênticas</b>. Declarar{" "}
                <code className="mx-mono">( Gps )</code> em vez de <code className="mx-mono">( Navigation )</code>{" "}
                muda só o nome de fábrica no <code className="mx-mono">.edl</code> -- nenhum comportamento.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Onde isso roda no frame</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">process(dt)</code> é a <b>fase 3</b> -- a mesma fase em que a decisão
                (árvore de comportamento/UBF) roda. Dentro dela: posição/atitude/velocidade próprias, o UTC, o{" "}
                <code className="mx-mono">Route</code> (<code className="mx-mono">tcFrame()</code>, ainda fase 3),
                o bullseye, e só por último <code className="mx-mono">updateNavSteering()</code> -- ou seja, os
                dados de guiagem que a decisão desta MESMA fase vai ler já estão atualizados quando ela roda, desde
                que <code className="mx-mono">Navigation</code> apareça ANTES do agente na lista de{" "}
                <code className="mx-mono">components:</code> do player.
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div className="mx-card">
            <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Navigation</div>
            <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>
              {entry ? entry.own : 4} slots próprios -- Ins/Gps não acrescentam nenhum.
            </p>
            <div className="mx-slotgrid">
              {(entry ? entry.sl : Object.keys(REF_NAVIGATION_SLOT_DOCS)).map((s) => (
                <div className="mx-slot" key={s}>
                  <span>{s}{REF_NAVIGATION_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_NAVIGATION_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                  <span style={{ maxWidth: 360 }}>{REF_NAVIGATION_SLOT_DOCS[s] ? REF_NAVIGATION_SLOT_DOCS[s][1] : ""}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Navigation::process(dt) -- fase 3, na íntegra</span><span>C++</span></div>
              {renderNavSnippet("Navigation::process (fase 3)")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Navigation::updateNavSteering() -- oito atribuições, zero cálculo</span><span>C++</span></div>
              {renderNavSnippet("Navigation::updateNavSteering (native)")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ---------------------------- Autopilot ---------------------------- */

const REF_AUTOPILOT_SLOT_DOCS = {
  navMode: ["bool", "liga processModeNavigation() -- RE-LATCHED todo frame por modeManager(), mesmo que algo desligue \"na mão\" entre um frame e outro"],
  holdAltitude: ["Distance", "altitude retida quando altitudeHoldMode -- default: altitude atual do player, no reset()"],
  altitudeHoldMode: ["bool", "hold de altitude -- forçado true quando navMode liga"],
  holdVelocityKts: ["kt", "velocidade retida -- default: velocidade atual do player"],
  velocityHoldMode: ["bool", "hold de velocidade -- forçado true quando navMode liga"],
  holdHeading: ["Angle", "rumo retido -- default: rumo atual do player"],
  headingHoldMode: ["bool", "hold de rumo -- forçado true quando navMode liga"],
  loiterMode: ["bool", "modo de espera (hipódromo) -- mutuamente exclusivo com navMode/followTheLead em modeManager()"],
  loiterPatternLength: ["NM", "comprimento da perna reta do hipódromo"],
  loiterPatternCcwFlag: ["bool", "sentido anti-horário do hipódromo (default: horário)"],
  leadFollowingDistanceTrail: ["m", "distância atrás do líder, modo \"seguir líder\""],
  leadFollowingDistanceRight: ["m", "distância à direita do líder"],
  leadFollowingDeltaAltitude: ["m", "altitude acima/abaixo do líder"],
  leadPlayerName: ["Identifier", "nome do player líder -- setar DEPOIS, não antes, de followTheLeadMode"],
  followTheLeadMode: ["bool", "modo \"seguir líder\" -- maior prioridade em modeManager(), acima de loiter e nav"],
  maxRateOfTurnDps: ["deg/s", "2º parâmetro de DynamicsModel::setCommandedHeadingD() -- RacModel E JSBSimModel (os dois nativos deste fork) declaram esse parâmetro SEM NOME e o descartam. Slot sem efeito nos dois dynamics model shipped aqui"],
  maxBankAngle: ["deg", "3º parâmetro do mesmo setCommandedHeadingD() -- mesmo destino: descartado nos dois"],
  maxClimbRateFpm: ["ft/min", "variante em pés/min de maxClimbRateMps -- mesmo parâmetro de setCommandedAltitude()"],
  maxClimbRateMps: ["m/s", "2º parâmetro de setCommandedAltitude() -- RacModel/JSBSimModel também o declaram sem nome e descartam"],
  maxPitchAngle: ["deg", "3º parâmetro de setCommandedAltitude() -- idem, descartado nos dois"],
  loiterPatternTime: ["s", "tempo da perna reta, alternativa a loiterPatternLength"],
  maxAcceleration: ["NPS", "2º parâmetro de setCommandedVelocityKts() -- aqui o parâmetro TEM nome (vNps) nos dois DynamicsModel, mas nenhum dos dois corpos o referencia. Dead code sutilmente diferente do de cima: não é descartado por assinatura, é ignorado no corpo"],
};

function AutopilotReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Autopilot"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Autopilot</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models::Pilot</span>
          <span className="mx-chip">factory: "Autopilot"</span>
          <span className="mx-chip">navMode | hold modes | loiter | follow-the-lead</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Autopilot")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          O piloto automático NATIVO -- quatro modos mutuamente exclusivos escolhidos a cada frame por{" "}
          <code className="mx-mono">modeManager()</code> (follow-the-lead &gt; loiter &gt; nav), mais os três
          "hold" (rumo/altitude/velocidade) que os controladores de baixo nível de fato atuam. Este projeto NÃO
          usa <code className="mx-mono">navMode</code> em produção -- todo cenário real pilota via árvore de
          comportamento, com <code className="mx-mono">navMode: false</code>. O único lugar onde
          <code className="mx-mono"> navMode: true</code> chegou a ser exercitado de propósito foi{" "}
          <code className="mx-mono">tests/fixtures/full-systems-nav</code>, e o resultado medido lá é o que motivou
          este próprio piloto rate-limited em <code className="mx-mono">NavigateAction.cpp</code> -- ver o laboratório abaixo.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Autopilot → Pilot → System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Autopilot" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "lab"} data-on={tab === "lab" ? 1 : 0} onClick={() => setTab("lab")}>Laboratório de navegação</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>modeManager() -- re-latch incondicional</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                A PRIMEIRA linha de <code className="mx-mono">modeManager()</code> é{" "}
                <code className="mx-mono">setNavMode(isNavModeOn())</code> -- todo frame, mesmo que ninguém tenha
                pedido nada. O comentário nativo chama isso de "re-latch": se algo desligasse{" "}
                <code className="mx-mono">navMode</code> só chamando <code className="mx-mono">setHeadingHoldMode
                (false)</code> direto (sem tocar <code className="mx-mono">navMode</code> em si), o próximo frame
                reimporia os três hold modes de volta -- <code className="mx-mono">navMode</code> é a fonte da
                verdade, os hold modes individuais só refletem o que ele mandou.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>processModeNavigation() -- perseguição pura, zero avanço</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Quando <code className="mx-mono">navMode</code> está ligado, o comando de rumo é{" "}
                <code className="mx-mono">setCommandedHeadingD(nav-&gt;getTrueBrgDeg())</code> -- a marcação BRUTA,
                recalculada a cada frame, sem filtro/rampa nenhum. Altitude e velocidade vêm do próprio steerpoint
                (<code className="mx-mono">getCmdAltitudeFt()</code>/<code className="mx-mono">getCmdAirspeedKts()</code>).
                É pure pursuit com ganho instantâneo -- exatamente o padrão que diverge perto de um alvo/waypoint
                quando a taxa de variação da marcação supera a taxa de guinada disponível (mesma classe de
                instabilidade documentada para <code className="mx-mono">domain::pursuit()</code> na aba step-by-step).
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Os limites de manobra do Autopilot -- e por que não limitam nada aqui</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                <code className="mx-mono">maxRateOfTurnDps</code>/<code className="mx-mono">maxBankAngle</code> (rumo)
                e <code className="mx-mono">maxClimbRateMps</code>/<code className="mx-mono">maxPitchAngle</code> (altitude)
                são passados para o <code className="mx-mono">DynamicsModel</code> a cada frame
                (<code className="mx-mono">headingController()</code>/<code className="mx-mono">altitudeController()</code>).
                O PRÓPRIO cabeçalho de <code className="mx-mono">Autopilot.hpp</code> já avisa: "Limiting the
                autopilot inputs ... will work if the dynamics model can support the limits as well. If not ...
                these inputs will have no effect." Medido neste fork: <b>nenhum dos dois</b>{" "}
                <code className="mx-mono">DynamicsModel</code> shipped (<code className="mx-mono">RacModel</code>,{" "}
                <code className="mx-mono">JSBSimModel</code>) suporta -- os dois declaram os parâmetros de rumo/altitude
                SEM NOME (descartados em tempo de compilação) e o de velocidade (<code className="mx-mono">maxAcceleration</code>)
                COM nome mas nunca referenciado no corpo (descartado em runtime). As cinco slots de limite do Autopilot
                são, hoje, decorativas nos dois dynamics model deste fork -- ver Código-fonte.
              </p>
            </div>
          </div>
        )}

        {tab === "lab" && <AutopilotNavLab />}

        {tab === "slots" && (
          <div className="mx-card">
            <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Autopilot</div>
            <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{entry ? entry.own : 22} slots próprios.</p>
            <div className="mx-slotgrid">
              {(entry ? entry.sl : Object.keys(REF_AUTOPILOT_SLOT_DOCS)).map((s) => (
                <div className="mx-slot" key={s}>
                  <span>{s}{REF_AUTOPILOT_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_AUTOPILOT_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                  <span style={{ maxWidth: 380 }}>{REF_AUTOPILOT_SLOT_DOCS[s] ? REF_AUTOPILOT_SLOT_DOCS[s][1] : ""}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Autopilot::modeManager() -- re-latch, na íntegra</span><span>C++</span></div>
              {renderNavSnippet("Autopilot::modeManager (native)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Autopilot::processModeNavigation() -- na íntegra</span><span>C++</span></div>
              {renderNavSnippet("Autopilot::processModeNavigation (native)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Autopilot::setNavMode() -- o que acontece ao DESLIGAR</span><span>C++</span></div>
              {renderNavSnippet("Autopilot::setNavMode (native)")}
              <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 8, marginBottom: 0 }}>
                Desligar <code className="mx-mono">navMode</code> trava rumo/altitude/velocidade comandados no valor
                ATUAL do player -- não zera nem mantém o último comando de navegação. É o que evita um "salto" para
                um alvo velho se alguém desligar o modo no meio do voo.
              </p>
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Autopilot::headingController() -- onde os limites de manobra SAEM do Autopilot</span><span>C++</span></div>
              {renderNavSnippet("Autopilot::headingController (native)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">RacModel::setCommandedHeadingD() / JSBSimModel::setCommandedHeadingD() -- onde eles ENTRAM</span><span>C++</span></div>
              <div style={{ display: "flex", gap: 12, flexWrap: "wrap", alignItems: "flex-start" }}>
                <div style={{ flex: "1 1 220px" }}>{renderNavSnippet("RacModel::setCommandedHeadingD (native)")}</div>
                <div style={{ flex: "1 1 260px" }}>{renderNavSnippet("JSBSimModel::setCommandedHeadingD (native)")}</div>
              </div>
              <p className="mx-warn" style={{ marginTop: 10, marginBottom: 0 }}>
                Os dois últimos parâmetros de <code className="mx-mono">setCommandedHeadingD(hdg, maxTurnRateDps,
                maxBankAngleDegs)</code> chegam sem NOME nas duas implementações -- não é possível referenciá-los
                mesmo por engano; <code className="mx-mono">maxTurnRateDps</code>/<code className="mx-mono">maxBankAngleDegs</code>{" "}
                do <code className="mx-mono">Autopilot</code> não têm efeito algum na taxa de giro real da aeronave
                nestes dois <code className="mx-mono">DynamicsModel</code>.
              </p>
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">NavigateAction (este projeto) -- a resposta ao pure pursuit divergente</span><span>C++</span></div>
              {renderNavSnippet("NavigateAction (constante)")}
              {renderNavSnippet("NavigateAction::tick (próprio deste projeto)")}
              <p style={{ fontSize: 11.5, color: "var(--muted)", marginTop: 8, marginBottom: 0 }}>
                Lê os MESMOS dados de <code className="mx-mono">Route</code>/<code className="mx-mono">Steerpoint</code> que{" "}
                <code className="mx-mono">Autopilot::processModeNavigation()</code> consultaria -- não reimplementa
                navegação, só amortece o COMANDO de rumo a no máximo 3°/s (contra os 6°/s de{" "}
                <code className="mx-mono">maxRateOfTurnDps</code> da AERONAVE, que -- como visto acima -- nem chega a
                ser aplicado pelo <code className="mx-mono">DynamicsModel</code>). Ver o laboratório para o efeito,
                medido com dados reais de rota.
              </p>
            </div>
          </>
        )}
      </div>
    </div>
  );
}


/* ====================== Referência -- Player / System (base de todo player/subsistema) ============
 * Terceira leva da enciclopédia (a primeira foi só Missile; a segunda, Steerpoint/Route/
 * Navigation/Autopilot). Mesmo padrão das duas mais simples (Steerpoint/Navigation): hero +
 * Visão geral / Slots / Código-fonte, SEM laboratório -- Player/System não têm uma única
 * grandeza numérica pra simular (a fase do frame já tem sua própria aba, "Simulação"; o ciclo
 * de vida do Mode já aparece na aba Missile). O que falta documentar aqui é ESTRUTURAL: o
 * despacho de fase em DOIS NÍVEIS (Player só resolve a fase 0 por si; cada System resolve as
 * quatro por si, um nível abaixo), os "10 papéis" de Player resolvidos por TIPO -- não por nome
 * de slot --, e o freeze em cascata que a seção libs/xclock do CLAUDE.md já documenta de fora;
 * aqui é a fonte, com arquivo:linha.
 * ==================================================================================== */

const PLAYER_SNIPPETS = {
  "Player::updateTC (despacho de fase -- so' a fase 0 e' PROPRIA)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 528,
    trunc: false,
    lines: [
      "void Player::updateTC(const double dt0)",
      "{",
      "   // Make sure we've loaded our system pointers",
      "   if (loadSysPtrs) {",
      "      updateSystemPointers();",
      "      loadSysPtrs = false;",
      "   }",
      "",
      "   if (mode == ACTIVE || mode == PRE_RELEASE) {",
      "",
      "      // ---",
      "      // Time-out requests for reflections of RF emissions hitting us",
      "      // ---",
      "      for (unsigned int i = 0; i < MAX_RF_REFLECTIONS; i++) {",
      "         if (rfReflect[i] != nullptr) {",
      "            rfReflectTimer[i] -= dt0;",
      "            if (rfReflectTimer[i] <= 0) {",
      "               // Clear the request",
      "               rfReflect[i]->unref();",
      "               rfReflect[i] = nullptr;",
      "            }",
      "         }",
      "      }",
      "",
      "      // ---",
      "      // Delta time -- real or frozen?",
      "      // ---",
      "      double dt{dt0};",
      "      if (isFrozen()) dt = 0.0;",
      "",
      "      // ---",
      "      // Compute delta time for modules running every fourth phase",
      "      // ---",
      "      double dt4{dt * 4.0};     // Delta time for items running every fourth phase",
      "      switch (getWorldModel()->phase()) {",
      "",
      "         // Phase 0 -- Dynamics",
      "         case 0 : {",
      "            // Our dynamics",
      "            dynamics(dt4);",
      "",
      "            // Log our player's dynamic data just after its been updated ...",
      "            if (dataLogTime > 0.0) {",
      "               // When we have a data logging time, update the timer",
      "               dataLogTimer -= dt4;",
      "               if (dataLogTimer <= 0.0) {",
      "                  // At timeout, log the player's data and ...",
      "",
      "                  BEGIN_RECORD_DATA_SAMPLE( getWorldModel()->getDataRecorder(), REID_PLAYER_DATA )",
      "                     SAMPLE_1_OBJECT( this )",
      "                  END_RECORD_DATA_SAMPLE()",
      "",
      "                  // reset the timer.",
      "                  dataLogTimer = dataLogTime;",
      "               }",
      "            }",
      "",
      "            // Update signatures after we've updated our dynamics",
      "            if (signature != nullptr) signature->updateTC(dt4);",
      "            if (irSignature != nullptr) irSignature->updateTC(dt4);",
      "         }",
      "         break;",
      "",
      "         // Phase 1 -- Sensors transmit",
      "         case 1 :",
      "         break;",
      "",
      "         // Phase 2 -- Sensors Receive",
      "         case 2 :",
      "         break;",
      "",
      "         // Phase 3 -- PDL and other logic",
      "         case 3 :",
      "         break;",
      "",
      "      }",
      "",
      "      // ---",
      "      // Notes:",
      "      //  a) Remember that our subsystems in the components list (e.g., pilot, nav,",
      "      //     sms and obc) are updated by our call to BaseClass:updateTC()",
      "      //  b) We're calling BaseClass::updateTC() class because we want to update",
      "      //     our player dynamics, etc before our subsystems.",
      "      // ---",
      "      BaseClass::updateTC(dt);",
      "   }",
      "}",
    ],
  },
  "Player::dynamics (local vs. rede)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 2764,
    trunc: false,
    lines: [
      "void Player::dynamics(const double dt)",
      "{",
      "   // ---",
      "   // Local player ...",
      "   // ---",
      "   if (isLocalPlayer()) {",
      "      // Update the external dynamics model (if any)",
      "      if (getDynamicsModel() != nullptr) {",
      "         // If we have a dynamics model ...",
      "         getDynamicsModel()->freeze( isFrozen() );",
      "         getDynamicsModel()->dynamics(dt);",
      "      }",
      "",
      "      // Update our position",
      "      positionUpdate(dt);",
      "",
      "      if (getNib() != nullptr || true) {",
      "         if (!syncState1Ready) {",
      "            syncState1.setGeocPosition(getGeocPosition());",
      "            syncState1.setGeocVelocity(getGeocVelocity());",
      "            syncState1.setGeocAcceleration(getGeocAcceleration());",
      "            syncState1.setGeocEulerAngles(getGeocEulerAngles());",
      "            syncState1.setAngularVelocities(getAngularVelocities());",
      "            syncState1.setTimeExec(getWorldModel()->getExecTimeSec());",
      "            syncState1.setTimeUtc(getWorldModel()->getSysTimeOfDay());",
      "            syncState1.setValid(true);",
      "            syncState1Ready = true;",
      "            syncState2Ready = false;",
      "            //std::cout << \"Set syncState1\" << std::endl;",
      "         } else {",
      "            syncState2.setGeocPosition(getGeocPosition());",
      "            syncState2.setGeocVelocity(getGeocVelocity());",
      "            syncState2.setGeocAcceleration(getGeocAcceleration());",
      "            syncState2.setGeocEulerAngles(getGeocEulerAngles());",
      "            syncState2.setAngularVelocities(getAngularVelocities());",
      "            syncState2.setTimeExec(getWorldModel()->getExecTimeSec());",
      "            syncState2.setTimeUtc(getWorldModel()->getSysTimeOfDay());",
      "            syncState2.setValid(true);",
      "            syncState2Ready = true;",
      "            syncState1Ready = false;",
      "            //std::cout << \"Set syncState2\" << std::endl;",
      "         }",
      "      }",
      "",
      "      // ---",
      "      // Check for ground collisions",
      "      // ---",
      "      if (getAltitudeAgl() < 0.0 && isLocalPlayer() && isMajorType(AIR_VEHICLE | WEAPON | SPACE_VEHICLE)) {",
      "         // We're below the ground!",
      "         this->event(CRASH_EVENT,nullptr);",
      "      }",
      "   }",
      "",
      "   // ---",
      "   // Network I-player ...",
      "   // ---",
      "   else {",
      "      // dead reckoning our position and orientation",
      "      deadReckonPosition(dt);",
      "   }",
      "}",
    ],
  },
  "Player::updateSystemPointers (os 10 papeis)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 3138,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// updateSystemPointers() -- update all of our system (component) pointers",
      "//------------------------------------------------------------------------------",
      "void Player::updateSystemPointers()",
      "{",
      "   // ---",
      "   // Set base::Pair pointers for our primary systems located in our list of subcomponents",
      "   // ---",
      "   loadSysPtrs = false;",
      "   setDynamicsModel( findByType(typeid(DynamicsModel)) );",
      "   setDatalink( findByType(typeid(Datalink)) );",
      "   setGimbal( findByType(typeid(Gimbal)) );",
      "   setIrSystem( findByType(typeid(IrSystem)) );",
      "   setNavigation( findByType(typeid(Navigation)) );",
      "   setOnboardComputer( findByType(typeid(OnboardComputer)) );",
      "   setPilot( findByType(typeid(Pilot)) );",
      "   setRadio( findByType(typeid(Radio)) );",
      "   setSensor( findByType(typeid(RfSensor)) );",
      "   setStoresMgr( findByType(typeid(StoresMgr)) );",
      "}",
    ],
  },
  "Player::isFrozen (cascata ate' a Simulation)": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 442,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// isFrozen() -- checks both player's freeze flag and the simulation's freeze flag",
      "//------------------------------------------------------------------------------",
      "bool Player::isFrozen() const",
      "{",
      "   bool frz{BaseClass::isFrozen()};",
      "   if (!frz && sim != nullptr) frz = sim->isFrozen();",
      "   return frz;",
      "}",
    ],
  },
};

const playerSnip = (key) => (key ? PLAYER_SNIPPETS[key] || null : null);

function renderPlayerSnippet(key) {
  const snip = playerSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_PLAYER_SLOT_DOCS = {
  initXPos: ["Distance|Number", "posição inicial (+norte) -- só usada se a posição inicial escolhida for LOCAL (as três formas -- LOCAL/GEOD/WORLD -- são mutuamente exclusivas)"],
  initYPos: ["Distance|Number", "posição inicial (+leste) -- idem"],
  initAlt: ["Distance|Number", "altitude inicial (HAE, +acima) -- usada pelas três formas de posição inicial"],
  initPosition: ["List", "atalho [ norte leste baixo ] de uma vez -- o próprio header já avisa: será removido numa versão futura"],
  initLatitude: ["LatLon|Angle|Number", "latitude inicial -- segunda forma de posição, alternativa a initXPos/initYPos"],
  initLongitude: ["LatLon|Angle|Number", "longitude inicial -- idem"],
  initGeocentric: ["List", "posição inicial em ECEF [ x y z ], metros -- terceira forma, independente das duas acima"],
  initRoll: ["Angle|Number", "rolagem inicial (radianos por padrão)"],
  initPitch: ["Angle|Number", "arfagem inicial"],
  initHeading: ["Angle|Number", "rumo inicial"],
  initEuler: ["List", "atalho [ roll pitch yaw ] de uma vez, radianos"],
  initVelocity: ["Number", "velocidade total inicial, m/s -- vira velocidade de corpo (ua) no reset()"],
  initVelocityKts: ["Number", "idem, em nós"],
  type: ["String", "string livre (\"F-16A\", \"Tank\"...) -- é o que TacviewOutput usa para casar typeMap/colorMap/modelMap quando REID_NEW_PLAYER nunca chega (ver a seção libs/xtacview do CLAUDE.md)"],
  side: ["String", "BLUE/RED/YELLOW/CYAN/GRAY/WHITE (enum Side, bitmask) -- default GRAY"],
  signature: ["RfSignature", "assinatura de RCS (ex.: SigSphere, SigPlate)"],
  irSignature: ["IrSignature", "assinatura infravermelha"],
  camouflageType: ["Number", "inteiro definido pelo usuário, 0 = nenhum -- consultado por SigSwitch para alternar assinatura"],
  terrainElevReq: ["Number(bool)", "true = a elevação de terreno vem do sistema de imagem gerada (IG), NUNCA do banco DTED/SRTM local -- ver updateElevation()"],
  interpolateTerrain: ["Number(bool)", "interpola a elevação entre postes do DTED/SRTM local, em vez do valor bruto da célula"],
  terrainOffset: ["Distance", "offset de ground clamping do terreno até o CG do player, metros"],
  positionFreeze: ["Number(bool)", "congela X/Y (LOCAL) ou lat/lon (GEOD) -- ver altitudeFreeze para o eixo vertical"],
  altitudeFreeze: ["Number(bool)", "congela Z (LOCAL) ou altitude (GEOD)"],
  attitudeFreeze: ["Number(bool)", "congela atitude -- quem de fato precisa respeitar isso é o dynamicsModel, não Player sozinho"],
  fuelFreeze: ["Number(bool)", "congela consumo de combustível -- também depende do dynamicsModel"],
  crashOverride: ["Number(bool)", "ignora CRASH_EVENT -- crashNotification()/collisionNotification() ainda gravam o REID, só não mudam mode nem propagam KILL_EVENT"],
  killOverride: ["Number(bool)", "ignora KILL_EVENT -- killedNotification() ainda grava REID_PLAYER_KILLED, só não muda dano/mode"],
  killRemoval: ["Number(bool)", "default false -- SEM ele, killedNotification() nunca faz setMode(KILLED), mesmo com o evento chegando de verdade (ver a trilha step-by-step, achado sobre a4_target)"],
  enableNetOutput: ["Number(bool)", "habilita a saída de rede (DIS/HLA) deste player -- default true"],
  dataLogTime: ["Time", "intervalo entre amostras REID_PLAYER_DATA para o gravador -- default 0 (zero = nunca loga), ver a armadilha 1 de libs/xtacview"],
  testRollRate: ["Angle", "taxa de rolagem de TESTE (corpo ou Euler, ver testBodyAxis) -- só faz sentido sem dynamicsModel"],
  testPitchRate: ["Angle", "idem, arfagem"],
  testYawRate: ["Angle", "idem, guinada (rumo)"],
  testBodyAxis: ["Number(bool)", "true = as três taxas de teste acima são do CORPO -- false (default) = são taxas de Euler"],
  useCoordSys: ["String", "força o sistema usado para ATUALIZAR a posição (WORLD/GEOD/LOCAL) -- por default é herdado de qual das três formas de posição inicial foi de fato usada"],
};

function PlayerReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Player"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Player</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "Player"</span>
          <span className="mx-chip">35 slots próprios · 314 métodos</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Player")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          A base de TODO player da simulação (aeronave, veículo terrestre, navio, prédio, forma de vida,
          veículo espacial...) -- apesar do comentário do próprio header chamá-la de "interface abstrata"
          (ver o achado mais abaixo), é uma classe CONCRETA: um <code className="mx-mono">( Player )</code> nu
          já monta e roda, só sem comportamento físico nenhum (<code className="mx-mono">getMajorType()</code> devolve
          apenas <code className="mx-mono">GENERIC</code>). Toda subclasse concreta (<code className="mx-mono">AirVehicle</code>,{" "}
          <code className="mx-mono">GroundVehicle</code>, <code className="mx-mono">Ship</code>...) sobrescreve pouco
          mais que <code className="mx-mono">getMajorType()</code>/<code className="mx-mono">getGrossWeight()</code> --
          a mecânica inteira de posição/atitude/freeze/fase já mora aqui.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Player → AbstractPlayer → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Player" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>updateTC() -- despacho de fase EM DOIS NÍVEIS</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">Player::updateTC()</code> só faz algo de verdade na <b>fase 0</b>{" "}
                (<code className="mx-mono">dynamics()</code> + log de <code className="mx-mono">REID_PLAYER_DATA</code> +{" "}
                <code className="mx-mono">updateTC()</code> das assinaturas) -- as fases 1/2/3 são vazias NA PRÓPRIA
                classe. Quem de fato processa cada fase são os SUBCOMPONENTES (todo <code className="mx-mono">System</code>{" "}
                em <code className="mx-mono">components:</code>), cada um rodando o MESMO switch de novo, sozinho,
                dentro do próprio <code className="mx-mono">updateTC()</code> (ver a aba System).{" "}
                <code className="mx-mono">BaseClass::updateTC(dt)</code> roda incondicionalmente no fim -- é ele quem
                desce a cada subcomponente, EM TODA FASE, independente de qual <code className="mx-mono">case</code> bateu
                aqui em cima. O <code className="mx-mono">dt</code> que chega já é 1/4 do <code className="mx-mono">dt</code>{" "}
                do frame (<code className="mx-mono">Simulation::updateTcPlayerList()</code> divide por 4 antes de
                chamar); <code className="mx-mono">dt4 = dt * 4.0</code> multiplica de volta, pro módulo "que roda a cada
                quarta fase" receber o <code className="mx-mono">dt</code> do FRAME inteiro.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>dynamics() -- local escolhe a física, rede nunca toca nela</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">isLocalPlayer()</code> decide tudo: local roda{" "}
                <code className="mx-mono">dynamicsModel-&gt;dynamics(dt)</code> (se houver) e{" "}
                <code className="mx-mono">positionUpdate()</code> -- integração trapezoidal sobre LOCAL/GEOD/WORLD,
                com ground clamping para <code className="mx-mono">GROUND_VEHICLE|SHIP|BUILDING|LIFE_FORM</code>. Em
                rede, <code className="mx-mono">dynamics()</code> NUNCA toca nenhum <code className="mx-mono">dynamicsModel</code> --
                só <code className="mx-mono">deadReckonPosition()</code>, que extrapola posição/atitude a partir do
                último PDU (<code className="mx-mono">nib-&gt;updateDeadReckoning()</code>), com o MESMO teste de ground
                clamping. É a peça que fecha por que um fantasma DIS (ver <code className="mx-mono">src/poc/dis/bandit</code>{" "}
                no CLAUDE.md) nunca precisa de <code className="mx-mono">JSBSimModel</code>/<code className="mx-mono">Autopilot</code>{" "}
                do lado receptor -- a física dele nunca roda ali, só a extrapolação.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado, não redescobrir:</b> <code className="mx-mono">CRASH_EVENT</code> por altitude negativa só
                dispara para 3 dos 8 <code className="mx-mono">MajorType</code> -- dentro de{" "}
                <code className="mx-mono">dynamics()</code>, a condição é{" "}
                <code className="mx-mono">getAltitudeAgl() &lt; 0.0 &amp;&amp; isMajorType(AIR_VEHICLE|WEAPON|SPACE_VEHICLE)</code>{" "}
                (Player.cpp:2811). <code className="mx-mono">GROUND_VEHICLE|SHIP|BUILDING|LIFE_FORM</code> nunca
                crasham por AQUI -- são ground-clamped por <code className="mx-mono">positionUpdate()</code>/{" "}
                <code className="mx-mono">deadReckonPosition()</code> em vez disso.{" "}
                <code className="mx-mono">Player::getMajorType()</code> puro devolve sempre{" "}
                <code className="mx-mono">GENERIC</code> (0x01, Player.cpp:645-648) -- um{" "}
                <code className="mx-mono">( Player )</code> sem subclasse nenhuma não pertence a NENHUM dos dois
                grupos: não é ground-clamped e nunca dispara <code className="mx-mono">CRASH_EVENT</code> por altitude.
                Atravessaria o chão para sempre, sem aviso nenhum.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>updateSystemPointers() -- os 10 papéis, resolvidos por TIPO</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                Os dez papéis primários (<code className="mx-mono">dynamicsModel/datalink/gimbal/irSystem/
                navigation/onboardComputer/pilot/radio/sensor/storesMgr</code>) são todos declarados como{" "}
                <code className="mx-mono">base::Pair*</code> genérico no header -- o tipo real só aparece dentro do
                CORPO do setter, via <code className="mx-mono">findByType(typeid(X))</code>. É por isso que o nome do
                slot EDL (<code className="mx-mono">dynamicsModel:</code>) é cosmético: quem resolve o papel é o
                TIPO C++ do objeto, nunca a chave usada no arquivo. <code className="mx-mono">loadSysPtrs</code>{" "}
                (setado por <code className="mx-mono">processComponents()</code> sempre que a lista de{" "}
                <code className="mx-mono">components:</code> muda) só é consumido no TOPO de{" "}
                <code className="mx-mono">updateTC()</code>/<code className="mx-mono">reset()</code> -- trocar um
                subcomponente NO MEIO de um frame não reembaralha os ponteiros até o próximo ciclo.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>isFrozen() -- onde a cascata de freeze começa</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">Player::isFrozen()</code> testa o próprio flag OU{" "}
                <code className="mx-mono">sim-&gt;isFrozen()</code> (445-450). Dentro de{" "}
                <code className="mx-mono">updateTC()</code>, congelar não pula a chamada -- ela roda igual, só com{" "}
                <code className="mx-mono">dt=0</code>, o que mantém os subcomponentes "vivos" (recebendo tick, sem
                avançar nada) durante a pausa. É o MESMO padrão, um nível abaixo, que{" "}
                <code className="mx-mono">System::isFrozen()</code> usa contra o próprio <code className="mx-mono">ownship</code>{" "}
                (ver a aba System) -- essa cadeia (Simulation → Player → System) é o que permite ao{" "}
                <code className="mx-mono">setPaused()</code> de <code className="mx-mono">libs/xclock</code> bastar
                tocar só a <code className="mx-mono">Simulation</code>.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado, não redescobrir:</b> o comentário do header (Player.hpp, linha 55) documenta{" "}
                <code className="mx-mono">Factory name: AbstractPlayer</code> -- mas{" "}
                <code className="mx-mono">AbstractPlayer</code> é uma classe DIFERENTE (a interface, em{" "}
                <code className="mx-mono">mixr::simulation</code>, da qual <code className="mx-mono">Player</code>{" "}
                deriva), e <code className="mx-mono">IMPLEMENT_SUBCLASS(Player, "Player")</code> (Player.cpp:52)
                registra o nome de fábrica REAL: <code className="mx-mono">"Player"</code>. O próprio{" "}
                <code className="mx-mono">models/BUILT-IN.md</code> deste repositório já registra o achado --
                "apesar do nome, é concreta e alcançável via EDL". Não é bug do framework, é o comentário que
                envelheceu: ler o <code className="mx-mono">.cpp</code>, não o header.
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div className="mx-card">
            <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Player</div>
            <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{entry ? entry.own : 35} slots próprios.</p>
            <div className="mx-slotgrid">
              {(entry ? entry.sl : Object.keys(REF_PLAYER_SLOT_DOCS)).map((s) => (
                <div className="mx-slot" key={s}>
                  <span>{s}{REF_PLAYER_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_PLAYER_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                  <span style={{ maxWidth: 380 }}>{REF_PLAYER_SLOT_DOCS[s] ? REF_PLAYER_SLOT_DOCS[s][1] : ""}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Player::updateTC() -- o despacho de fase, na íntegra</span><span>C++</span></div>
              {renderPlayerSnippet("Player::updateTC (despacho de fase -- so' a fase 0 e' PROPRIA)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Player::dynamics() -- local (positionUpdate) vs. rede (deadReckonPosition)</span><span>C++</span></div>
              {renderPlayerSnippet("Player::dynamics (local vs. rede)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Player::updateSystemPointers() -- na íntegra</span><span>C++</span></div>
              {renderPlayerSnippet("Player::updateSystemPointers (os 10 papeis)")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Player::isFrozen()</span><span>C++</span></div>
              {renderPlayerSnippet("Player::isFrozen (cascata ate' a Simulation)")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ---------------------------- System ---------------------------- */

const SYSTEM_SNIPPETS = {
  "System::updateTC (o mesmo switch, um nivel abaixo)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 81,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// updateTC() -- update time critical stuff here",
      "//------------------------------------------------------------------------------",
      "void System::updateTC(const double dt0)",
      "{",
      "   // We're nothing without an ownship ...",
      "   if (ownship == nullptr && getOwnship() == nullptr) return;",
      "",
      "   // ---",
      "   // Delta time",
      "   // ---",
      "",
      "   // real or frozen?",
      "   double dt{dt0};",
      "   if (isFrozen()) dt = 0.0;",
      "",
      "   // Delta time for methods that are running every fourth phase",
      "   double dt4{dt * 4.0};",
      "",
      "   // ---",
      "   // Four phases per frame",
      "   // ---",
      "   WorldModel* sim{ownship->getWorldModel()};",
      "   if (sim == nullptr) return;",
      "",
      "   switch (sim->phase()) {",
      "",
      "      case 0 : // Frame0 --- Dynamics method",
      "         dynamics(dt4);",
      "         break;",
      "",
      "      case 1 : // Frame1 --- Transmit method",
      "         transmit(dt4);",
      "         break;",
      "",
      "      case 2 : // Frame2 --- Receive method",
      "         receive(dt4);",
      "         break;",
      "",
      "      case 3 : // Frame3 --- Process method",
      "         process(dt4);",
      "         break;",
      "   }",
      "",
      "   // ---",
      "   // Last, update our base class",
      "   // and use 'dt' because if we're frozen then so are our subcomponents.",
      "   // ---",
      "   BaseClass::updateTC(dt);",
      "}",
    ],
  },
  "System::isFrozen+reset+updateData (o mesmo guarda, tres vezes)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 49,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// isFrozen() -- checks both the system's freeze flag and its ownship's freeze flag",
      "//------------------------------------------------------------------------------",
      "bool System::isFrozen() const",
      "{",
      "   bool frz{BaseClass::isFrozen()};",
      "   if (!frz && ownship != nullptr) frz = ownship->isFrozen();",
      "   return frz;",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// reset() -- Reset parameters",
      "//------------------------------------------------------------------------------",
      "void System::reset()",
      "{",
      "   // We're nothing without an ownship ...",
      "   if (ownship == nullptr && getOwnship() == nullptr) return;",
      "",
      "   BaseClass::reset();",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// updateData() -- update background data here",
      "//------------------------------------------------------------------------------",
      "void System::updateData(const double dt)",
      "{",
      "   // We're nothing without an ownship ...",
      "   if (ownship == nullptr && getOwnship() == nullptr) return;",
      "",
      "   BaseClass::updateData(dt);",
      "}",
    ],
  },
  "System::getOwnship (lazy)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 196,
    trunc: false,
    lines: [
      "// Returns a pointer to our ownship player",
      "Player* System::getOwnship()",
      "{",
      "   if (ownship == nullptr) findOwnship();",
      "   return ownship;",
      "}",
      "",
      "// Returns a pointer to our ownship player (const version)",
      "const Player* System::getOwnship() const",
      "{",
      "   if (ownship == nullptr) {",
      "      (const_cast<System*>(this))->findOwnship();",
      "   }",
      "   return ownship;",
      "}",
    ],
  },
  "System::findOwnship (o mesmo findContainerByType de sempre)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 223,
    trunc: false,
    lines: [
      "// find our ownship",
      "bool System::findOwnship()",
      "{",
      "   if (ownship == nullptr) {",
      "      ownship = static_cast<Player*>(findContainerByType( typeid(Player) ));",
      "   }",
      "",
      "   return (ownship != nullptr);",
      "}",
    ],
  },
  "System::copyData (ownship nunca sobrevive a um clone)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 34,
    trunc: false,
    lines: [
      "void System::copyData(const System& org, const bool)",
      "{",
      "   BaseClass::copyData(org);",
      "",
      "   // Don't copy ownship, we'll need to reacquire it.",
      "   ownship = nullptr;",
      "",
      "   pwrSw = org.pwrSw;",
      "}",
    ],
  },
  "System::killedNotification (so' repassa)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 151,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// killedNotification() -- Default killed notification handler",
      "//------------------------------------------------------------------------------",
      "bool System::killedNotification(Player* const p)",
      "{",
      "   // Just let all of our subcomponents know that we were just killed",
      "   base::PairStream* subcomponents{getComponents()};",
      "   if(subcomponents != nullptr) {",
      "      for (base::List::Item* item = subcomponents->getFirstItem(); item != nullptr; item = item->getNext()) {",
      "         base::Pair* pair{static_cast<base::Pair*>(item->getValue())};",
      "         base::Component* sc{static_cast<base::Component*>(pair->object())};",
      "         sc->event(KILL_EVENT, p);",
      "      }",
      "      subcomponents->unref();",
      "      subcomponents = nullptr;",
      "   }",
      "   return true;",
      "}",
    ],
  },
  "System::dynamics/transmit/receive/process (no-op default)": {
    file: "contexts/src/mixr/src/models/system/System.cpp",
    line: 132,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// Default phase callbacks",
      "//------------------------------------------------------------------------------",
      "void System::dynamics(const double)",
      "{",
      "}",
      "",
      "void System::transmit(const double)",
      "{",
      "}",
      "",
      "void System::receive(const double)",
      "{",
      "}",
      "",
      "void System::process(const double)",
      "{",
      "}",
    ],
  },
};

const systemSnip = (key) => (key ? SYSTEM_SNIPPETS[key] || null : null);

function renderSystemSnippet(key) {
  const snip = systemSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_SYSTEM_SLOT_DOCS = {
  powerSwitch: ["String", "\"OFF\"/\"STBY\"/\"ON\" (case-insensitive) -- vira PWR_OFF/PWR_STBY/PWR_ON; nenhum callback de fase da própria classe consulta isso, é convenção para as subclasses"],
};

const REF_SYSTEM_POWER_ENUM = [
  ["PWR_OFF", "0", "\"OFF\"/\"off\" no slot"],
  ["PWR_STBY", "1", "\"STBY\"/\"stby\""],
  ["PWR_ON", "2", "\"ON\"/\"on\" -- default do slot e do membro pwrSw"],
  ["PWR_LAST", "3", "não é valor válido -- gancho para subclasses estenderem o enum (PWR_NEW1 = BaseClass::PWR_LAST, ...)"],
];

function SystemReferencePage({ onOpenCatalog }) {
  const entry = MODEL["System"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>System</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "System"</span>
          <span className="mx-chip">ownship + despacho de fase</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("System")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          A base de TODO subsistema que se anexa a um <code className="mx-mono">Player</code> via{" "}
          <code className="mx-mono">components:</code> -- <code className="mx-mono">Autopilot</code>,{" "}
          <code className="mx-mono">RfSensor</code>, <code className="mx-mono">Gimbal</code>,{" "}
          <code className="mx-mono">Datalink</code>, <code className="mx-mono">StoresMgr</code>,{" "}
          <code className="mx-mono">Navigation</code> e as outras classes dos "10 papéis" de{" "}
          <code className="mx-mono">Player</code> (ver a aba Player) são TODAS <code className="mx-mono">System</code>{" "}
          por baixo. Não decide NADA sozinha -- os quatro callbacks de fase (
          <code className="mx-mono">dynamics/transmit/receive/process</code>) são no-op por padrão. O que ela garante
          é o ENCAIXE: despacho de fase, descoberta do <code className="mx-mono">ownship</code> e freeze em cascata --
          não o comportamento em si.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de System" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>updateTC() -- o MESMO switch de fase, um nível abaixo</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">System::updateTC()</code> recalcula <code className="mx-mono">dt4 = dt * 4.0</code>{" "}
                de novo -- o mesmo valor que <code className="mx-mono">Player::updateTC()</code> já tinha computado
                um nível acima, mas de forma totalmente independente: cada <code className="mx-mono">System</code>{" "}
                redescobre a fase perguntando a <code className="mx-mono">ownship-&gt;getWorldModel()-&gt;phase()</code>{" "}
                e despacha para <code className="mx-mono">dynamics/transmit/receive/process</code> conforme o caso.{" "}
                <code className="mx-mono">Component::updateTC()</code> desce chamando{" "}
                <code className="mx-mono">obj-&gt;tcFrame(dt)</code> em CADA filho, TODA fase -- quem filtra por fase
                é cada <code className="mx-mono">System</code>, individualmente, dentro do PRÓPRIO{" "}
                <code className="mx-mono">updateTC()</code>, nunca um despachante central.{" "}
                <code className="mx-mono">BaseClass::updateTC(dt)</code> no fim usa <code className="mx-mono">dt</code>{" "}
                (não <code className="mx-mono">dt0</code>) -- "porque se estamos congelados, nossos subcomponentes
                também estão" (comentário nativo).
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>"We're nothing without an ownship" -- o mesmo guarda, três vezes</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">reset()</code>/<code className="mx-mono">updateData()</code>/{" "}
                <code className="mx-mono">updateTC()</code> começam todos com{" "}
                <code className="mx-mono">if (ownship == nullptr &amp;&amp; getOwnship() == nullptr) return;</code> --
                um <code className="mx-mono">System</code> sem <code className="mx-mono">ownship</code> (situação
                transitória, entre ser adicionado a <code className="mx-mono">components:</code> e o próximo ciclo de{" "}
                <code className="mx-mono">updateSystemPointers()</code>/<code className="mx-mono">findOwnship()</code>)
                simplesmente não faz NADA, em silêncio -- nem propaga a chamada para os próprios subcomponentes.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>findOwnship() -- a origem da família de armadilhas "container()" deste repositório</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                <code className="mx-mono">getOwnship()</code> é preguiçoso: só chama{" "}
                <code className="mx-mono">findOwnship()</code> se <code className="mx-mono">ownship</code> ainda é{" "}
                <code className="mx-mono">nullptr</code>; <code className="mx-mono">findOwnship()</code> usa{" "}
                <code className="mx-mono">findContainerByType(typeid(Player))</code>, subindo a árvore de{" "}
                <code className="mx-mono">Component</code> até achar o primeiro <code className="mx-mono">Player</code>{" "}
                ancestral -- exatamente o MESMO mecanismo que{" "}
                <code className="mx-mono">TacviewOutput::resolveInfo()</code>, o monitor do Groot e{" "}
                <code className="mx-mono">configurePlans()</code> (CLAUDE.md) já usam, e cuja família de armadilhas
                (objeto aninhado num slot NOMEADO não é alcançado por <code className="mx-mono">container()</code>, só
                quem está em <code className="mx-mono">components:</code> recursivo é) nasce exatamente aqui.{" "}
                <code className="mx-mono">copyData()</code> zera <code className="mx-mono">ownship</code>{" "}
                explicitamente num clone ("Don't copy ownship, we'll need to reacquire it") -- um{" "}
                <code className="mx-mono">System</code> clonado (hot-swap de plugin, ou o template de um{" "}
                <code className="mx-mono">Ntm</code> ao materializar um fantasma DIS) sempre redescobre o{" "}
                <code className="mx-mono">ownship</code> do zero, nunca herda um ponteiro potencialmente errado do
                original.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>killedNotification() default -- só repassa, nunca muda mode</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Ao contrário de <code className="mx-mono">Player::killedNotification()</code> (que marca dano/fumaça/
                chamas em 1.0 e pode fazer <code className="mx-mono">setMode(KILLED)</code>), o default de{" "}
                <code className="mx-mono">System::killedNotification()</code> só propaga{" "}
                <code className="mx-mono">KILL_EVENT</code> para os PRÓPRIOS subcomponentes e devolve{" "}
                <code className="mx-mono">true</code> -- reagir de verdade (desligar um sensor, por exemplo) é
                responsabilidade de cada subclasse concreta (<code className="mx-mono">Radar</code>/{" "}
                <code className="mx-mono">Rwr</code>/<code className="mx-mono">TrackManager</code> sobrescrevem isso,
                ver <code className="mx-mono">models/BUILT-IN.md</code>).
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>enum de powerSwitch -- a única coisa que System faz sozinha</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 8px" }}>
                Os quatro callbacks de fase são corpo VAZIO na base -- um <code className="mx-mono">( System )</code>{" "}
                puro é um componente legal e inofensivo, que não decide nem lê nada. O único estado que a própria
                classe manipula é <code className="mx-mono">powerSwitch</code>, e nem esse é consultado por{" "}
                <code className="mx-mono">updateTC()</code>/<code className="mx-mono">dynamics()</code> nativamente --
                é convenção para as subclasses lerem via <code className="mx-mono">getPowerSwitch()</code>.
              </p>
              <div className="mx-slotgrid">
                {REF_SYSTEM_POWER_ENUM.map(([k, v, d]) => (
                  <div className="mx-enumrow" key={k}>
                    <div className="mx-enumrow-name">{k} <span style={{ color: "var(--muted)", fontWeight: 400 }}>={v}</span></div>
                    <div className="mx-enumrow-desc">{d}</div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div className="mx-card">
            <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de System</div>
            <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{entry ? entry.own : 1} slot próprio.</p>
            <div className="mx-slotgrid">
              {(entry ? entry.sl : Object.keys(REF_SYSTEM_SLOT_DOCS)).map((s) => (
                <div className="mx-slot" key={s}>
                  <span>{s}{REF_SYSTEM_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SYSTEM_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                  <span style={{ maxWidth: 380 }}>{REF_SYSTEM_SLOT_DOCS[s] ? REF_SYSTEM_SLOT_DOCS[s][1] : ""}</span>
                </div>
              ))}
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">System::updateTC() -- na íntegra</span><span>C++</span></div>
              {renderSystemSnippet("System::updateTC (o mesmo switch, um nivel abaixo)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">System::isFrozen() / reset() / updateData() -- o guarda "sem ownship" repetido</span><span>C++</span></div>
              {renderSystemSnippet("System::isFrozen+reset+updateData (o mesmo guarda, tres vezes)")}
            </div>
            <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginBottom: 12, alignItems: "flex-start" }}>
              <div className="mx-card" style={{ flex: "1 1 260px" }}>
                <div className="mx-lbl"><span className="mx-mono">System::getOwnship()</span><span>C++</span></div>
                {renderSystemSnippet("System::getOwnship (lazy)")}
              </div>
              <div className="mx-card" style={{ flex: "1 1 220px" }}>
                <div className="mx-lbl"><span className="mx-mono">System::findOwnship()</span><span>C++</span></div>
                {renderSystemSnippet("System::findOwnship (o mesmo findContainerByType de sempre)")}
              </div>
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">System::copyData()</span><span>C++</span></div>
              {renderSystemSnippet("System::copyData (ownship nunca sobrevive a um clone)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">System::killedNotification()</span><span>C++</span></div>
              {renderSystemSnippet("System::killedNotification (so' repassa)")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">System::dynamics() / transmit() / receive() / process() -- default no-op</span><span>C++</span></div>
              {renderSystemSnippet("System::dynamics/transmit/receive/process (no-op default)")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ====================== Referência -- Gimbal / ScanGimbal / StabilizingGimbal / Antenna =========
 * Quarta leva da enciclopédia -- a primeira de QUATRO cobrindo a cadeia de radiofrequência
 * inteira (apontamento -> detecção -> pista -> assinatura). Mesmo padrão sem laboratório:
 * hero + Visão geral + Slots + Código-fonte. Duas ramificações de herança, não uma só --
 * Gimbal -> ScanGimbal -> Antenna é o lado mecânico+RF; StabilizingGimbal é irmã de
 * ScanGimbal, compensa a atitude do OWNSHIP (não a própria). Uma classe de RfSensor (a
 * próxima entrada da Referência) aponta pra Antenna por NOME (antennaName), o oposto de
 * como Player resolve seus "10 papéis" (por TIPO) -- contraste citado na própria página.
 * ==================================================================================== */

const GIMBAL_SNIPPETS = {
  "Gimbal::servoController (POSITION_SERVO vs RATE_SERVO)": {
    file: "contexts/src/mixr/src/models/system/Gimbal.cpp",
    line: 287,
    trunc: true,
    lines: [
      "void Gimbal::servoController(const double dt)",
      "{",
      "   // Only if we're not frozen ...",
      "   if (servoMode != FREEZE_SERVO) {",
      "",
      "      // ---",
      "      // Compute rate",
      "      // ---",
      "      base::Vec3d rate1( 0.0f, 0.0f, 0.0f );",
      "      if (servoMode == POSITION_SERVO) {",
      "",
      "         // position servo: drive the gimbal toward the commanded position",
      "         rate1 = cmdPos - pos;",
      "         rate1[AZ_IDX]   = base::angle::aepcdRad(rate1[AZ_IDX]);",
      "         rate1[ELEV_IDX] = base::angle::aepcdRad(rate1[ELEV_IDX]);",
      "         rate1[ROLL_IDX] = base::angle::aepcdRad(rate1[ROLL_IDX]);",
      "",
      "         // ---",
      "         // rate1 is radians per frame (step)",
      "         // Limit rate1:",
      "         //   Mechanical, fast-slew: rate is limited to maximum mechanical rate",
      "         //   Electronic, fast-slew: rate is unlimited!",
      "         //   Mechanical, slow-slew: rate is commanded rate limited to max mechanical rate",
      "         //   Electronic, slow-slew: rate is commanded rate (unlimited)",
      "         // ---",
      "         if (isFastSlewMode() && type == MECHANICAL) {",
      "               base::Vec3d step = maxRate * dt;",
      "               limitVec(rate1, step);",
      "         } else if (isSlowSlewMode()) {",
      "               base::Vec3d cmdRate1 = cmdRate;",
      "               if (type == MECHANICAL) {",
      "                  limitVec(cmdRate1, maxRate);",
      "               }",
      "               base::Vec3d step = cmdRate1 * dt;",
      "               limitVec(rate1, step);",
      "         }",
      "",
      "         if (dt != 0.0) rate = rate1 * (1.0f/dt);",
      "         else rate.set(0.0,0.0,0.0);",
      "      }",
      "",
      "      else if (servoMode == RATE_SERVO) {",
      "         // rate servo: follow commanded rate",
      "         rate1 = cmdRate;",
      "",
      "         // set servo rate to limited rate",
      "         if (type == MECHANICAL) limitVec(rate1, maxRate);",
      "",
      "         rate = rate1;",
      "      }",
    ],
  },
  "Gimbal::setSlotPlayerTypes (o comentario diz 0, o default real e 0xFFFF)": {
    file: "contexts/src/mixr/src/models/system/Gimbal.cpp",
    line: 1113,
    trunc: false,
    lines: [
      "",
      "// Player of interest types (default: 0 )",
      "bool Gimbal::setSlotPlayerTypes(const base::PairStream* const msg)",
      "{",
      "   bool ok{};",
      "   if (msg != nullptr) {",
      "      unsigned int mask{};",
      "      const base::List::Item* item{msg->getFirstItem()};",
      "      while (item != nullptr) {",
      "         const auto pair = static_cast<const base::Pair*>(item->getValue());",
      "         const auto type = dynamic_cast<const base::String*>( pair->object() );",
      "         if (type != nullptr) {",
      "            if ( utStrcasecmp(*type,\"air\") == 0 ) {",
      "               mask = (mask | Player::AIR_VEHICLE);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"ground\") == 0 ) {",
      "               mask = (mask | Player::GROUND_VEHICLE);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"weapon\") == 0 ) {",
      "               mask = (mask | Player::WEAPON);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"ship\") == 0 ) {",
      "               mask = (mask | Player::SHIP);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"building\") == 0 ) {",
      "               mask = (mask | Player::BUILDING);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"lifeform\") == 0 ) {",
      "               mask = (mask | Player::LIFE_FORM);",
      "            }",
      "            else if ( utStrcasecmp(*type,\"space\") == 0 ) {",
      "               mask = (mask | Player::SPACE_VEHICLE);",
      "            }",
      "         }",
      "         item = item->getNext();",
      "      }",
      "      ok = setPlayerOfInterestTypes(mask);",
      "   }",
      "   return ok;",
      "}",
      "",
      "// Max number of players of interest (default: 0)",
    ],
  },
  "ScanGimbal::pseudoRandomScanController -- estado 0, preso para sempre": {
    file: "contexts/src/mixr/src/models/system/ScanGimbal.cpp",
    line: 415,
    trunc: false,
    lines: [
      "{",
      "    static base::Integer iBar(1);",
      "",
      "    // Depending on our scan state, we will either start or stop the bar",
      "    switch(getScanState()) {",
      "        // reset state, must be in electronic mode or we will not operate",
      "        case 0: {",
      "            if (prScanVertices != nullptr) {",
      "                if ( isGimbalType(ELECTRONIC) ) {",
      "                    setServoMode(POSITION_SERVO);",
      "                    setFastSlewMode(true);",
      "                    setScanState(1);",
      "                }",
      "                else setScanMode(MANUAL_SCAN);",
      "            }",
      "        }",
      "            break;",
    ],
  },
  "ScanGimbal::setSlotPRVertices -- escreve atraves do ponteiro nulo": {
    file: "contexts/src/mixr/src/models/system/ScanGimbal.cpp",
    line: 957,
    trunc: false,
    lines: [
      "// setSlotPRVertices() -- gets a pairstream and puts the vertices into an array",
      "// example --",
      "//     vertices: { [ 1 2 ]  [ 3 4 ] [ 5 6 ] }",
      "bool ScanGimbal::setSlotPRVertices(const base::PairStream* const prObj)",
      "{",
      "   bool ok{true};",
      "",
      "   if (prObj != nullptr) {",
      "        // find how many vertices we have",
      "        const unsigned int n{prObj->entries()};",
      "        // Get the vertices from the pair stream",
      "        nprv = 0;",
      "        const base::List::Item* item{prObj->getFirstItem()};",
      "        // holds our array values",
      "        base::Vec2d tempVerts(0.0, 0.0);",
      "",
      "        while (item != nullptr && nprv < n) {",
      "            const auto p = dynamic_cast<const base::Pair*>(item->getValue());",
      "            if (p != nullptr) {",
      "                const base::Object* obj2{p->object()};",
      "                const auto msg2 = dynamic_cast<const base::List*>(obj2);",
      "                if (msg2 != nullptr) {",
      "                    double values[2]{};",
      "                    const unsigned int nl{msg2->getNumberList(values, 2)};",
      "",
      "                    if (nl == 2) {",
      "                        // set our values in our vector array",
      "                        prScanVertices[nprv].set(values[0],values[1]);",
      "                        nprv++;",
      "                    }",
      "                    else ok = false;",
      "                }",
      "            }",
      "            item = item->getNext();",
      "        }",
      "    }",
      "    return ok;",
      "}",
    ],
  },
  "StabilizingGimbal::roll/elevationStabilizingController": {
    file: "contexts/src/mixr/src/models/system/StabilizingGimbal.cpp",
    line: 79,
    trunc: false,
    lines: [
      "void StabilizingGimbal::rollStabilizingController(const double)",
      "{",
      "    if (getOwnship() == nullptr) return;",
      "",
      "    base::Vec3d tpos{getCmdPosition()};",
      "    if (mountPosition == NOSE){",
      "        tpos[ROLL_IDX] = static_cast<double>(-getOwnship()->getRoll());",
      "    }",
      "    else if (mountPosition == TAIL){",
      "        tpos[ROLL_IDX] = static_cast<double>(getOwnship()->getRoll());",
      "    }",
      "    else if (mountPosition == RIGHT_WING){",
      "        tpos[ELEV_IDX] = static_cast<double>(-getOwnship()->getPitch());",
      "    }",
      "    else if (mountPosition == LEFT_WING){",
      "        tpos[ELEV_IDX] = static_cast<double>(getOwnship()->getPitch());",
      "    }",
      "    setCmdPos( tpos );",
      "}",
    ],
  },
  "StabilizingGimbal::setSlotMountPosition -- comentado, o slot nao existe": {
    file: "contexts/src/mixr/src/models/system/StabilizingGimbal.cpp",
    line: 156,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// setSlotMountPosition() -- calls setMountPosition()",
      "//------------------------------------------------------------------------------",
      "/*",
      "bool StabilizingGimbal::setSlotMountPosition(base::String* const msg)",
      "{",
      "    // set our scan mode",
      "    bool ok = true;",
      "    if (msg != nullptr) {",
      "        if (*msg == \"nose\") ok = setMountPosition(NOSE);",
      "        else if (*msg == \"tail\") ok = setMountPosition(TAIL);",
      "        else if (*msg == \"left\") ok = setMountPosition(LEFT_WING);",
      "        else if (*msg == \"right\") ok = setMountPosition(RIGHT_WING);",
      "        else ok = false;",
      "    }",
      "    return ok;",
      "}",
      "*/",
    ],
  },
  "Antenna::rfTransmit -- fallback de ganho, ERP e o gate de threshold": {
    file: "contexts/src/mixr/src/models/system/Antenna.cpp",
    line: 458,
    trunc: true,
    lines: [
      "      if (!haveGainTgt) {",
      "         // ---",
      "         // No antenna pattern table",
      "         // ---",
      "         for (unsigned int i = 0; i < ntgts; i++) {",
      "            gainTgt[i] = 1.0;",
      "         }",
      "      }",
      "",
      "      // Compute antenna effective gain",
      "      double aeGain[MAX_PLAYERS]{};",
      "      base::multArrayConst(gainTgt, getGain(), aeGain, ntgts);",
      "",
      "      // Compute Effective Radiated Power (watts) (Equation 2-1)",
      "      double erp[MAX_PLAYERS]{};",
      "      base::multArrayConst(aeGain, xmit->getPower(), erp, ntgts);",
      "",
      "      // Fetch the required data arrays from the TargetDataBlock",
      "      const double* ranges{tdb->getTargetRanges()};",
      "      const double* rngRates{tdb->getTargetRangeRates()};",
      "      const base::Vec3d* losO2T{tdb->getLosVectors()};",
      "      const base::Vec3d* losT2O{tdb->getTargetLosVectors()};",
      "      Player** targets{tdb->getTargets()};",
      "",
      "      // ---",
      "      // Send emission packets to the targets",
      "      // ---",
      "      for (unsigned int i = 0; i < ntgts; i++) {",
      "",
      "         // Only of power exceeds an optional threshold",
      "         if (erp[i] > threshold) {",
      "",
      "            // Get a free emission packet",
      "            Emission* em{};",
      "            if (recycle) {",
      "               base::lock(freeEmLock);",
      "               em = freeEmStack.pop();",
      "               base::unlock(freeEmLock);",
      "            }",
      "",
      "            bool cloned{};",
      "            if (em == nullptr) {",
    ],
  },
  "RfSystem::reset -- resolve a Antenna por NOME (nao por tipo)": {
    file: "contexts/src/mixr/src/models/system/RfSystem.cpp",
    line: 126,
    trunc: false,
    lines: [
      "void RfSystem::reset()",
      "{",
      "   BaseClass::reset();",
      "",
      "   // ---",
      "   // Do we need to find the antenna?",
      "   // ---",
      "   if (getAntenna() == nullptr && getAntennaName() != nullptr && getOwnship() != nullptr) {",
      "      // We have a name of the antenna, but not the antenna itself",
      "      const char* name{*getAntennaName()};",
      "",
      "      // Get the named antenna from the player's list of gimbals, antennas and optics",
      "      const auto p = dynamic_cast<Antenna*>( getOwnship()->getGimbalByName(name) );",
      "      if (p != nullptr) {",
      "         setAntenna( p );",
      "         getAntenna()->setSystem(this);",
      "      }",
      "",
      "      if (getAntenna() == nullptr) {",
      "         // The assigned antenna was not found!",
      "         std::cerr << \"RfSystem::reset() ERROR -- antenna: \" << name << \", was not found!\" << std::endl;",
      "         setSlotAntennaName(nullptr);",
      "      }",
      "   }",
      "",
      "   // ---",
      "   // Initialize players of interest",
      "   // ---",
      "   processPlayersOfInterest();",
      "",
      "}",
    ],
  },
};

const gimbalSnip = (key) => (key ? GIMBAL_SNIPPETS[key] || null : null);

function renderGimbalSnippet(key) {
  const snip = gimbalSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_GIMBAL_SLOT_DOCS = {
  type: ["String", "\"mechanical\"/\"electronic\" -- default ELECTRONIC"],
  location: ["List", "[ x y z ] metros -- posição do gimbal no container, default 0,0,0"],
  initPosition: ["List", "[ az el roll ] rad -- posição inicial"],
  initPosAzimuth: ["Angle", "componente isolado da posição inicial"],
  initPosElevation: ["Angle", "idem, elevação"],
  initPosRoll: ["Angle", "idem, roll"],
  azimuthLimits: ["List", "[ left right ] rad -- fora de [-π,π] = sem limite"],
  azimuthLimitLeft: ["Angle", "componente isolado"],
  azimuthLimitRight: ["Angle", "componente isolado"],
  elevationLimits: ["List", "[ lower upper ] rad"],
  elevationLimitLower: ["Angle", "componente isolado"],
  elevationLimitUpper: ["Angle", "componente isolado"],
  rollLimits: ["List", "[ lower upper ] -- o header chama de Angle, mas o slot registrado é List"],
  rollLimitLower: ["Angle", "componente isolado"],
  rollLimitUpper: ["Angle", "componente isolado"],
  maxRates: ["List", "[ az el roll ] rad/s -- taxa mecânica máxima, default 120°/s cada"],
  maxRateAzimuth: ["Angle", "componente isolado"],
  maxRateElevation: ["Angle", "componente isolado"],
  maxRateRoll: ["Angle", "componente isolado"],
  commandPosition: ["List", "[ az el roll ] rad -- arma POSITION_SERVO"],
  commandPosAzimuth: ["Angle", "componente isolado, arma POSITION_SERVO"],
  commandPosElevation: ["Angle", "componente isolado"],
  commandPosRoll: ["Angle", "componente isolado"],
  commandRates: ["List", "[ az el roll ] rad/s -- arma RATE_SERVO"],
  commandRateAzimuth: ["Angle", "componente isolado, arma RATE_SERVO"],
  commandRateElevation: ["Angle", "componente isolado"],
  commandRateRoll: ["Angle", "componente isolado"],
  terrainOcculting: ["Number(bool)", "default false"],
  checkHorizon: ["Number(bool)", "default true"],
  playerOfInterestTypes: ["PairStream", "nomes → bitmask (air/ground/weapon/ship/building/lifeform/space) -- default REAL 0xFFFF (todos), apesar do comentário do .cpp dizer \"(default: 0)\""],
  maxPlayersOfInterest: ["Number", "default 200"],
  maxRange2PlayersOfInterest: ["Distance", "default 0 = sem limite"],
  maxAngle2PlayersOfInterest: ["Angle", "default 0 = sem limite"],
  localPlayersOfInterestOnly: ["Number(bool)", "default false"],
  useWorldCoordinates: ["Number(bool)", "default true -- usa ECEF do alvo"],
  ownHeadingOnly: ["Number(bool)", "default true"],
};

const REF_SCANGIMBAL_SLOT_DOCS = {
  scanMode: ["String", "manual/horizontal/vertical/conical/circular/pseudorandom/spiral -- default MANUAL_SCAN"],
  leftToRightScan: ["Boolean", "sentido do scan de barra -- default true"],
  scanWidth: ["Number", "largura do volume de busca, rad -- default 0"],
  searchVolume: ["List", "[ width height ] rad -- também arma HORIZONTAL_BAR_SCAN e deriva numBars/barSpacing"],
  reference: ["List", "[ az el ] rad -- centro do volume de busca, default 0,0"],
  barSpacing: ["Number", "largura entre barras, rad -- default 0"],
  numBars: ["Integer", "número de barras -- default 1"],
  revolutionsPerSec: ["Number", "Hz -- scan cônico/circular/espiral, default 5"],
  scanRadius: ["Number|Angle", "raio do scan cônico/espiral -- default 2°"],
  pseudoRandomPattern: ["PairStream", "vértices [ az el ] -- ver achado: o array-alvo nunca é alocado"],
  maxRevolutions: ["Number", "máx. voltas do scan espiral -- default 1.0"],
};

const REF_STABILIZINGGIMBAL_SLOT_DOCS = {
  stabilizingMode: ["String", "\"elevation\"/\"roll\"/\"horizon\" -- default HORIZON"],
};

const REF_ANTENNA_SLOT_DOCS = {
  polarization: ["String", "none/vertical/horizontal/slant/RHC/LHC -- default NONE"],
  threshold: ["Power", "limiar de ERP para enviar emissão -- default 0.0 W"],
  gain: ["Number", "ganho escalar, adimensional -- default 1.0"],
  gainPattern: ["Func1|Func2", "padrão de ganho off-boresight, em dB -- default 0"],
  gainPatternDeg: ["Boolean", "padrão em graus (true) ou radianos (false, default)"],
  recycle: ["Boolean", "reciclar objetos Emission -- default true"],
  beamWidth: ["Angle|Number", "largura de feixe -- default 3.5°, deve ser > 0"],
};

function GimbalReferencePage({ onOpenCatalog }) {
  const entry = MODEL["Antenna"];
  const gEntry = MODEL["Gimbal"];
  const sgEntry = MODEL["ScanGimbal"];
  const stgEntry = MODEL["StabilizingGimbal"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>Gimbal / Antenna</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "Gimbal"</span>
          <span className="mx-chip">+ ScanGimbal → Antenna</span>
          <span className="mx-chip">+ StabilizingGimbal (irmã)</span>
          {entry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("Antenna")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          Duas ramificações de herança, não uma cadeia só. <code className="mx-mono">Gimbal</code> (base
          mecânica: posição/taxa/limite, servo de posição OU taxa) → <code className="mx-mono">ScanGimbal</code>{" "}
          (acrescenta padrões de varredura) → <code className="mx-mono">Antenna</code> (acrescenta a física de
          RF -- ganho, polarização, ERP). <code className="mx-mono">StabilizingGimbal</code> é IRMÃ de{" "}
          <code className="mx-mono">ScanGimbal</code> -- deriva direto de <code className="mx-mono">Gimbal</code>{" "}
          e compensa a ATITUDE DO OWNSHIP, nunca a própria. Um <code className="mx-mono">RfSensor</code> (aba
          seguinte) não HERDA nada disto -- ele aponta para uma <code className="mx-mono">Antenna</code> por{" "}
          NOME (slot <code className="mx-mono">antennaName</code>, resolvido em <code className="mx-mono">reset()</code>) --
          o oposto de como <code className="mx-mono">Player</code> resolve seus 10 papéis, por TIPO (ver a aba Player).
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {entry ? entry.ch.join(" → ") : "Antenna → ScanGimbal → Gimbal → System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de Gimbal/Antenna" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>servoController() -- posição OU taxa, nunca os dois</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">enum ServoMode {"{"}FREEZE_SERVO, RATE_SERVO, POSITION_SERVO{"}"}</code>{" "}
                escolhe UM caminho por frame. Em <code className="mx-mono">POSITION_SERVO</code>, o erro
                angular (<code className="mx-mono">cmdPos - pos</code>) vira a taxa do frame. Em gimbal{" "}
                <b>eletrônico</b> com <code className="mx-mono">fastSlewMode</code> ligado, NENHUM dos dois
                ramos de limitação executa (só <code className="mx-mono">MECHANICAL &amp;&amp; fastSlew</code> ou{" "}
                <code className="mx-mono">slowSlew</code>) -- o comentário nativo já avisa: "Electronic,
                fast-slew: rate is unlimited!". Não é "rápido", é INSTANTÂNEO: o gimbal salta pra posição
                comandada em UM frame só, qualquer que seja <code className="mx-mono">dt</code>.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Bar scan -- 1, 2, 3 E 4 barras (não só "1, 2 e 4")</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                O header de <code className="mx-mono">ScanGimbal</code> documenta "1, 2 e 4 bar scans" -- mas{" "}
                <code className="mx-mono">computeNewBarPos()</code> tem QUATRO tabelas de lookup
                (1/2/3/4 barras); números ímpares alternam sentido a cada ciclo via{" "}
                <code className="mx-mono">isReverseScan()</code>. <code className="mx-mono">setSearchVolume()</code>{" "}
                deriva <code className="mx-mono">numBars</code> automaticamente por faixa de altura quando o
                valor pedido não é 1/2/3/4 (&lt;1°→1, &lt;5°→2, &lt;10°→3, senão 4) -- e chamar{" "}
                <code className="mx-mono">setBarSpacing()</code>/<code className="mx-mono">setNumBars()</code>{" "}
                depois RECALCULA <code className="mx-mono">scanHeight</code> como efeito colateral (o próprio
                header já avisa "se misturar os dois, resultado inesperado").
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Bug confirmado, não redescobrir:</b> <code className="mx-mono">ScanGimbal::prScanVertices</code>{" "}
                (<code className="mx-mono">base::Vec2d*</code>, `ScanGimbal.hpp:234`) nasce{" "}
                <code className="mx-mono">nullptr</code> e NUNCA é alocado -- nenhum{" "}
                <code className="mx-mono">new base::Vec2d[...]</code> existe em{" "}
                <code className="mx-mono">ScanGimbal.cpp</code> inteiro (confirmado por busca no fonte
                vendorizado e no pacote Conan instalado). <code className="mx-mono">setSlotPRVertices()</code>{" "}
                escreve incondicionalmente em <code className="mx-mono">prScanVertices[nprv]</code> assim que o
                slot <code className="mx-mono">pseudoRandomPattern</code> recebe QUALQUER vértice de verdade --
                escrita através de ponteiro nulo. E mesmo que essa escrita não derrube o processo,{" "}
                <code className="mx-mono">pseudoRandomScanController()</code> testa{" "}
                <code className="mx-mono">if (prScanVertices != nullptr)</code> no estado 0 -- que é SEMPRE
                falso -- então o modo <code className="mx-mono">PSEUDO_RANDOM_SCAN</code> fica PRESO no estado 0
                para sempre: nunca degrada pra <code className="mx-mono">MANUAL_SCAN</code> (esse branch está
                aninhado DENTRO do teste de nulo, também inalcançável), nunca escaneia, sem aviso nenhum.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>StabilizingGimbal -- compensa o OWNSHIP, não a si mesma</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Lê <code className="mx-mono">getOwnship()-&gt;getRoll()</code>/<code className="mx-mono">getPitch()</code>{" "}
                -- se <code className="mx-mono">getOwnship()==nullptr</code>, os dois controllers retornam sem
                fazer nada. <code className="mx-mono">stabilizingMode: horizon</code> (default) chama OS DOIS
                controllers (roll e elevação) no mesmo frame; <code className="mx-mono">roll</code>/{" "}
                <code className="mx-mono">elevation</code> chamam só um. Para montagem <code className="mx-mono">NOSE</code>,{" "}
                contra-rola o berço (<code className="mx-mono">-getRoll()</code>); para <code className="mx-mono">TAIL</code>,
                o sinal inverte.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado:</b> <code className="mx-mono">MountPosition</code>{" "}
                (<code className="mx-mono">TAIL</code>/<code className="mx-mono">LEFT_WING</code>/{" "}
                <code className="mx-mono">RIGHT_WING</code>) é INALCANÇÁVEL via EDL -- o corpo de{" "}
                <code className="mx-mono">setSlotMountPosition()</code> está inteiro dentro de um comentário de
                bloco <code className="mx-mono">{"/* ... */"}</code> (`StabilizingGimbal.cpp:159-173`) e a slot
                table declara SÓ <code className="mx-mono">stabilizingMode</code> -- nenhum{" "}
                <code className="mx-mono">.edl</code> pode mudar a montagem, que fica travada em{" "}
                <code className="mx-mono">NOSE</code> (o default) para sempre. Os ramos{" "}
                <code className="mx-mono">TAIL</code>/<code className="mx-mono">*_WING</code> dos dois
                controllers são código morto em qualquer cenário configurado só por EDL.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Antenna -- a física de RF por cima do apontamento geométrico</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                <code className="mx-mono">rfTransmit(Emission*)</code> (saída): ganho off-boresight via a{" "}
                <code className="mx-mono">Tdb</code> (Target Data Block) + padrão de ganho{" "}
                (<code className="mx-mono">Func1</code>/<code className="mx-mono">Func2</code>, dB→linear) --
                sem padrão configurado, cai num fallback SILENCIOSO de ganho 1.0 (sem aviso). Calcula ERP =
                ganho efetivo × potência do transmissor, e só envia se <code className="mx-mono">erp &gt; threshold</code>.{" "}
                <code className="mx-mono">onRfEmissionEvent(Emission*)</code> (entrada): ganho de recepção pelo
                MESMO padrão (agora off-boresight do transmissor) × área efetiva × ganho de polarização (tabela
                fixa 6×6 -- combinações cruzadas VERTICAL×HORIZONTAL dão 0.0; qualquer coisa com{" "}
                <code className="mx-mono">NONE</code> dá 1.0, nunca penalizada).
              </p>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: 0 }}>
                <code className="mx-mono">RfSystem::reset()</code> resolve <code className="mx-mono">antennaName</code>{" "}
                por <code className="mx-mono">getOwnship()-&gt;getGimbalByName(name)</code> + `dynamic_cast`{" "}
                UMA vez (nunca em <code className="mx-mono">updateData()</code>/`process()`) e grava o back-pointer{" "}
                (<code className="mx-mono">Antenna::setSystem(this)</code>) -- um cenário de produção deste
                repositório já documenta em comentário o corolário: "uma antena por sensor -- compartilhar
                antena entre dois sensores faz o último a dar reset() vencer, sem aviso" (`tests/fixtures/
                built-in_mixr_1/configs/scenario_max_player.edl.in`).
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 460px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Gimbal</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{gEntry ? gEntry.own : 36} slots próprios -- a maior slot table desta Referência.</p>
              <div className="mx-slotgrid">
                {(gEntry ? gEntry.sl : Object.keys(REF_GIMBAL_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_GIMBAL_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_GIMBAL_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 340 }}>{REF_GIMBAL_SLOT_DOCS[s] ? REF_GIMBAL_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de ScanGimbal</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{sgEntry ? sgEntry.own : 11} slots próprios.</p>
              <div className="mx-slotgrid">
                {(sgEntry ? sgEntry.sl : Object.keys(REF_SCANGIMBAL_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SCANGIMBAL_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SCANGIMBAL_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_SCANGIMBAL_SLOT_DOCS[s] ? REF_SCANGIMBAL_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 260px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de StabilizingGimbal</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{stgEntry ? stgEntry.own : 1} slot próprio -- `mountPosition` não tem slot (ver achado).</p>
              <div className="mx-slotgrid">
                {(stgEntry ? stgEntry.sl : Object.keys(REF_STABILIZINGGIMBAL_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_STABILIZINGGIMBAL_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_STABILIZINGGIMBAL_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_STABILIZINGGIMBAL_SLOT_DOCS[s] ? REF_STABILIZINGGIMBAL_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de Antenna</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{entry ? entry.own : 7} slots próprios -- mais os herdados de ScanGimbal/Gimbal.</p>
              <div className="mx-slotgrid">
                {(entry ? entry.sl : Object.keys(REF_ANTENNA_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_ANTENNA_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_ANTENNA_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_ANTENNA_SLOT_DOCS[s] ? REF_ANTENNA_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Gimbal::servoController() -- POSITION_SERVO vs. RATE_SERVO</span><span>C++</span></div>
              {renderGimbalSnippet("Gimbal::servoController (POSITION_SERVO vs RATE_SERVO)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Gimbal::setSlotPlayerTypes() -- e o comentário "(default: 0)" logo acima</span><span>C++</span></div>
              {renderGimbalSnippet("Gimbal::setSlotPlayerTypes (o comentario diz 0, o default real e 0xFFFF)")}
            </div>
            <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginBottom: 12, alignItems: "flex-start" }}>
              <div className="mx-card" style={{ flex: "1 1 320px" }}>
                <div className="mx-lbl"><span className="mx-mono">ScanGimbal -- estado 0 do pseudo-random (preso pra sempre)</span><span>C++</span></div>
                {renderGimbalSnippet("ScanGimbal::pseudoRandomScanController -- estado 0, preso para sempre")}
              </div>
              <div className="mx-card" style={{ flex: "1 1 320px" }}>
                <div className="mx-lbl"><span className="mx-mono">ScanGimbal::setSlotPRVertices() -- escrita através de ponteiro nulo</span><span>C++</span></div>
                {renderGimbalSnippet("ScanGimbal::setSlotPRVertices -- escreve atraves do ponteiro nulo")}
              </div>
            </div>
            <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginBottom: 12, alignItems: "flex-start" }}>
              <div className="mx-card" style={{ flex: "1 1 260px" }}>
                <div className="mx-lbl"><span className="mx-mono">StabilizingGimbal::rollStabilizingController()</span><span>C++</span></div>
                {renderGimbalSnippet("StabilizingGimbal::roll/elevationStabilizingController")}
              </div>
              <div className="mx-card" style={{ flex: "1 1 280px" }}>
                <div className="mx-lbl"><span className="mx-mono">setSlotMountPosition() -- corpo inteiro comentado</span><span>C++</span></div>
                {renderGimbalSnippet("StabilizingGimbal::setSlotMountPosition -- comentado, o slot nao existe")}
              </div>
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Antenna::rfTransmit() -- fallback de ganho, ERP e o gate de threshold</span><span>C++</span></div>
              {renderGimbalSnippet("Antenna::rfTransmit -- fallback de ganho, ERP e o gate de threshold")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">RfSystem::reset() -- resolve a Antenna por NOME</span><span>C++</span></div>
              {renderGimbalSnippet("RfSystem::reset -- resolve a Antenna por NOME (nao por tipo)")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ====================== Referência -- RfSystem / RfSensor / Radar / Sar / Rwr / Jammer / SensorMgr
 * Segunda das quatro entradas de RF. RfSystem é a base comum (antena por NOME, fila de
 * emissão, jamSignal); RfSensor acrescenta o TrackManager (também por nome) e o modo/faixa;
 * Radar/Sar/Rwr/Jammer/SensorMgr são as cinco folhas concretas -- cada uma estruturalmente
 * só-transmissora, só-receptora, ou nem uma coisa nem outra (SensorMgr).
 * ==================================================================================== */

const RFSENSOR_SNIPPETS = {
  "RfSystem::rfReceivedEmission -- fila de emissao + acumulo de jamSignal": {
    file: "contexts/src/mixr/src/models/system/RfSystem.cpp",
    line: 205,
    trunc: false,
    lines: [
      "// rfReceivedEmission() -- process returned RF Emission",
      "//------------------------------------------------------------------------------",
      "void RfSystem::rfReceivedEmission(Emission* const em, Antenna* const, double raGain)",
      "{",
      "   // Queue up emissions for receive() to process",
      "   if (em != nullptr && isReceiverEnabled()) {",
      "",
      "      // Test to make sure the received emission is in-band before proceeding",
      "      if (affectsRfSystem(em)) {",
      "",
      "         // Pulses this radar frame (from emission)",
      "         //double pulses = static_cast<double>( em->getPulses() );",
      "         //if (pulses <= 0) pulses = 1.0f;",
      "",
      "         // Compute signal losses",
      "         //    Basically, we're simulating Hannen's S/I equation from page 356 of his notes.",
      "         //    Where I is N + J. J is noise from jamming.",
      "         //    Receiver Loss affects the total I, so we have to wait until J is added to N in Radar.",
      "         double losses{getRfSignalProcessLoss() * em->getAtmosphericAttenuationLoss() * em->getTransmitLoss()};",
      "         if (losses < 1.0) losses = 1.0;",
      "",
      "         // Range loss",
      "         const double rl{em->getRangeLoss()};",
      "",
      "         // Signal Equation (one way signal)",
      "         // Signal Equation (Part of equation 2-7)",
      "         // Signal (equation 3-3)",
      "         const double signal{em->getPower() * rl * raGain / losses};",
      "",
      "         // Noise Jammer -- add this signal to the total interference signal (noise)",
      "         if ( em->isECMType(Emission::ECM_NOISE) ) {",
      "            // CGB part of the noise jamming equation says we're only affected by the ratio of the",
      "            // transmitter and receiver bandwidths.",
      "            // It's possible that we'll want to account for this in the signal calculation above.",
      "            // But, for now, it is sufficient right here.",
      "            jamSignal += (signal * getBandwidth() / em->getBandwidth());",
      "         }",
      "",
      "         // Save packet and signal for receive()",
      "         base::lock(packetLock);",
      "         if (np < MAX_EMISSIONS) {",
      "            em->ref();",
      "            packets[np] = em;",
      "            signals[np] = signal;",
      "            np++;",
      "         }",
      "         base::unlock(packetLock);",
      "",
      "      }",
      "   }",
      "}",
    ],
  },
  "Radar::transmit -- pulses cai para 1 quando PRF*dt arredonda a zero": {
    file: "contexts/src/mixr/src/models/system/Radar.cpp",
    line: 153,
    trunc: false,
    lines: [
      "void Radar::transmit(const double dt)",
      "{",
      "   BaseClass::transmit(dt);",
      "",
      "   // Transmitting, scanning and have an antenna?",
      "   if ( !areEmissionsDisabled() && isTransmitting() ) {",
      "      // Send the emission to the other player",
      "      const auto em = new Emission();",
      "      em->setFrequency(getFrequency());",
      "      em->setBandwidth(getBandwidth());",
      "      const double prf1{getPRF()};",
      "      em->setPRF(prf1);",
      "      int pulses{static_cast<int>(prf1 * dt + 0.5)};",
      "      if (pulses == 0) pulses = 1; // at least one",
      "      em->setPulses(pulses);",
      "      const double p{getPeakPower()};",
      "      em->setPower(p);",
      "      em->setMaxRangeNM(getRange());",
      "      em->setPulseWidth(getPulseWidth());",
      "      em->setTransmitLoss(getRfTransmitLoss());",
      "      em->setReturnRequest( isReceiverEnabled() );",
      "      em->setTransmitter(this);",
      "      getAntenna()->rfTransmit(em);",
      "      em->unref();",
      "   }",
      "",
      "}",
    ],
  },
  "Radar::receive -- inicio (fila, interferencia, filtro de eco proprio/ECM)": {
    file: "contexts/src/mixr/src/models/system/Radar.cpp",
    line: 184,
    trunc: true,
    lines: [
      "void Radar::receive(const double dt)",
      "{",
      "   BaseClass::receive(dt);",
      "",
      "   // Can't do anything without an antenna",
      "   if (getAntenna() == nullptr) return;",
      "",
      "   // Clear the next sweep",
      "   csweep = computeSweepIndex( static_cast<double>(base::angle::R2DCC * getAntenna()->getAzimuth()) );",
      "   clearSweep(csweep);",
      "",
      "   // Compute noise level",
      "   // CGB moved here from RfSystem",
      "   // Basically, we're simulation Hannen's S/I equation from page 356 of his notes.",
      "   // Where I is N + J. J is noise from jamming.",
      "   // Receiver Loss affects the total I, so we have to wait until this point to account for it.",
      "   const double interference{(getRfRecvNoise() + jamSignal) * getRfReceiveLoss()};",
      "   const double noise{getRfRecvNoise() * getRfReceiveLoss()};",
      "   currentJamSignal = jamSignal * getRfReceiveLoss();",
      "   int countNumJammedEm{};",
      "",
      "   // ---",
      "   // Process Returned Emissions",
      "   // ---",
      "",
      "   Emission* em{};",
      "   double signal{};",
      "",
      "   // Get an emission from the queue",
      "   base::lock(packetLock);",
      "   if (np > 0) {",
      "      np--; // Decrement 'np', now the array index",
      "      em = packets[np];",
      "      signal = signals[np];",
      "   }",
      "   base::unlock(packetLock);",
      "",
      "   while (em != nullptr) {",
      "",
      "      // exclude noise jammers (accounted for already in RfSystem::rfReceivedEmission)",
      "      if (em->getTransmitter() == this || (em->isECM() && !em->isECMType(Emission::ECM_NOISE)) ) {",
      "",
      "         // compute the return trip loss ...",
    ],
  },
  "Radar::receive -- o gate de S/I a 125% do alcance, e o reset de jamSignal": {
    file: "contexts/src/mixr/src/models/system/Radar.cpp",
    line: 251,
    trunc: false,
    lines: [
      "         if (signal > 0.0) {",
      "",
      "            // Signal/Noise  (Equation 2-9)",
      "            const double signalToInterferenceRatio{signal / interference};",
      "            const double signalToInterferenceRatioDbl{10.0f * std::log10(signalToInterferenceRatio)};",
      "            const double signalToNoiseRatio{signal / noise};",
      "            const double signalToNoiseRatioDbl{10.0f * std::log10(signalToNoiseRatio)};",
      "",
      "            // Is S/N above receiver threshold and within 125% of max range?",
      "            // CGB, if \"signal <= 0.0\", then \"signalToInterferenceRatioDbl\" is probably invalid",
      "            // we should probably do something smart with \"signalToInterferenceRatioDbl\" above as well.",
      "            base::lock(myLock);",
      "            if (signalToInterferenceRatioDbl >= getRfThreshold() && em->getRange() <= (maxRng*1.25) && rptQueue.isNotFull()) {",
      "",
      "               // send the report to the track manager",
      "               em->ref();",
      "               rptQueue.put(em);",
      "               rptSnQueue.put(signalToInterferenceRatioDbl);",
      "",
      "               // Save signal for real-beam display",
      "               const int iaz{csweep};",
      "               const unsigned int irng{computeRangeIndex( em->getRange() )};",
      "               sweeps[iaz][irng] += (signalToInterferenceRatioDbl/100.0f);",
      "               vclos[iaz][irng] = em->getRangeRate();",
      "",
      "            } else if (signalToInterferenceRatioDbl < getRfThreshold() && signalToNoiseRatioDbl >= getRfThreshold()) {",
      "               countNumJammedEm++;",
      "            }",
      "            base::unlock(myLock);",
      "         }",
      "      }",
      "",
      "      em->unref();   // this unref() undoes the ref() done by RfSystem::rfReceivedEmission",
      "      em = nullptr;",
      "",
      "      // Get another emission from the queue",
      "      base::lock(packetLock);",
      "      if (np > 0) {",
      "         np--;",
      "         em = packets[np];",
      "         signal = signals[np];",
      "      }",
      "      base::unlock(packetLock);",
      "   }",
      "",
      "   numberOfJammedEmissions = countNumJammedEm;",
      "",
      "   // Set interference signal back to zero",
      "   jamSignal = 0;",
      "}",
    ],
  },
  "Rwr::receive -- rejeita ECM, nunca le/zera jamSignal, entrega na fase 2": {
    file: "contexts/src/mixr/src/models/system/Rwr.cpp",
    line: 74,
    trunc: true,
    lines: [
      "void Rwr::receive(const double dt)",
      "{",
      "   BaseClass::receive(dt);",
      "",
      "   // clear the back buffer",
      "   clearRays(0);",
      "",
      "   // Receiver losses",
      "#if 0",
      "   const double noise{getRfRecvNoise()};",
      "#else",
      "   const double noise{getRfRecvNoise() * getRfReceiveLoss()};",
      "#endif",
      "",
      "   // Process received emissions",
      "   TrackManager* tm{getTrackManager()};",
      "   Emission* em{};",
      "   double signal{};",
      "",
      "   // Get an emission from the queue",
      "   base::lock(packetLock);",
      "   if (np > 0) {",
      "      np--; // Decrement 'np', now the array index",
      "      em = packets[np];",
      "      signal = signals[np];",
      "   }",
      "   base::unlock(packetLock);",
      "",
      "   while (em != nullptr) {",
      "",
      "      // CGB, if \"signal <= 0.0\", then \"snDbl\" is probably invalid",
      "      if (signal > 0.0 && dt != 0.0) {",
      "",
      "         // Signal over noise (equation 3-5)",
      "         const double sn{signal / noise};",
      "         const double snDbl{10.0 * std::log10(sn)};",
      "",
      "         // Is S/N above receiver threshold  ## dpg -- for now, don't include ECM emissions",
      "         if (snDbl > getRfThreshold() && !em->isECM() && rptQueue.isNotFull()) {",
      "            // Send report to the track manager",
      "            if (tm != nullptr) {",
      "               tm->newReport(em, snDbl);",
      "            }",
      "",
      "            // Get Angle Of Arrival",
      "            const double aoa{em->getAzimuthAoi()};",
    ],
  },
  "Jammer::transmit -- ECM_NOISE e o UNICO sinal de \"isto e um jammer\"": {
    file: "contexts/src/mixr/src/models/system/Jammer.cpp",
    line: 21,
    trunc: false,
    lines: [
      "Jammer::Jammer()",
      "{",
      "    STANDARD_CONSTRUCTOR()",
      "    setTransmitterEnableFlag(true);",
      "    setReceiverEnabledFlag(false);",
      "    setTypeId(\"JAMMER\");",
      "}",
      "",
      "void Jammer::copyData(const Jammer& org, const bool)",
      "{",
      "    BaseClass::copyData(org);",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// transmit() -- send jam emissions",
      "//------------------------------------------------------------------------------",
      "void Jammer::transmit(const double)",
      "{",
      "    // Send the emission to the other player",
      "    if ( !areEmissionsDisabled() && isTransmitting() ) {",
      "        const auto em = new Emission();",
      "        em->setFrequency(getFrequency());",
      "        const double p{getPeakPower()};",
      "        em->setPower(p);",
      "        em->setTransmitLoss(getRfTransmitLoss());",
      "        em->setMaxRangeNM(getRange());",
      "        em->setBandwidth(getBandwidth());",
      "        em->setTransmitter(this);",
      "        em->setReturnRequest(false);",
      "        em->setECM(Emission::ECM_NOISE);",
      "        getAntenna()->rfTransmit(em);",
      "        em->unref();",
      "    }",
      "}",
    ],
  },
  "Sar::requestImage/cancel -- 'ok' nunca vira true, mesmo com sucesso": {
    file: "contexts/src/mixr/src/models/system/Sar.cpp",
    line: 157,
    trunc: false,
    lines: [
      "bool Sar::requestImage(",
      "        const unsigned int w,           // Image width (pixels)",
      "        const unsigned int h,           // Image height (pixels)",
      "        const double r)                 // Image Resolution (meters/pixel)",
      "{",
      "   bool ok{};",
      "   if ( isSystemReady() ) {",
      "      if (isMessageEnabled(MSG_INFO)) {",
      "         std::cout << \"starting new SAR (\" << w << \",\" << h << \") at \" << r << std::endl;",
      "      }",
      "      width = w;",
      "      height = h;",
      "      resolution = r;",
      "      timer = DEFAULT_SAR_TIME;",
      "      setTransmitterEnableFlag(true);",
      "   }",
      "   return ok;",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// cancel() -- Cancel the current SAR imaging",
      "//------------------------------------------------------------------------------",
      "void Sar::cancel()",
      "{",
      "   setTransmitterEnableFlag(false);",
      "   timer = 0;",
      "}",
    ],
  },
  "SensorMgr -- o arquivo inteiro. 'gerenciar' e so herdar a arvore de components:": {
    file: "contexts/src/mixr/src/models/system/SensorMgr.cpp",
    line: 1,
    trunc: false,
    lines: [
      "",
      "#include \"mixr/models/system/SensorMgr.hpp\"",
      "",
      "namespace mixr {",
      "namespace models {",
      "",
      "IMPLEMENT_SUBCLASS(SensorMgr, \"SensorMgr\")",
      "EMPTY_SLOTTABLE(SensorMgr)",
      "EMPTY_COPYDATA(SensorMgr)",
      "EMPTY_DELETEDATA(SensorMgr)",
      "",
      "SensorMgr::SensorMgr()",
      "{",
      "    STANDARD_CONSTRUCTOR()",
      "}",
      "",
      "}",
      "}",
    ],
  },
};

const rfSensorSnip = (key) => (key ? RFSENSOR_SNIPPETS[key] || null : null);

function renderRfSensorSnippet(key) {
  const snip = rfSensorSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_RFSYSTEM_SLOT_DOCS = {
  antennaName: ["String", "nome da Antenna a resolver em reset() -- mesmo padrão \"por nome\" do TrackManager em RfSensor"],
  frequency: ["Frequency|Number", "Hz -- default 0"],
  bandwidth: ["Frequency|Number", "Hz -- default 1.0, deve ser ≥ 1"],
  powerPeak: ["Number(Power)", "Watts -- default 0"],
  threshold: ["Decibel", "limiar do receptor acima do ruído, dB"],
  noiseFigure: ["Number", "adimensional, ≥ 1 -- default 1.0"],
  systemTemperature: ["Number", "Kelvin -- default 290.0"],
  lossXmit: ["Number|Decibel", "perda de transmissão, ≥ 1 -- default 1.0"],
  lossRecv: ["Number|Decibel", "perda de recepção, ≥ 1 -- default 1.0"],
  lossSignalProcess: ["Number|Decibel", "perda de processamento de sinal, ≥ 1 -- default 1.0"],
  disableEmissions: ["Number(bool)", "desliga o envio de pacotes de emissão -- default false"],
  bandwidthNoise: ["Frequency|Number", "Hz -- se ausente, cai para bandwidth (getBandwidthNoise())"],
};

const REF_RFSENSOR_SLOT_DOCS = {
  trackManagerName: ["String", "nome do TrackManager a resolver em reset()"],
  modes: ["PairStream|RfSensor", "lista de submodos -- cada item vira dynamic_cast<RfSensor*>, senão erro"],
  ranges: ["List", "vetor de alcances, NM"],
  initRangeIdx: ["Number", "índice inicial [1..nRanges] -- default 1"],
  PRF: ["Frequency|Number", "Hz -- \"must be greater than zero\", mas default é 0.0 (### NES no próprio header)"],
  pulseWidth: ["Time|Number", "segundos -- mesma inconsistência de PRF"],
  beamWidth: ["Angle|Number", "(Deprecated: moved to Antenna) radianos -- default D2RCC*3.5"],
  typeId: ["String", "ID de tipo R/F, para PDU de emissão eletromagnética DIS -- default '\\0'"],
  syncXmitWithScan: ["Number(bool)", "sincroniza transmissor com a varredura da antena -- default false"],
};

const REF_RADAR_SLOT_DOCS = {
  igain: ["Number|Decibel", "ganho do integrador, ≥ 1.0 -- default 1.0"],
};

const REF_SAR_SLOT_DOCS = {
  chipSize: ["Number", "pixels -- ver achado: lido/escrito, NUNCA consultado na geração da imagem"],
};

function RfSensorReferencePage({ onOpenCatalog }) {
  const rfsEntry = MODEL["RfSystem"];
  const rfsrEntry = MODEL["RfSensor"];
  const radarEntry = MODEL["Radar"];
  const sarEntry = MODEL["Sar"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>RfSensor / Radar / Rwr / Sar / Jammer</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "RfSystem" → "RfSensor"</span>
          <span className="mx-chip">+ Radar → Sar, Rwr, Jammer, SensorMgr</span>
          {rfsrEntry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("RfSensor")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          A cadeia de DETECÇÃO -- separada da de APONTAMENTO (aba Gimbal/Antenna) por composição, não
          herança: <code className="mx-mono">RfSystem</code> só tem um <code className="mx-mono">Antenna*</code>{" "}
          resolvido por NOME. Cinco folhas concretas, cada uma estruturalmente enviesada: {" "}
          <code className="mx-mono">Radar</code> transmite E recebe; <code className="mx-mono">Rwr</code> só{" "}
          RECEBE (nunca sobrescreve <code className="mx-mono">transmit()</code>); <code className="mx-mono">Jammer</code>{" "}
          só TRANSMITE (receptor desligado no construtor); <code className="mx-mono">Sar</code> nasce com os dois
          desligados, ligando o transmissor só durante a "exposição"; <code className="mx-mono">SensorMgr</code>{" "}
          não faz NADA sozinho -- é um nó de <code className="mx-mono">components:</code> que hospeda outros
          sensores, sem fan-out manual nenhum.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {radarEntry ? radarEntry.ch.join(" → ") : "Radar → RfSensor → RfSystem → System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de RfSensor" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>RfSystem::rfReceivedEmission() -- o ÚNICO ponto de entrada</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Toda emissão recebida por QUALQUER antena passa por aqui: filtro de banda
                (<code className="mx-mono">affectsRfSystem()</code>), cálculo do sinal de uma perna
                (<code className="mx-mono">power × rangeLoss × antGain / losses</code>), e -- só se a emissão for{" "}
                <code className="mx-mono">ECM_NOISE</code> -- soma direto em <code className="mx-mono">jamSignal</code>{" "}
                (aqui, não em <code className="mx-mono">receive()</code>, porque a perda de recepção só é aplicada
                depois). O resto vira dois arrays paralelos (<code className="mx-mono">packets[]</code>/{" "}
                <code className="mx-mono">signals[]</code>) que <code className="mx-mono">receive()</code>{" "}
                (fase 2, de cada subclasse) drena depois -- fila cheia (<code className="mx-mono">MAX_EMISSIONS</code>)
                descarta em silêncio.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado -- o próprio header já registra a inconsistência (não é leitura nova):</b>{" "}
                <code className="mx-mono">RfSensor.hpp</code> documenta{" "}
                <code className="mx-mono">PRF</code>/<code className="mx-mono">pulseWidth</code> como "must be
                greater than zero" mas com "(default: 0.0)" -- e o comentário literal ao lado diz{" "}
                <code className="mx-mono">### NES: Initial value not greater than 0)</code>. Na prática, um{" "}
                <code className="mx-mono">RfSensor</code> sem <code className="mx-mono">PRF</code> no{" "}
                <code className="mx-mono">.edl</code> nasce com o campo zerado -- e{" "}
                <code className="mx-mono">Radar::transmit()</code> calcula{" "}
                <code className="mx-mono">pulses = round(PRF·dt+0.5)</code>, que com <code className="mx-mono">PRF=0</code>{" "}
                dá <code className="mx-mono">pulses=0</code>, corrigido na hora por{" "}
                <code className="mx-mono">if (pulses==0) pulses=1</code> -- o sensor "funciona" com um pulso
                fictício em vez de falhar visivelmente.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Radar -- o único que fecha o ciclo E limpa jamSignal</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">receive()</code> só aceita eco PRÓPRIO
                (<code className="mx-mono">em-&gt;getTransmitter()==this</code>) ou ECM NÃO-ruído -- jamming de
                ruído já foi contabilizado em <code className="mx-mono">jamSignal</code>, não é reprocessado como
                retorno individual. Um relatório só entra na fila se{" "}
                <code className="mx-mono">S/I ≥ threshold</code> <b>E</b> alcance ≤ 125% do{" "}
                <code className="mx-mono">maxRange</code> configurado; sem isso mas com{" "}
                <code className="mx-mono">S/N ≥ threshold</code>, conta como "emissão jammed". Ao final de{" "}
                <code className="mx-mono">receive()</code>, <code className="mx-mono">jamSignal = 0</code> -- é a
                ÚNICA das cinco folhas que de fato drena esse acumulador.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Rwr -- estruturalmente só-receptor, e rejeita ECM</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                O construtor só liga o receptor -- <code className="mx-mono">Rwr</code> nunca sobrescreve{" "}
                <code className="mx-mono">transmit()</code> (herda o vazio de <code className="mx-mono">System</code>):
                um RWR neste framework nunca emite nada próprio. Ao contrário de{" "}
                <code className="mx-mono">Radar</code>, <code className="mx-mono">receive()</code>{" "}
                REJEITA qualquer emissão marcada ECM (<code className="mx-mono">!em-&gt;isECM()</code> no gate) e
                entrega o relatório IMEDIATAMENTE ao <code className="mx-mono">TrackManager</code> dentro da
                própria fase 2 -- <code className="mx-mono">Radar</code> só entrega no fim de varredura, fase 3.{" "}
                <code className="mx-mono">Rwr</code> também nunca lê nem zera <code className="mx-mono">jamSignal</code>{" "}
                (herdado de <code className="mx-mono">RfSystem</code>) -- um jammer de ruído continuamente
                iluminando um RWR acumula esse campo sem limite, sem efeito nenhum porque ninguém o consulta ali.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Jammer -- um flag no Emission é toda a diferença</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Construtor liga transmissor, desliga receptor, seta <code className="mx-mono">typeId="JAMMER"</code>.{" "}
                <code className="mx-mono">transmit()</code> é o ÚNICO método sobrescrito: monta um{" "}
                <code className="mx-mono">Emission</code> comum e faz{" "}
                <code className="mx-mono">em-&gt;setECM(Emission::ECM_NOISE)</code> -- não há tipo C++ nem flag no{" "}
                <code className="mx-mono">RfSystem</code> receptor que marque "isto é jamming"; é só esse campo de
                dado no próprio <code className="mx-mono">Emission</code>, checado em três lugares diferentes
                (<code className="mx-mono">RfSystem::rfReceivedEmission</code>, <code className="mx-mono">
                Radar::receive</code>, o gate de exclusão de <code className="mx-mono">Rwr::receive</code>).
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado, Sar:</b> <code className="mx-mono">requestImage()</code>/<code className="mx-mono">addImage()</code>{" "}
                declaram <code className="mx-mono">bool ok{"{}"}</code> e NUNCA fazem{" "}
                <code className="mx-mono">ok = true</code> em nenhum ramo -- o valor de retorno é sistematicamente{" "}
                <code className="mx-mono">false</code>, mesmo quando a exposição é armada com sucesso. Além disso:{" "}
                <code className="mx-mono">chipSize</code> é lido/escrito e participa de{" "}
                <code className="mx-mono">copyData()</code>, mas NENHUM ponto do arquivo o consulta ao gerar a
                imagem de teste (<code className="mx-mono">testImage()</code> usa só{" "}
                <code className="mx-mono">width</code>/<code className="mx-mono">height</code>); e{" "}
                <code className="mx-mono">busy</code> nunca recebe <code className="mx-mono">true</code> em lugar
                nenhum -- <code className="mx-mono">isImagingInProgress()</code> depende só de{" "}
                <code className="mx-mono">timer &gt; 0</code>.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>SensorMgr -- "gerenciar uma lista" é herdar a recursão de Component, nada escrito à mão</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Não sobrescreve <code className="mx-mono">process()</code>/<code className="mx-mono">receive()</code>/{" "}
                <code className="mx-mono">transmit()</code>/<code className="mx-mono">updateData()</code>/{" "}
                <code className="mx-mono">reset()</code> -- zero. Cada sensor dentro do seu{" "}
                <code className="mx-mono">components:</code> (<code className="mx-mono">Tws</code>,{" "}
                <code className="mx-mono">Stt</code>, <code className="mx-mono">Gmti</code>...) é ele próprio um{" "}
                <code className="mx-mono">System</code> completo, computando a PRÓPRIA fase através da mesma
                recursão genérica de <code className="mx-mono">Component::updateTC()</code> já documentada na aba
                System -- <code className="mx-mono">SensorMgr</code> só existe como o nó intermediário que permite
                um `Player` apontar pra UM container só onde vários sensores convivem. O arquivo inteiro (ver
                Código-fonte) tem 19 linhas.
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 380px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de RfSystem</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{rfsEntry ? rfsEntry.own : 12} slots próprios.</p>
              <div className="mx-slotgrid">
                {(rfsEntry ? rfsEntry.sl : Object.keys(REF_RFSYSTEM_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_RFSYSTEM_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_RFSYSTEM_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_RFSYSTEM_SLOT_DOCS[s] ? REF_RFSYSTEM_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 380px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de RfSensor</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{rfsrEntry ? rfsrEntry.own : 9} slots próprios.</p>
              <div className="mx-slotgrid">
                {(rfsrEntry ? rfsrEntry.sl : Object.keys(REF_RFSENSOR_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_RFSENSOR_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_RFSENSOR_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_RFSENSOR_SLOT_DOCS[s] ? REF_RFSENSOR_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 220px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Radar</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{radarEntry ? radarEntry.own : 1} slot próprio.</p>
              <div className="mx-slotgrid">
                {(radarEntry ? radarEntry.sl : Object.keys(REF_RADAR_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_RADAR_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_RADAR_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 260 }}>{REF_RADAR_SLOT_DOCS[s] ? REF_RADAR_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 220px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Sar</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{sarEntry ? sarEntry.own : 1} slot próprio.</p>
              <div className="mx-slotgrid">
                {(sarEntry ? sarEntry.sl : Object.keys(REF_SAR_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SAR_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SAR_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 260 }}>{REF_SAR_SLOT_DOCS[s] ? REF_SAR_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: 0 }}>
                <code className="mx-mono">Rwr</code>, <code className="mx-mono">Jammer</code> e{" "}
                <code className="mx-mono">SensorMgr</code> declaram <code className="mx-mono">EMPTY_SLOTTABLE</code> --
                zero slots próprios; herdam os 9+12 de <code className="mx-mono">RfSensor</code>/<code className="mx-mono">RfSystem</code>.
              </p>
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">RfSystem::rfReceivedEmission() -- na íntegra</span><span>C++</span></div>
              {renderRfSensorSnippet("RfSystem::rfReceivedEmission -- fila de emissao + acumulo de jamSignal")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Radar::transmit() -- na íntegra</span><span>C++</span></div>
              {renderRfSensorSnippet("Radar::transmit -- pulses cai para 1 quando PRF*dt arredonda a zero")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Radar::receive() -- início (fila, interferência, filtro de eco próprio/ECM)</span><span>C++</span></div>
              {renderRfSensorSnippet("Radar::receive -- inicio (fila, interferencia, filtro de eco proprio/ECM)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Radar::receive() -- o gate de S/I a 125% do alcance, e o reset de jamSignal</span><span>C++</span></div>
              {renderRfSensorSnippet("Radar::receive -- o gate de S/I a 125% do alcance, e o reset de jamSignal")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Rwr::receive() -- rejeita ECM, nunca lê/zera jamSignal, entrega na fase 2</span><span>C++</span></div>
              {renderRfSensorSnippet("Rwr::receive -- rejeita ECM, nunca le/zera jamSignal, entrega na fase 2")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Jammer -- construtor + transmit(), na íntegra</span><span>C++</span></div>
              {renderRfSensorSnippet("Jammer::transmit -- ECM_NOISE e o UNICO sinal de \"isto e um jammer\"")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Sar::requestImage()/cancel() -- 'ok' nunca vira true</span><span>C++</span></div>
              {renderRfSensorSnippet("Sar::requestImage/cancel -- 'ok' nunca vira true, mesmo com sucesso")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">SensorMgr.cpp -- o arquivo inteiro</span><span>C++</span></div>
              {renderRfSensorSnippet("SensorMgr -- o arquivo inteiro. 'gerenciar' e so herdar a arvore de components:")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ====================== Referência -- TrackManager e a família de pistas =========================
 * Terceira das quatro entradas de RF. TrackManager é a base (fila de emissão, lista de
 * pistas) -- mas ela e a irmã AngleOnlyTrackManager NUNCA são construíveis via EDL, apesar
 * do nome de fábrica bater com o header (o mesmo tipo de trap já achado em Player). Quatro
 * folhas concretas (AirTrkMgr/GmtiTrkMgr/RwrTrkMgr/AirAngleOnlyTrkMgr) e uma QUINTA,
 * AirAngleOnlyTrkMgrPT, que é totalmente concreta em C++ mas AINDA ASSIM inalcançável --
 * o achado mais nítido desta página, já batido por este próprio repositório.
 * ==================================================================================== */

const TRACKMGR_SNIPPETS = {
  "TrackManager::newReport -- a fila que toda folha concreta alimenta": {
    file: "contexts/src/mixr/src/models/system/trackmanager/TrackManager.cpp",
    line: 301,
    trunc: false,
    lines: [
      "//------------------------------------------------------------------------------",
      "// newReport() -- Accept a new emission report",
      "//------------------------------------------------------------------------------",
      "void TrackManager::newReport(Emission* em, double sn)",
      "{",
      "   // Queue up emissions reports",
      "   if (em != nullptr) {",
      "      base::lock(queueLock);",
      "      if (emQueue.isNotFull()) {",
      "      em->ref();",
      "      emQueue.put(em);",
      "      snQueue.put(sn);",
      "      }",
      "      base::unlock(queueLock);",
      "   }",
      "}",
    ],
  },
  "models::factory.cpp -- so 4 das 7 classes tem despacho (o smoking gun)": {
    file: "contexts/src/mixr/src/models/factory.cpp",
    line: 434,
    trunc: false,
    lines: [
      "   // Tracks",
      "   else if ( name == Track::getFactoryName() ) {",
      "      obj = new Track();",
      "   }",
      "",
      "   // Track Managers",
      "   else if ( name == GmtiTrkMgr::getFactoryName() ) {",
      "      obj = new GmtiTrkMgr();",
      "   }",
      "   else if ( name == AirTrkMgr::getFactoryName() ) {",
      "      obj = new AirTrkMgr();",
      "   }",
      "   else if ( name == RwrTrkMgr::getFactoryName() ) {",
      "      obj = new RwrTrkMgr();",
      "   }",
      "   else if ( name == AirAngleOnlyTrkMgr::getFactoryName() ) {",
      "      obj = new AirAngleOnlyTrkMgr();",
      "   }",
    ],
  },
  "AirTrkMgr -- correlacao por IGUALDADE DE PONTEIRO (ground truth), nao geometria": {
    file: "contexts/src/mixr/src/models/system/trackmanager/AirTrkMgr.cpp",
    line: 182,
    trunc: false,
    lines: [
      "   // ---",
      "   // 3) Match current tracks to new reports (observations)",
      "   // ---",
      "   base::lock(trkListLock);",
      "   for (unsigned int it = 0; it < nTrks; it++) {",
      "      trackNumMatches[it] = 0;",
      "      const RfTrack* const trk{static_cast<const RfTrack*>(tracks[it])};  // we produce only RfTracks",
      "      const Player* const tgt{trk->getLastEmission()->getTarget()};",
      "      for (unsigned int ir = 0; ir < nReports; ir++) {",
      "         if (emissions[ir]->getTarget() == tgt) {",
      "            // We have a new report for the same target as this track ...",
      "            report2TrackMatch[ir][it] = true;",
      "            trackNumMatches[it]++;",
      "            reportNumMatches[ir]++;",
      "         }",
      "         else report2TrackMatch[ir][it] = false;",
      "      }",
      "   }",
    ],
  },
  "AirTrkMgr -- gamma comentado (morto) e o gate de salto grande": {
    file: "contexts/src/mixr/src/models/system/trackmanager/AirTrkMgr.cpp",
    line: 258,
    trunc: false,
    lines: [
      "      if (haveU[i]) {",
      "         // Have Input vector U, use ...",
      "         // where B is ...",
      "         double b0{alpha};",
      "         double b1{};",
      "         if (age[i] != 0) b1 = beta / age[i];",
      "         double b2{};",
      "         //double b2 = gamma * 2.0f / (age[i]*age[i]);",
      "         if (u[i].length2() > d2) {",
      "            // Large position change: just set position",
      "            b0 = 1.0;",
      "            b1 = 0.0;",
      "         }",
      "",
      "         // X(k+1) = A*X(k) + B*U(k)",
      "         tracks[i]->setPosition(     (tpos*A[0][0] + tvel*A[0][1] + tacc*A[0][2]) + (u[i]*b0) );",
      "         tracks[i]->setVelocity(     (tpos*A[1][0] + tvel*A[1][1] + tacc*A[1][2]) + (u[i]*b1) );",
    ],
  },
  "AirTrkMgr::setSlotRangeGate -- 'ok = true' no ramo de erro": {
    file: "contexts/src/mixr/src/models/system/trackmanager/AirTrkMgr.cpp",
    line: 393,
    trunc: false,
    lines: [
      "bool AirTrkMgr::setSlotRangeGate(const base::Number* const num)",
      "{",
      "   double value{};",
      "   const auto p = dynamic_cast<const base::Distance*>(num);",
      "   if (p != nullptr) {",
      "      // We have a distance and we want it in meters ...",
      "      base::Meters meters;",
      "      value = meters.convert(*p);",
      "   }",
      "   else if (num != nullptr) {",
      "      // We have only a number, assume it's in meters ...",
      "      value = num->getReal();",
      "   }",
      "",
      "   // Set the value if it's valid",
      "   bool ok{true};",
      "   if (value > 0.0) {",
      "      rngGate = value;",
      "   }",
      "   else {",
      "      std::cerr << \"TrackManager::setRangeGate: invalid gate, must be greater than zero.\" << std::endl;",
      "      ok = true;",
      "   }",
      "   return ok;",
      "}",
    ],
  },
  "RwrTrkMgr -- \"a ownship da emissao E o nosso alvo\"": {
    file: "contexts/src/mixr/src/models/system/trackmanager/RwrTrkMgr.cpp",
    line: 125,
    trunc: false,
    lines: [
      "   double newSignal[MAX_REPORTS]{};",
      "   double newRdot[MAX_REPORTS]{};",
      "   base::Vec3d tgtPos[MAX_REPORTS];",
      "   double tmp{};",
      "   for (Emission* em = getReport(&tmp); em != nullptr; em = getReport(&tmp)) {",
      "      if (nReports < MAX_REPORTS) {",
      "         // save the report",
      "         Player* tgt{em->getOwnship()};  // The emissions ownship is our target!",
      "         emissions[nReports] = em;",
      "         newSignal[nReports] = tmp;",
      "         newRdot[nReports] = emissions[nReports]->getRangeRate();",
      "         reportNumMatches[nReports] = 0;",
      "         tgtPos[nReports] = tgt->getPosition() - ownship->getPosition();",
      "         nReports++;",
      "      } else {",
      "         // ignore -- too many reports",
      "         em->unref();",
      "      }",
      "   }",
      "",
      "   // ---",
      "   // 3) Match current tracks to new reports (observations)",
      "   // ---",
      "   base::lock(trkListLock);",
      "   for (unsigned int it = 0; it < nTrks; it++) {",
      "      trackNumMatches[it] = 0;",
      "      const RfTrack* const trk{static_cast<const RfTrack*>(tracks[it])};        // we produce only RfTracks",
      "      const Player* const tgt{trk->getLastEmission()->getOwnship()};            // The emissions ownship is our target!",
      "      for (unsigned int ir = 0; ir < nReports; ir++) {",
      "         if (emissions[ir]->getOwnship() == tgt) {  // The emissions ownship is our target!",
      "            // We have a new report for the same target as this track ...",
      "            report2TrackMatch[ir][it] = true;",
      "            trackNumMatches[it]++;",
      "            reportNumMatches[ir]++;",
      "         }",
      "         else report2TrackMatch[ir][it] = false;",
      "      }",
      "   }",
    ],
  },
  "AirAngleOnlyTrkMgrPT -- o UNICO gate geometrico de verdade da familia": {
    file: "contexts/src/mixr/src/models/system/trackmanager/AirAngleOnlyTrkMgrPT.cpp",
    line: 169,
    trunc: false,
    lines: [
      "            // ---",
      "            // 3) Match new reports (observations) to all potential track matches",
      "            // ---",
      "            base::lock(trkListLock);",
      "            for (unsigned int it = 0; it < nTrks; it++) {",
      "                trackNumMatches[it] = 0;",
      "                const IrTrack* const trk{static_cast<const IrTrack*>(tracks[it])};  // we produce only IrTracks",
      "                for (unsigned int ir = 0; ir < nReports; ir++) {",
      "                    double azDiff{queryMessages[ir]->getRelativeAzimuth() - trk->getPredictedAzimuth()};",
      "                    if (azDiff < 0.0f) azDiff = 0.0f - azDiff;",
      "                    double elDiff{queryMessages[ir]->getRelativeElevation() - trk->getPredictedElevation()};",
      "                    if (elDiff < 0.0f) elDiff = 0.0f - elDiff;",
      "                    if ((azDiff < azimuthBin) && (elDiff < elevationBin)) {",
      "                        // We have a new report for the same target as this track ...",
      "                        report2TrackMatch[ir][it] = 1;",
      "                        trackNumMatches[it]++;",
      "                        reportNumMatches[ir]++;",
      "                    } else",
      "                        report2TrackMatch[ir][it] = 0;",
      "                }",
      "            }",
    ],
  },
  "Track::setPosition -- alcance/marcacao/LOS derivados automaticamente": {
    file: "contexts/src/mixr/src/models/Track.cpp",
    line: 249,
    trunc: false,
    lines: [
      "// setPosition() -- set track's position vector",
      "bool Track::setPosition(const base::Vec3d& p)",
      "{",
      "   // set position vector",
      "   pos = p;",
      "",
      "   // compute ranges",
      "   const double gndRng2{pos.x()*pos.x() + pos.y()*pos.y()};",
      "   gndRng = std::sqrt(gndRng2);",
      "   rng = std::sqrt(gndRng2 +  pos.z()*pos.z());",
      "",
      "   // compute angles",
      "   taz = std::atan2(pos.y(),pos.x());",
      "   raz[0] = base::angle::aepcdRad(taz - osGndTrk);",
      "   rel[0] = std::atan2(-pos.z(), gndRng);",
      "",
      "   // Set LOS unit vector",
      "   if (rng > 0) los.set( pos.x()/rng, pos.y()/rng, pos.z()/rng );",
      "   else los.set(0,0,0);",
      "",
      "   return true;",
      "}",
    ],
  },
};

const trackMgrSnip = (key) => (key ? TRACKMGR_SNIPPETS[key] || null : null);

function renderTrackMgrSnippet(key) {
  const snip = trackMgrSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_TRACKMANAGER_SLOT_DOCS = {
  maxTracks: ["Number", "default 200 (MAX_TRKS)"],
  maxTrackAge: ["Time|Number", "segundos -- default 3.0"],
  firstTrackId: ["Number", "default 1000"],
  alpha: ["Number", "ganho de posição do filtro α-β-γ -- default 1.0"],
  beta: ["Number", "ganho de velocidade -- default 0.0"],
  gamma: ["Number", "ganho de aceleração -- ver achado: MORTO em toda subclasse concreta"],
  logTrackUpdates: ["Number(bool)", "default true"],
};

const REF_AIRTRKMGR_SLOT_DOCS = {
  positionGate: ["Number|Distance", "metros -- default 2·NM2M (~3704 m); único gate REALMENTE lido"],
  rangeGate: ["Number|Distance", "metros -- default 500.0; slot MORTO, nunca consultado em processTrackList()"],
  velocityGate: ["Number", "m/s -- default 10.0; slot MORTO, mesmo caso de rangeGate"],
};

const REF_ANGLEONLY_SLOT_DOCS = {
  azimuthBin: ["Number|Angle", "rad -- default π (180°)"],
  elevationBin: ["Number|Angle", "rad -- default π"],
};

function TrackManagerReferencePage({ onOpenCatalog }) {
  const tmEntry = MODEL["TrackManager"];
  const airEntry = MODEL["AirTrkMgr"];
  const aoEntry = MODEL["AngleOnlyTrackManager"];
  const trackEntry = MODEL["Track"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>TrackManager</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "TrackManager" -- não construível</span>
          <span className="mx-chip">4 folhas construíveis + 1 fantasma (PT)</span>
          {tmEntry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("TrackManager")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          O que acontece com uma detecção depois que um <code className="mx-mono">RfSensor</code> a entrega
          (aba anterior): vira um <code className="mx-mono">Track</code>. <code className="mx-mono">TrackManager</code>{" "}
          é a base -- lista de até <code className="mx-mono">maxTracks</code>, fila de{" "}
          <code className="mx-mono">Emission</code>/<code className="mx-mono">IrQueryMsg</code>, um filtro
          α-β-γ genérico -- mas <code className="mx-mono">processTrackList()</code> é puro virtual: CADA
          subclasse decide sozinha o que conta como "a mesma pista". Quatro folhas concretas resolvem isso
          por IGUALDADE DE PONTEIRO (ground truth, não geometria); só a quinta, citada abaixo, usa um gate
          angular de verdade.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {airEntry ? airEntry.ch.join(" → ") : "AirTrkMgr → TrackManager → System → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de TrackManager" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado central desta página -- já batido pelo próprio repositório, não hipotético:</b>{" "}
                <code className="mx-mono">TrackManager</code>/<code className="mx-mono">AngleOnlyTrackManager</code>{" "}
                usam <code className="mx-mono">IMPLEMENT_PARTIAL_SUBCLASS</code> com nome de fábrica que BATE
                com o header, mas <code className="mx-mono">models::factory.cpp</code> só tem branch de despacho
                para 4 classes (<code className="mx-mono">GmtiTrkMgr</code>/<code className="mx-mono">AirTrkMgr</code>/{" "}
                <code className="mx-mono">RwrTrkMgr</code>/<code className="mx-mono">AirAngleOnlyTrkMgr</code>) --
                as duas bases ficam <code className="mx-mono">clone()</code>-nulas, de fato abstratas. Mas o caso
                mais nítido é a QUINTA classe, <code className="mx-mono">AirAngleOnlyTrkMgrPT</code>: ela é{" "}
                <b>totalmente concreta</b> em C++ (<code className="mx-mono">IMPLEMENT_SUBCLASS</code> de
                verdade, <code className="mx-mono">clone()</code> funcional) -- mas SEM branch em{" "}
                <code className="mx-mono">factory.cpp</code> mesmo assim. Este próprio repositório já bateu nisso:
                um comentário em <code className="mx-mono">tests/fixtures/built-in_mixr_1/configs/
                scenario_max_player.edl.in</code> registra que <code className="mx-mono">MergingIrSensor::reset()</code>{" "}
                EXIGE um <code className="mx-mono">( AirAngleOnlyTrkMgrPT )</code> e avisa em toda partida quando
                não acha um -- e essa classe simplesmente não tem como ser criada via EDL neste fork.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Correlação é por PONTEIRO do alvo (ground truth), não por geometria</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">AirTrkMgr</code>/<code className="mx-mono">GmtiTrkMgr</code> casam
                relatório↔pista comparando <code className="mx-mono">emissions[ir]-&gt;getTarget() == tgt</code> --
                o ponteiro C++ do <code className="mx-mono">Player</code> real, não uma janela de
                posição/velocidade. Não há nearest-neighbor gating na associação em si;{" "}
                <code className="mx-mono">positionGate</code> só entra DEPOIS, como "gate de salto grande" -- se
                a correção excede o gate, descarta a suavização e trava direto na posição observada.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>gamma é slot morto em TODAS as quatro folhas concretas</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                O termo que <code className="mx-mono">gamma</code> multiplicaria (<code className="mx-mono">b2</code>)
                é sempre <code className="mx-mono">double b2{"{}"}</code> (zero), com a fórmula real COMENTADA --{" "}
                <code className="mx-mono">//double b2 = gamma * 2.0f / (age[i]*age[i]);</code> -- idêntica em{" "}
                <code className="mx-mono">AirTrkMgr</code> e <code className="mx-mono">GmtiTrkMgr</code>;{" "}
                em <code className="mx-mono">RwrTrkMgr</code> nem o comentário sobrevive. Configurável no{" "}
                <code className="mx-mono">.edl</code>, aceito sem erro, nunca influencia nada.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Bug confirmado:</b> <code className="mx-mono">AirTrkMgr::setSlotRangeGate()</code> (e os
                irmãos <code className="mx-mono">setSlotVelocityGate()</code>) declaram{" "}
                <code className="mx-mono">bool ok{"{true}"}</code> e reatribuem{" "}
                <code className="mx-mono">ok = true</code> no PRÓPRIO ramo de erro (deveria ser{" "}
                <code className="mx-mono">false</code>) -- um valor inválido é logado em{" "}
                <code className="mx-mono">stderr</code> mas o parser EDL recebe sucesso, e o valor
                antigo/default fica mantido em silêncio. A mensagem de erro também nomeia a classe errada:{" "}
                "TrackManager::setRangeGate", não <code className="mx-mono">AirTrkMgr::setSlotRangeGate</code> --
                a mesma classe de descuido já achada em <code className="mx-mono">Radar::setSlotIGain()</code>{" "}
                (aba anterior). E, à parte do bug: <code className="mx-mono">rangeGate</code>/{" "}
                <code className="mx-mono">velocityGate</code> são slots MORTOS -- nenhuma ocorrência de{" "}
                <code className="mx-mono">rngGate</code>/<code className="mx-mono">velGate</code> dentro de{" "}
                <code className="mx-mono">processTrackList()</code> além do próprio setter/getter.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>RwrTrkMgr -- "a ownship da emissão É o nosso alvo"</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Para um RWR, o <code className="mx-mono">Emission</code> chega de um radar INIMIGO iluminando o
                ownship -- o "alvo" da pista RWR é o EMISSOR, não quem foi irradiado (comentário literal repetido
                4× no arquivo: "The emissions ownship is our target!"). Apesar do nome,{" "}
                <code className="mx-mono">RwrTrkMgr</code> NÃO é angle-only -- guarda posição relativa 3D
                completa (<code className="mx-mono">tgt-&gt;getPosition() - ownship-&gt;getPosition()</code>)
                porque a simulação conhece a posição verdadeira do emissor. A família angle-only DE VERDADE é a
                separada <code className="mx-mono">AngleOnlyTrackManager</code>/<code className="mx-mono">
                AirAngleOnlyTrkMgr*</code>, usada por sensores IR.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>AirAngleOnlyTrkMgrPT -- o único gate geométrico de verdade</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Reimplementa <code className="mx-mono">processTrackList()</code> do zero, com um algoritmo
                escrito à mão para lidar com "merged tracks" de um <code className="mx-mono">MergingIrSensor</code>{" "}
                (vários alvos colapsados num único retorno IR): casa relatório↔pista por{" "}
                <b>diferença de ângulo</b> (<code className="mx-mono">|az-azPredito| &lt; azimuthBin</code>{" "}
                <b>E</b> o mesmo em elevação) -- os ÚNICOS dois slots de{" "}
                <code className="mx-mono">AngleOnlyTrackManager</code> que alguma subclasse de fato lê. A
                irmã <code className="mx-mono">AirAngleOnlyTrkMgr</code> (sem "PT") HERDA{" "}
                <code className="mx-mono">azimuthBin</code>/<code className="mx-mono">elevationBin</code> mas
                correlaciona por ponteiro, igual às outras três -- os slots ficam mortos lá.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>Track -- geometria derivada automaticamente, qualidade nunca escrita</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">setPosition()</code> recalcula alcance/marcação/LOS toda vez que a
                posição muda -- ninguém precisa lembrar de atualizar os três separadamente.{" "}
                <code className="mx-mono">IrTrack::setPosition()</code> SOBRESCREVE para NÃO recalcular az/el
                (comentário "but do not set rel az or el") -- faz sentido: numa pista angle-only, az/el vêm
                direto do sensor, não são derivados de uma posição 3D confiável.{" "}
                <code className="mx-mono">quality</code>/<code className="mx-mono">setQuality()</code> nunca é
                escrito por NENHUM <code className="mx-mono">TrackManager</code> desta família -- fica para
                sempre em <code className="mx-mono">0.0</code>; não existe decaimento de qualidade nesta família,
                apesar do slot table sugerir um conceito de "qualidade de pista" ativo.
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 380px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de TrackManager</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{tmEntry ? tmEntry.own : 7} slots próprios -- herdados por todas as subclasses.</p>
              <div className="mx-slotgrid">
                {(tmEntry ? tmEntry.sl : Object.keys(REF_TRACKMANAGER_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_TRACKMANAGER_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_TRACKMANAGER_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 320 }}>{REF_TRACKMANAGER_SLOT_DOCS[s] ? REF_TRACKMANAGER_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 320px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de AirTrkMgr</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{airEntry ? airEntry.own : 3} slots próprios -- 2 deles mortos.</p>
              <div className="mx-slotgrid">
                {(airEntry ? airEntry.sl : Object.keys(REF_AIRTRKMGR_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_AIRTRKMGR_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_AIRTRKMGR_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 300 }}>{REF_AIRTRKMGR_SLOT_DOCS[s] ? REF_AIRTRKMGR_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 280px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>Slots próprios de AngleOnlyTrackManager</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{aoEntry ? aoEntry.own : 2} slots próprios -- só PT de fato os lê.</p>
              <div className="mx-slotgrid">
                {(aoEntry ? aoEntry.sl : Object.keys(REF_ANGLEONLY_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_ANGLEONLY_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_ANGLEONLY_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 280 }}>{REF_ANGLEONLY_SLOT_DOCS[s] ? REF_ANGLEONLY_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: 0 }}>
                <code className="mx-mono">GmtiTrkMgr</code>, <code className="mx-mono">RwrTrkMgr</code>,{" "}
                <code className="mx-mono">AirAngleOnlyTrkMgr</code> e <code className="mx-mono">AirAngleOnlyTrkMgrPT</code>{" "}
                declaram <code className="mx-mono">EMPTY_SLOTTABLE</code> -- zero slots próprios; herdam da base
                mais próxima. <code className="mx-mono">Track</code>/<code className="mx-mono">RfTrack</code>/{" "}
                <code className="mx-mono">IrTrack</code> também não têm slot algum -- nunca são criados via
                EDL, só por <code className="mx-mono">new</code> direto dentro de um{" "}
                <code className="mx-mono">TrackManager</code>.
              </p>
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">TrackManager::newReport() -- na íntegra</span><span>C++</span></div>
              {renderTrackMgrSnippet("TrackManager::newReport -- a fila que toda folha concreta alimenta")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">models::factory.cpp -- só 4 das 7 classes desta família têm despacho</span><span>C++</span></div>
              {renderTrackMgrSnippet("models::factory.cpp -- so 4 das 7 classes tem despacho (o smoking gun)")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">AirTrkMgr -- correlação por ponteiro do alvo (passo 3)</span><span>C++</span></div>
              {renderTrackMgrSnippet("AirTrkMgr -- correlacao por IGUALDADE DE PONTEIRO (ground truth), nao geometria")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">AirTrkMgr -- gamma comentado (morto) + gate de salto grande</span><span>C++</span></div>
              {renderTrackMgrSnippet("AirTrkMgr -- gamma comentado (morto) e o gate de salto grande")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">AirTrkMgr::setSlotRangeGate() -- 'ok = true' no ramo de erro</span><span>C++</span></div>
              {renderTrackMgrSnippet("AirTrkMgr::setSlotRangeGate -- 'ok = true' no ramo de erro")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">RwrTrkMgr -- "the emissions ownship is our target!"</span><span>C++</span></div>
              {renderTrackMgrSnippet("RwrTrkMgr -- \"a ownship da emissao E o nosso alvo\"")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">AirAngleOnlyTrkMgrPT -- o gate por diferença de ângulo (passo 3)</span><span>C++</span></div>
              {renderTrackMgrSnippet("AirAngleOnlyTrkMgrPT -- o UNICO gate geometrico de verdade da familia")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Track::setPosition() -- na íntegra</span><span>C++</span></div>
              {renderTrackMgrSnippet("Track::setPosition -- alcance/marcacao/LOS derivados automaticamente")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

/* ====================== Referência -- RfSignature (7 formas de RCS) + Emission ==================
 * Quarta e última entrada de RF. RfSignature é a interface mais enxuta desta Referência
 * inteira -- UM método puro virtual, getRCS(Emission*) -- e as sete formas concretas variam
 * de "constante" a "tabela 2D interpolada". Emission é o objeto que atravessa o ciclo inteiro
 * (Antenna -> Player alvo -> de volta) SEM ser clonado -- o mesmo ponteiro carrega ida e
 * volta, e o termo 1/R^4 da equação de radar emerge como produto de duas contas de 1/(4piR^2)
 * independentes, feitas em pontos diferentes do pipeline, nunca escrito como R^4 em lugar nenhum.
 * ==================================================================================== */

const RFSIGNATURE_SNIPPETS = {
  "RfSignature -- interface + o nome de fabrica real e 'Signature'": {
    file: "contexts/src/mixr/include/mixr/models/Signatures.hpp",
    line: 20,
    trunc: false,
    lines: [
      "class RfSignature : public base::Component",
      "{",
      "    DECLARE_SUBCLASS(RfSignature, base::Component)",
      "public:",
      "    RfSignature();",
      "    virtual double getRCS(const Emission* const em)=0;",
      "};",
      "",
      "// -- src/models/Signatures.cpp:",
      "// IMPLEMENT_ABSTRACT_SUBCLASS(RfSignature, \"Signature\")   <- nao \"RfSignature\"",
    ],
  },
  "SigConstant::getRCS/setRCS -- ignora o Emission, aceita Number OU Decibel": {
    file: "contexts/src/mixr/src/models/Signatures.cpp",
    line: 77,
    trunc: false,
    lines: [
      "double SigConstant::getRCS(const Emission* const)",
      "{",
      "    return rcs;",
      "}",
      "",
      "bool SigConstant::setRCS(const base::Number* const num)",
      "{",
      "    bool ok{};",
      "    double r{-1.0};",
      "",
      "    const auto d = dynamic_cast<const base::Area*>(num);",
      "    if (d != nullptr) {",
      "        // Has area units and we need square meters",
      "        base::SquareMeters m2;",
      "        r = m2.convert(*d);",
      "    } else if (num != nullptr) {",
      "        // square meters (Number or Decibel)",
      "        r = num->getReal();",
      "    }",
      "",
      "    if (r >= 0.0) { rcs = r; ok = true; }",
      "    else { std::cerr << \"SigConstant::setRCS: invalid rcs; must be greater than or equal to zero!\" << std::endl; }",
      "    return ok;",
      "}",
    ],
  },
  "SigSphere::getRCS -- pi*r^2, calculado uma vez em setRadius()": {
    file: "contexts/src/mixr/src/models/Signatures.cpp",
    line: 137,
    trunc: false,
    lines: [
      "double SigSphere::getRCS(const Emission* const)",
      "{",
      "    return rcs;",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// setRadiusFromSlot() -- Set the radius from Slot table",
      "//------------------------------------------------------------------------------",
      "bool SigSphere::setSlotRadius(base::Number* const num)",
      "{",
      "    bool ok{};",
      "    double r{-1.0};",
      "",
      "    const auto d = dynamic_cast<base::Distance*>(num);",
      "    if (d != nullptr) {",
      "        // Has distance units and we need meters",
      "        base::Meters meters;",
      "        r = meters.convert(*d);",
      "    } else if (num != nullptr) {",
      "        // Just a Number",
      "        r = num->getReal();",
      "    }",
      "",
      "    if (r >= 0.0) { setRadius(r); ok = true; }",
      "    else { std::cerr << \"SigSphere::setRadius: invalid radius; must be greater than or equal to zero!\" << std::endl; }",
      "    return ok;",
      "}",
    ],
  },
  "SigPlate / SigDihedralCR / SigTrihedralCR -- tres formulas lado a lado": {
    file: "contexts/src/mixr/src/models/Signatures.cpp",
    line: 206,
    trunc: true,
    lines: [
      "double SigPlate::getRCS(const Emission* const em)",
      "{",
      "    double rcs{};",
      "    if (em != nullptr) {",
      "        double lambda{em->getWavelength()};",
      "        double area{a * b};",
      "        if (lambda > 0.0 && area > 0.0) {",
      "            // If we have lambda and the area of the plate, compute the RCS",
      "            rcs = (4.0 * base::PI * area * area) / (lambda * lambda);",
      "        }",
      "    }",
      "    return static_cast<double>(rcs);",
      "}",
      "",
      "// ... setA()/setB() omitidos ...",
      "",
      "//==============================================================================",
      "// Class: SigDihedralCR",
      "//==============================================================================",
      "IMPLEMENT_SUBCLASS(SigDihedralCR, \"SigDihedralCR\")",
      "EMPTY_SLOTTABLE(SigDihedralCR)",
      "",
      "SigDihedralCR::SigDihedralCR()",
      "{",
      "    STANDARD_CONSTRUCTOR()",
      "    length = 0.0;   // <- nunca mais lido em lugar nenhum do arquivo (campo morto)",
      "}",
      "",
      "double SigDihedralCR::getRCS(const Emission* const em)",
      "{",
      "    double rcs{};",
      "    if (em != nullptr) {",
      "        const double lambda{em->getWavelength()};",
      "        if (lambda > 0.0) {",
      "            const double a{getA()};   // <- herdado de SigPlate; 'length' NAO e usado aqui",
      "            rcs = (8.0 * base::PI * a*a*a*a) / (lambda*lambda);",
      "        }",
      "    }",
      "    return static_cast<double>(rcs);",
      "}",
      "",
      "//==============================================================================",
      "// Class: SigTrihedralCR",
      "//==============================================================================",
      "double SigTrihedralCR::getRCS(const Emission* const em)",
      "{",
      "    double rcs{};",
      "    if (em != nullptr) {",
      "        const double lambda{em->getWavelength()};",
      "        if (lambda > 0.0) {",
      "            const double a{getA()};",
      "            rcs = (12.0 * base::PI * a*a*a*a) / (lambda*lambda);   // <- NAO delega a SigDihedralCR",
      "        }",
      "    }",
      "    return static_cast<double>(rcs);",
      "}",
    ],
  },
  "SigSwitch::getRCS -- camouflageType+1 como indice 1-based em components:": {
    file: "contexts/src/mixr/src/models/Signatures.cpp",
    line: 362,
    trunc: false,
    lines: [
      "double SigSwitch::getRCS(const Emission* const em)",
      "{",
      "   double rcs{};",
      "",
      "   // Find our ownship player ...",
      "   const Player* ownship{static_cast<const Player*>(findContainerByType(typeid(Player)))};",
      "   if (ownship != nullptr) {",
      "",
      "      // get our ownship's camouflage type",
      "      unsigned int camouflage{ownship->getCamouflageType()};",
      "      camouflage++; // our components are one based",
      "",
      "      // find a RfSignature with this index",
      "      base::Pair* pair{findByIndex(camouflage)};",
      "      if (pair != nullptr) {",
      "         const auto sig = dynamic_cast<RfSignature*>( pair->object() );",
      "         if (sig != nullptr) {",
      "",
      "            // OK -- we've found the correct RfSignature subcomponent",
      "            // now let it do all of the work",
      "            rcs = sig->getRCS(em);",
      "",
      "         }",
      "      }",
      "   }",
      "   return rcs;",
      "}",
    ],
  },
  "SigAzEl::getRCS -- interpolacao bilinear + conversao dB->linear explicita": {
    file: "contexts/src/mixr/src/models/Signatures.cpp",
    line: 449,
    trunc: false,
    lines: [
      "double SigAzEl::getRCS(const Emission* const em)",
      "{",
      "   double rcs{};",
      "   if (em != nullptr && tbl != nullptr) {",
      "",
      "      // angle of arrival (radians)",
      "      double iv1{em->getAzimuthAoi()};",
      "      double iv2{em->getElevationAoi()};",
      "",
      "      // If the table's independent variable's order is swapped: (El, Az)",
      "      if (isOrderSwapped()) {",
      "         iv1 = em->getElevationAoi();",
      "         iv2 = em->getAzimuthAoi();",
      "      }",
      "",
      "      // If the table's independent variables are in degrees ..",
      "      if (isInDegrees()) {",
      "         iv1 *= static_cast<double>(base::angle::R2DCC);",
      "         iv2 *= static_cast<double>(base::angle::R2DCC);",
      "      }",
      "",
      "      rcs = tbl->lfi(iv1,iv2);",
      "",
      "      // If the dependent data is in decibels ...",
      "      if (isDecibel()) {",
      "         rcs = std::pow(static_cast<double>(10.0), static_cast<double>(rcs / 10.0));",
      "      }",
      "   }",
      "   return rcs;",
      "}",
    ],
  },
  "Emission.cpp -- o arquivo inteiro, com o termo 1/(4*pi*r^2)": {
    file: "contexts/src/mixr/src/models/Emission.cpp",
    line: 1,
    trunc: false,
    lines: [
      "",
      "#include \"mixr/models/Emission.hpp\"",
      "",
      "#include \"mixr/models/system/RfSystem.hpp\"",
      "",
      "namespace mixr {",
      "namespace models {",
      "",
      "IMPLEMENT_SUBCLASS(Emission, \"Emission\")",
      "EMPTY_SLOTTABLE(Emission)",
      "",
      "Emission::Emission()",
      "{",
      "    STANDARD_CONSTRUCTOR()",
      "}",
      "",
      "void Emission::copyData(const Emission& org, const bool)",
      "{",
      "    BaseClass::copyData(org);",
      "",
      "    // Copy the data",
      "    freq = org.freq;",
      "    lambda = org.lambda;",
      "    pw = org.pw;",
      "    power = org.power;",
      "    polar = org.polar;",
      "    bw = org.bw;",
      "    gain = org.gain;",
      "    prf = org.prf;",
      "    pulses = org.pulses;",
      "    lossRng = org.lossRng;",
      "    lossAtmos = org.lossAtmos;",
      "    lossXmit = org.lossXmit;",
      "    rcs = org.rcs;",
      "",
      "    const RfSystem* mm = org.transmitter;",
      "    setTransmitter( const_cast<RfSystem*>(static_cast<const RfSystem*>(mm)) );",
      "",
      "    ecmFlag = org.ecmFlag;",
      "}",
      "",
      "void Emission::deleteData()",
      "{",
      "   clear();",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// clear() -- clears out the emissions",
      "//------------------------------------------------------------------------------",
      "void Emission::clear()",
      "{",
      "   BaseClass::clear();",
      "   setTransmitter(nullptr);",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// Sets the range to the target",
      "//------------------------------------------------------------------------------",
      "void Emission::setRange(const double r)",
      "{",
      "   BaseClass::setRange(r);",
      "",
      "   if (r > 1.0) lossRng = static_cast<double>(1.0/(4.0 * base::PI * r * r));",
      "   else lossRng = 1.0;",
      "}",
      "",
      "//------------------------------------------------------------------------------",
      "// setTransmitter() -- Sets the pointer to the source",
      "//------------------------------------------------------------------------------",
      "void Emission::setTransmitter(RfSystem* const t)",
      "{",
      "   //if (transmitter != nullptr) {",
      "   //   transmitter->unref();",
      "   //}",
      "   transmitter = t;",
      "   //if (transmitter != nullptr) {",
      "   //   transmitter->ref();",
      "   //}",
      "}",
      "",
      "}",
      "}",
    ],
  },
  "Player::onRfEmissionEventPlayer -- o ciclo completo em uma funcao so": {
    file: "contexts/src/mixr/src/models/player/Player.cpp",
    line: 2556,
    trunc: false,
    lines: [
      "bool Player::onRfEmissionEventPlayer(Emission* const em)",
      "{",
      "   // Player must be active ...",
      "   if (isNotMode(ACTIVE)) return false;",
      "",
      "   // ---",
      "   //  1) Compute the Line-Of-Sight vectors back to the transmitter (los0)",
      "   // ---",
      "   base::Vec3d xlos{em->getTgtLosVec()};",
      "   base::Vec4d los0( xlos.x(), xlos.y(), xlos.z(), 0.0 );",
      "",
      "   // 2) Transform the LOS vector to our player coordinates to get",
      "   // the Angle Of Incidence (AOI) vector",
      "   base::Vec4d aoi{rm * los0};",
      "   em->setAoiVector(aoi);",
      "",
      "   // 3) Compute the azimuth and elevation angles of incidence (AOI)",
      "   {",
      "      // 3-a) Get the aoi vector values & compute range squared",
      "      const double xa{aoi.x()};",
      "      const double ya{aoi.y()};",
      "      const double za{-aoi.z()};",
      "",
      "      // 3-b) Compute azimuth: az = atan2(ya, xa)",
      "      double aazr{std::atan2(ya, xa)};",
      "      em->setAzimuthAoi(aazr);",
      "",
      "      // 3-c) Compute elevation: el = atan2(za, ra), where 'ra' is sqrt of xa*xa & ya*ya",
      "      double ra{std::sqrt(xa*xa + ya*ya)};",
      "      double aelr{std::atan2(za,ra)};",
      "      em->setElevationAoi(aelr);",
      "   }",
      "",
      "   // 4) Compute and return the RCS",
      "   if (em->isReturnRequested()) {",
      "",
      "      if (signature != nullptr) {",
      "         double rcs{signature->getRCS(em)};",
      "         em->setRCS(rcs);",
      "      } else {",
      "         em->setRCS(0);",
      "      }",
      "",
      "      // Send reflected emissions back to the transmitter",
      "      em->getGimbal()->event(RF_EMISSION_RETURN,em);",
      "   }",
      "",
      "   // 6) Pass the emission to our antennas",
      "   {",
      "      Gimbal* g{getGimbal()};",
      "      if (g != nullptr && g->getPowerSwitch() != System::PWR_OFF) {",
      "         g->event(RF_EMISSION,em);",
      "      }",
      "   }",
      "",
      "   // 7) Pass the emission to anyone requesting reflected emissions",
      "   //    (we're doing do calculations here, this is only meaningful to",
      "   //     the receiving player)",
      "   for (unsigned int i = 0; i < MAX_RF_REFLECTIONS; i++) {",
      "      if (rfReflect[i] != nullptr) rfReflect[i]->event(RF_REFLECTED_EMISSION,em);",
      "   }",
      "",
      "   return true;",
      "}",
    ],
  },
};

const rfSignatureSnip = (key) => (key ? RFSIGNATURE_SNIPPETS[key] || null : null);

function renderRfSignatureSnippet(key) {
  const snip = rfSignatureSnip(key);
  if (!snip) return null;
  const toks = cppTokenizeLines(snip.lines);
  return (
    <div className="mx-code">
      {snip.lines.map((ln, k) => (
        <div key={k} className="mx-cl"><span className="mx-num">{snip.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
      ))}
    </div>
  );
}

const REF_SIGCONSTANT_SLOT_DOCS = { rcs: ["Number|Area", "m² (ou Decibel) -- default 0.0"] };
const REF_SIGSPHERE_SLOT_DOCS = { radius: ["Number|Distance", "metros -- default 0"] };
const REF_SIGPLATE_SLOT_DOCS = {
  a: ["Number|Distance", "comprimento, metros -- default 0"],
  b: ["Number|Distance", "largura, metros -- default 0"],
};
const REF_SIGAZEL_SLOT_DOCS = {
  table: ["Table2", "RCS(az,el) 2D -- clamp nas bordas, salvo `extrapolate` ligado na própria Table2"],
  swapOrder: ["Number(bool)", "se true, a 1ª variável independente é elevação, não azimute -- default false"],
  inDegrees: ["Number(bool)", "variáveis independentes em graus, não radianos -- default false"],
  inDecibel: ["Number(bool)", "dado dependente em dBsm -- default false"],
};

function RfSignatureReferencePage({ onOpenCatalog }) {
  const swEntry = MODEL["SigSwitch"];
  const azEntry = MODEL["SigAzEl"];
  const plateEntry = MODEL["SigPlate"];
  const sphereEntry = MODEL["SigSphere"];
  const constEntry = MODEL["SigConstant"];
  const [tab, setTab] = useState("overview");

  return (
    <div className="mx-body" style={{ paddingBottom: 40 }}>
      <div className="mx-refhero">
        <div style={{ display: "flex", alignItems: "baseline", gap: 10, flexWrap: "wrap" }}>
          <span className="mx-mono" style={{ fontSize: 18, fontWeight: 700 }}>RfSignature</span>
          <span style={{ fontSize: 12, color: "var(--muted)" }}>mixr::models</span>
          <span className="mx-chip">factory: "Signature" (a base) → 7 formas</span>
          <span className="mx-chip">+ Emission (o objeto que atravessa o ciclo)</span>
          {constEntry && onOpenCatalog && (
            <button className="mx-btn" style={{ fontSize: 11, marginLeft: "auto" }} onClick={() => onOpenCatalog("SigConstant")}>Ver no Catálogo →</button>
          )}
        </div>
        <p style={{ fontSize: 12.5, lineHeight: 1.55, maxWidth: 880, margin: "8px 0 0" }}>
          A interface mais enxuta desta Referência inteira: <code className="mx-mono">RfSignature</code> tem
          UM método puro virtual, <code className="mx-mono">getRCS(Emission*)</code>. As sete formas concretas
          vão de "constante" (<code className="mx-mono">SigConstant</code>) a "tabela 2D interpolada"
          (<code className="mx-mono">SigAzEl</code>), passando por geometrias ópticas clássicas de handbook
          (esfera, placa, diedro, triedro). <code className="mx-mono">Emission</code> é o objeto que atravessa
          o ciclo INTEIRO -- não é clonado no retorno, é o MESMO ponteiro, e o termo{" "}
          <code className="mx-mono">1/R⁴</code> da equação de radar emerge do PRODUTO de duas contas
          independentes de <code className="mx-mono">1/(4πR²)</code>, nunca escrito como{" "}
          <code className="mx-mono">R⁴</code> em lugar nenhum do fonte.
        </p>
        <div className="mx-mono" style={{ fontSize: 10.5, color: "var(--muted)", marginTop: 10 }}>
          {constEntry ? constEntry.ch.join(" → ") : "SigConstant → RfSignature → Component → Object"}
        </div>
      </div>

      <div className="mx-dtabs" role="tablist" aria-label="Seções de RfSignature" style={{ marginTop: 14 }}>
        <button className="mx-dtab" role="tab" aria-selected={tab === "overview"} data-on={tab === "overview" ? 1 : 0} onClick={() => setTab("overview")}>Visão geral</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "slots"} data-on={tab === "slots" ? 1 : 0} onClick={() => setTab("slots")}>Slots</button>
        <button className="mx-dtab" role="tab" aria-selected={tab === "code"} data-on={tab === "code" ? 1 : 0} onClick={() => setTab("code")}>Código-fonte</button>
      </div>

      <div className="mx-detailbody" key={tab}>
        {tab === "overview" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>SigConstant / SigSphere -- ignoram o Emission por completo</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">SigConstant::getRCS()</code> devolve um número fixo, aceitando o slot{" "}
                <code className="mx-mono">rcs</code> como <code className="mx-mono">Number</code> OU{" "}
                <code className="mx-mono">Area</code> -- a conversão dB→linear acontece IMPLICITAMENTE via{" "}
                <code className="mx-mono">Number::getReal()</code> polimórfico (um <code className="mx-mono">
                Decibel</code> já devolve o valor linear). <code className="mx-mono">SigSphere</code> pré-computa{" "}
                σ = πr² (óptica geométrica, r≫λ) uma vez em <code className="mx-mono">setRadius()</code> -- a
                única classe cujo <code className="mx-mono">getRCS()</code> nem olha o parâmetro, correto por
                definição (esfera não tem aspecto), mas sem checagem nenhuma da hipótese r≫λ.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p className="mx-warn" style={{ margin: 0 }}>
                <b>Achado -- confirma literalmente a simplificação já registrada em `models/BUILT-IN.md`:</b>{" "}
                <code className="mx-mono">SigPlate::getRCS()</code> usa σ = 4πA²/λ² (óptica física, incidência
                NORMAL) e NUNCA chama <code className="mx-mono">em-&gt;getAzimuthAoi()</code>/{" "}
                <code className="mx-mono">getElevationAoi()</code> -- o `Emission` só é lido para{" "}
                <code className="mx-mono">getWavelength()</code>. "Sempre normal ao transmissor" não é
                aproximação de projeto: é a ÚNICA geometria que o código sabe calcular. Qualquer aspecto fora do
                broadside continua produzindo o RCS de incidência normal, o máximo possível.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>SigDihedralCR / SigTrihedralCR -- duas fórmulas de verdade, um campo morto</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                σ = 8πa⁴/λ² (diedro) e σ = 12πa⁴/λ² (triedro) -- constantes DIFERENTES, nenhuma delega para a
                outra (cada <code className="mx-mono">getRCS()</code> reescreve o mesmo esqueleto do zero). As
                duas usam <code className="mx-mono">getA()</code> HERDADO de <code className="mx-mono">SigPlate</code>{" "}
                -- o membro próprio <code className="mx-mono">double length{"{}"}</code> é inicializado em ambos
                os construtores de <code className="mx-mono">SigDihedralCR</code> mas NUNCA lido em lugar nenhum
                do arquivo -- vestígio morto. O slot <code className="mx-mono">b</code> (herdado de{" "}
                <code className="mx-mono">SigPlate</code>, aceito silenciosamente no `.edl`) também é ignorado
                pela fórmula, que só usa <code className="mx-mono">a</code>.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>SigSwitch -- camouflageType como índice 1-based em components:</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                <code className="mx-mono">findContainerByType(typeid(Player))</code> -- o MESMO mecanismo já
                documentado como frágil em outras partes deste projeto (AlertDatalink, o monitor do Groot) --
                acha o ownship, lê <code className="mx-mono">getCamouflageType()</code>, soma 1 (componentes de
                EDL são 1-based) e busca por ÍNDICE POSICIONAL em <code className="mx-mono">components:</code>{" "}
                (<code className="mx-mono">findByIndex()</code>, não por chave nomeada). Sem `Player` ancestral,
                sem item naquele índice, ou item que não é `RfSignature`: degrada para{" "}
                <code className="mx-mono">rcs=0.0</code> em silêncio, sem log nenhum.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 420px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>SigAzEl -- a única que lê o AOI de verdade, e converte dB→linear corretamente</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: 0 }}>
                Lê <code className="mx-mono">getAzimuthAoi()</code>/<code className="mx-mono">getElevationAoi()</code>{" "}
                (já pré-calculados por <code className="mx-mono">Player</code>, ver o ciclo completo abaixo),
                interpola BILINEAR na <code className="mx-mono">Table2</code> (<code className="mx-mono">lfi()</code>{" "}
                -- clamp nas bordas por padrão, extrapolação só se a própria tabela ligar isso). Ao contrário de{" "}
                <code className="mx-mono">SigConstant</code>, o dado bruto da tabela é <code className="mx-mono">
                double</code> cru -- a conversão <code className="mx-mono">pow(10, rcs/10)</code> quando{" "}
                <code className="mx-mono">inDecibel</code> está ligado é EXPLÍCITA e está presente, evitando o
                erro clássico de misturar dBsm com m² linear.
              </p>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 8 }}>O ciclo completo -- um Emission só, ida e volta, nunca clonado</div>
              <p style={{ fontSize: 12, lineHeight: 1.6, margin: "0 0 8px" }}>
                (1) <code className="mx-mono">Antenna::rfTransmit()</code> clona um <code className="mx-mono">
                Emission</code> por alvo e despacha <code className="mx-mono">target-&gt;event(RF_EMISSION, em)</code>.
                (2) <code className="mx-mono">Player::onRfEmissionEventPlayer()</code> (ver Código-fonte) calcula
                o vetor de incidência no referencial do ALVO, grava AOI no PRÓPRIO <code className="mx-mono">em</code>,
                consulta <code className="mx-mono">signature-&gt;getRCS(em)</code> (ou grava{" "}
                <code className="mx-mono">0</code> sem assinatura) e devolve o MESMO objeto via{" "}
                <code className="mx-mono">em-&gt;getGimbal()-&gt;event(RF_EMISSION_RETURN, em)</code>.
                (3) <code className="mx-mono">Antenna::onRfEmissionReturnEventAntenna()</code>, do lado do
                transmissor original, calcula a área efetiva e chama{" "}
                <code className="mx-mono">sys-&gt;rfReceivedEmission(em, this, gain)</code> -- a MESMA rota que
                uma emissão de OUTRO player usaria. (4) <code className="mx-mono">RfSystem::rfReceivedEmission()</code>{" "}
                aplica a PRIMEIRA perna de <code className="mx-mono">1/(4πR²)</code> (via{" "}
                <code className="mx-mono">Emission::setRange()</code>). (5) <code className="mx-mono">Radar::receive()</code>{" "}
                lê <code className="mx-mono">em-&gt;getRCS()</code> e aplica a SEGUNDA perna do MESMO{" "}
                <code className="mx-mono">rangeLoss</code> -- como é a mesma distância nas duas contas, o produto
                reproduz <code className="mx-mono">1/(16π²R⁴)</code>, sem nenhum código explícito de "elevar R⁴".
              </p>
            </div>
          </div>
        )}

        {tab === "slots" && (
          <div style={{ display: "flex", gap: 16, flexWrap: "wrap", alignItems: "flex-start" }}>
            <div className="mx-card" style={{ flex: "1 1 220px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>SigConstant</div>
              <div className="mx-slotgrid">
                {(constEntry ? constEntry.sl : Object.keys(REF_SIGCONSTANT_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SIGCONSTANT_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SIGCONSTANT_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 260 }}>{REF_SIGCONSTANT_SLOT_DOCS[s] ? REF_SIGCONSTANT_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 220px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>SigSphere</div>
              <div className="mx-slotgrid">
                {(sphereEntry ? sphereEntry.sl : Object.keys(REF_SIGSPHERE_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SIGSPHERE_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SIGSPHERE_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 260 }}>{REF_SIGSPHERE_SLOT_DOCS[s] ? REF_SIGSPHERE_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 260px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>SigPlate</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>Herdados por SigDihedralCR/SigTrihedralCR (EMPTY_SLOTTABLE nos dois).</p>
              <div className="mx-slotgrid">
                {(plateEntry ? plateEntry.sl : Object.keys(REF_SIGPLATE_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SIGPLATE_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SIGPLATE_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 260 }}>{REF_SIGPLATE_SLOT_DOCS[s] ? REF_SIGPLATE_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 340px" }}>
              <div style={{ fontSize: 12.5, fontWeight: 700, marginBottom: 4 }}>SigAzEl</div>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: "0 0 10px" }}>{azEntry ? azEntry.own : 4} slots próprios.</p>
              <div className="mx-slotgrid">
                {(azEntry ? azEntry.sl : Object.keys(REF_SIGAZEL_SLOT_DOCS)).map((s) => (
                  <div className="mx-slot" key={s}>
                    <span>{s}{REF_SIGAZEL_SLOT_DOCS[s] ? <span style={{ color: "var(--muted)" }}> {"<"}{REF_SIGAZEL_SLOT_DOCS[s][0]}{">"}</span> : ""}</span>
                    <span style={{ maxWidth: 300 }}>{REF_SIGAZEL_SLOT_DOCS[s] ? REF_SIGAZEL_SLOT_DOCS[s][1] : ""}</span>
                  </div>
                ))}
              </div>
            </div>
            <div className="mx-card" style={{ flex: "1 1 100%" }}>
              <p style={{ fontSize: 11.5, color: "var(--muted)", margin: 0 }}>
                <code className="mx-mono">SigDihedralCR</code>, <code className="mx-mono">SigTrihedralCR</code> e{" "}
                <code className="mx-mono">SigSwitch</code> declaram <code className="mx-mono">EMPTY_SLOTTABLE</code>{" "}
                -- zero slots próprios (os dois primeiros herdam <code className="mx-mono">a</code>/<code className="mx-mono">b</code>{" "}
                de <code className="mx-mono">SigPlate</code>; <code className="mx-mono">SigSwitch</code> se
                configura inteiramente por <code className="mx-mono">components:</code>). <code className="mx-mono">
                Emission</code> também é <code className="mx-mono">EMPTY_SLOTTABLE</code> -- nunca é declarada
                num `.edl`, só criada em C++ por `Antenna`/`RfSystem`.
              </p>
            </div>
          </div>
        )}

        {tab === "code" && (
          <>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">RfSignature -- a interface inteira, e o nome de fábrica real</span><span>C++</span></div>
              {renderRfSignatureSnippet("RfSignature -- interface + o nome de fabrica real e 'Signature'")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">SigConstant::getRCS()/setRCS()</span><span>C++</span></div>
              {renderRfSignatureSnippet("SigConstant::getRCS/setRCS -- ignora o Emission, aceita Number OU Decibel")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">SigSphere::getRCS()/setSlotRadius()</span><span>C++</span></div>
              {renderRfSignatureSnippet("SigSphere::getRCS -- pi*r^2, calculado uma vez em setRadius()")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">SigPlate / SigDihedralCR / SigTrihedralCR -- as três fórmulas lado a lado</span><span>C++</span></div>
              {renderRfSignatureSnippet("SigPlate / SigDihedralCR / SigTrihedralCR -- tres formulas lado a lado")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">SigSwitch::getRCS()</span><span>C++</span></div>
              {renderRfSignatureSnippet("SigSwitch::getRCS -- camouflageType+1 como indice 1-based em components:")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">SigAzEl::getRCS()</span><span>C++</span></div>
              {renderRfSignatureSnippet("SigAzEl::getRCS -- interpolacao bilinear + conversao dB->linear explicita")}
            </div>
            <div className="mx-card" style={{ marginBottom: 12 }}>
              <div className="mx-lbl"><span className="mx-mono">Emission.cpp -- o arquivo inteiro</span><span>C++</span></div>
              {renderRfSignatureSnippet("Emission.cpp -- o arquivo inteiro, com o termo 1/(4*pi*r^2)")}
            </div>
            <div className="mx-card">
              <div className="mx-lbl"><span className="mx-mono">Player::onRfEmissionEventPlayer() -- o ciclo completo, na íntegra</span><span>C++</span></div>
              {renderRfSignatureSnippet("Player::onRfEmissionEventPlayer -- o ciclo completo em uma funcao so")}
            </div>
          </>
        )}
      </div>
    </div>
  );
}

function Reference({ onOpenCatalog }) {
  const [selected, setSelected] = useState("Missile");
  return (
    <div className="mx-body" style={{ display: "flex", gap: 0, paddingBottom: 40 }}>
      <div style={{ width: 208, flexShrink: 0, borderRight: "1px solid var(--rule)", paddingRight: 14, marginRight: 14 }}>
        <div className="mx-lbl"><span>classes documentadas</span></div>
        {REF_CLASSES.map((c) => (
          <div key={c.key} onClick={() => setSelected(c.key)}
               style={{ padding: "9px 10px", cursor: "pointer", borderRadius: 3, marginBottom: 4,
                        background: selected === c.key ? "var(--active-bg)" : "transparent",
                        borderLeft: `3px solid ${selected === c.key ? "var(--hot)" : "transparent"}` }}>
            <div className="mx-mono" style={{ fontSize: 13, fontWeight: selected === c.key ? 700 : 500 }}>{c.label}</div>
            <div style={{ fontSize: 10.5, color: "var(--muted)", lineHeight: 1.35, marginTop: 2 }}>{c.sub}</div>
          </div>
        ))}
        <div style={{ padding: "9px 10px", borderRadius: 3, border: "1px dashed var(--rule)", marginTop: 6 }}>
          <div className="mx-mono" style={{ fontSize: 12, color: "var(--sub-muted)" }}>+ mais em breve</div>
          <p style={{ fontSize: 10, color: "var(--sub-muted)", margin: "4px 0 0", lineHeight: 1.4 }}>
            Referência é curadoria manual, não extração automática -- cresce uma classe de cada vez.
          </p>
        </div>
      </div>
      <div style={{ flex: 1, minWidth: 0 }}>
        {selected === "Missile" && <MissileReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "Steerpoint" && <SteerpointReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "Navigation" && <NavigationReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "Autopilot" && <AutopilotReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "Player" && <PlayerReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "System" && <SystemReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "Gimbal" && <GimbalReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "RfSensor" && <RfSensorReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "TrackManager" && <TrackManagerReferencePage onOpenCatalog={onOpenCatalog} />}
        {selected === "RfSignature" && <RfSignatureReferencePage onOpenCatalog={onOpenCatalog} />}
      </div>
    </div>
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
        {ownSnippets.length ? ownSnippets.map(([m, s]) => {
          const toks = cppTokenizeLines(s.lines);
          return (
          <div key={m} style={{ marginBottom: 10 }}>
            <div className="mx-mono" style={{ fontSize: 11, color: "var(--muted)", marginBottom: 3 }}>{c}::{m} — {s.file}:{s.line}</div>
            <div className="mx-code">
              {s.lines.map((ln, k) => (
                <div key={k} className="mx-cl"><span className="mx-num">{s.line + k}</span><span className="mx-src">{renderCppSrc(toks[k], ln)}</span></div>
              ))}
              {s.trunc && <div className="mx-codecut">⋯ corpo truncado nesta visualização ⋯</div>}
            </div>
          </div>
          );
        }) : (
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
 * Diferente de layout()/flightLayout() (nó de tamanho FIXO): aqui altura E
 * largura variam por nó -- um stub Tier 2 tem ~1 linha e nome curto;
 * Player, expandida, tem 400+ linhas e um método herdado de 169
 * caracteres (Component::processComponents() por inteiro). Largura fixa
 * cortaria esse texto -- pedido explícito é "sem abreviação nenhuma".
 *
 * Duas passadas: pós-ordem MEDINDO a extensão vertical real de cada
 * subárvore (_subH = max(altura própria, soma das alturas dos filhos +
 * espaçamento)); pré-ordem POSICIONANDO (x = início da COLUNA da
 * profundidade, ver abaixo; y empilha os filhos pela extensão real,
 * centralizados no espaço que a própria subárvore ocupa -- pai centrado
 * entre o PRIMEIRO e o ÚLTIMO filho, mesma regra de layout(), só
 * alimentada com extensões reais em vez de um contador de folha uniforme). */
const UML_MIN_W = 170, UML_GAP_X = 70, UML_GAP = 20;
const UML_HEAD_BLOCK = 35, UML_HEADER_H = 22, UML_COMPT_PAD = 3, UML_ROW_H = 14, UML_DIVIDER_H = 1;

// Medição REAL de largura de texto via <canvas>, não uma razão
// caractere/pixel chutada -- só assim dá pra garantir "cabe sem
// abreviação" com a MESMA fonte que o navegador de fato resolve pra
// var(--mono) (ui-monospace/JetBrains Mono/SF Mono/Menlo, que não têm
// exatamente a mesma largura de glifo entre si). Sem <canvas> disponível
// (não deveria faltar num navegador real -- só previsto pra não quebrar
// num ambiente de teste sem DOM completo) cai num fallback por contagem de
// caractere, generoso de propósito: melhor caixa larga demais que texto
// cortado.
const UML_MONO = "ui-monospace,'JetBrains Mono','SF Mono',Menlo,monospace";
function makeTextMeasurer() {
  let ctx = null;
  try { ctx = document.createElement("canvas").getContext("2d"); } catch { ctx = null; }
  return (text, font, fallbackPxPerChar) => {
    if (ctx) { ctx.font = font; return ctx.measureText(text).width; }
    return text.length * fallbackPxPerChar;
  };
}
const measureUmlText = makeTextMeasurer();

// Larguras de padding/prefixo iguais às usadas no render da caixa (ver o
// <foreignObject> de cada compartimento, mais abaixo) -- se um dia
// divergirem, o texto volta a arriscar cortar.
function umlNodeWidth(node, collapsedSet) {
  const rowW = (text) => measureUmlText(text, `9.5px ${UML_MONO}`, 6.2) + 16 /* padding 8+8 */ + 10 /* prefixo -/#/+ */ + 6 /* folga */;
  const headerW = (text) => measureUmlText(text, `600 11px ${UML_MONO}`, 7.2) + 16 + 13 /* caret+gap */ + 6;
  const baseW = (text) => measureUmlText(text, `italic 9px ${UML_MONO}`, 5.9) + 16 + 6;

  const e = CLASS_DIAGRAM.classes[node.cls];
  if (!e) {
    const t2 = CLASS_DIAGRAM.tier2[node.cls] || {};
    const line = t2.base ? `«tier 2» : ${t2.base}` : "«tier 2»";
    return Math.max(UML_MIN_W, Math.ceil(headerW(node.cls)), Math.ceil(baseW(line)));
  }
  let w = Math.max(UML_MIN_W, headerW(node.cls));
  if (e.base) w = Math.max(w, baseW(`: ${e.base}`));
  // Recolhida, a caixa só mostra cabeçalho+base -- não precisa da largura
  // do maior atributo/método (que nem está visível), senão toda caixa
  // Tier 1 nasceria enorme mesmo recolhida (o estado padrão desta aba).
  // Reavaliado a cada expandir/recolher, igual já acontece com a altura.
  if (collapsedSet && collapsedSet.has(node.id)) return Math.ceil(w);
  e.attributes.forEach((a) => { w = Math.max(w, rowW(`${a.name} : ${a.type}`)); });
  e.components.forEach((c) => { w = Math.max(w, rowW(`${c.name} : ${c.target}${c.multiplicity === "many" ? "[*]" : ""}`)); });
  e.methods.forEach((m) => {
    let sig = m.signature;
    if (m.pureVirtual) sig = sig.replace(/\s*=\s*0\s*$/, "") + " {abstract}";
    w = Math.max(w, rowW(sig));
  });
  return Math.ceil(w);
}

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

function umlLayout(root, heightOf, widthOf) {
  // Largura por COLUNA (profundidade), não por nó individual -- todo nó de
  // uma mesma coluna começa no mesmo X, largo o bastante pro MAIOR nome/
  // atributo/método daquela coluna específica, nunca do grafo inteiro (uma
  // caixa Tier 2 minúscula não fica gigante só porque Player, em outra
  // coluna, tem um método de 169 caracteres).
  const colWidths = [];
  (function scanWidths(n, depth) {
    n._w = widthOf(n);
    colWidths[depth] = Math.max(colWidths[depth] || 0, n._w);
    (n.children || []).forEach((c) => scanWidths(c, depth + 1));
  })(root, 0);
  const colX = [0];
  for (let d = 1; d < colWidths.length; d++) colX[d] = colX[d - 1] + colWidths[d - 1] + UML_GAP_X;

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
    const x = colX[depth];
    if (!kids.length) {
      nodes.push({ ...n, x, y: top + n._subH / 2, depth, h: n._h, w: n._w });
      return;
    }
    const kidsTotal = kids.reduce((a, k) => a + k._subH, 0) + UML_GAP * (kids.length - 1);
    let cursor = top + (n._subH - kidsTotal) / 2;
    kids.forEach((k) => { place(k, depth + 1, cursor); cursor += k._subH + UML_GAP; });
    const f = nodes.find((m) => m.id === kids[0].id);
    const l = nodes.find((m) => m.id === kids[kids.length - 1].id);
    nodes.push({ ...n, x, y: (f.y + l.y) / 2, depth, h: n._h, w: n._w });
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
  // Teto de zoom mais alto que o das outras abas (20x, não 10x) -- caixas
  // aqui podem chegar a mais de 1000px de largura (método herdado de 169
  // caracteres exibido sem abreviação), então 10x não bastava pra ler
  // texto de perto numa árvore desse tamanho.
  const { view, setView, svgRef, onDown, onMove, onUp, maxZoom } = usePanZoom({ k: 1, x: 0, y: 0 }, 20);

  // Chegada vinda de outra aba (deep-link futuro) -- mesmo padrão do
  // useEffect de `focus` em Exec/Catalog.
  useEffect(() => {
    if (!focus) return;
    const n = STRUCT_ALL.find((x) => x.cls === focus);
    if (n) setPinned(n.id);
    setFocus(null);
  }, [focus, setFocus]);

  const heightOf = useCallback((n) => umlNodeHeight(n, collapsed), [collapsed]);
  const widthOf = useCallback((n) => umlNodeWidth(n, collapsed), [collapsed]);
  // Colapsar um nó muda seu {h} (e agora também {w}) -- força o recálculo do layout INTEIRO
  // (nunca scroll interno num foreignObject: sem precedente no arquivo, e
  // relayoutar ~44 nós é desprezível -- o Catálogo já refiltra 342 a cada
  // tecla).
  const nodes = useMemo(() => umlLayout(STRUCT_TOPOLOGY, heightOf, widthOf), [heightOf, widthOf]);
  const pos = useMemo(() => Object.fromEntries(nodes.map((n) => [n.id, n])), [nodes]);

  const W = Math.max(...nodes.map((n) => n.x + n.w)) + 40;
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
      <h2 style={{ fontSize: 15, margin: "0 0 6px", fontWeight: 600 }}>Diagrama de Classes</h2>
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
            <input type="range" min={ZOOM_MIN} max={maxZoom} step={0.01} value={view.k} aria-label="Zoom"
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
                // Meio do vão REAL entre o lado direito de p e o lado
                // esquerdo de q -- não mais um deslocamento fixo, porque a
                // largura de p e a coluna de q agora variam por conteúdo.
                const mid = (p.x + p.w + q.x) / 2;
                const d = `M ${p.x + p.w} ${p.y} H ${mid} V ${q.y} H ${q.x}`;
                return (
                  <g key={a + ">" + b}>
                    <path d={d} fill="none" stroke={compose ? "var(--ink)" : "var(--rule)"} strokeWidth={compose ? 1.5 : 1.2}
                      markerStart={compose ? "url(#uml-diamond)" : undefined} />
                    {compose && child.label && (
                      <foreignObject x={mid - 120} y={q.y - 15} width="240" height="12" style={{ pointerEvents: "none" }}>
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
                const pRight = p.x + p.w, qRight = q.x + q.w;
                const d = p.x <= q.x
                  ? `M ${pRight} ${p.y} C ${pRight + 70} ${p.y}, ${q.x - 70} ${q.y}, ${q.x} ${q.y}`
                  : `M ${p.x} ${p.y} C ${p.x - 70} ${p.y}, ${qRight + 70} ${q.y}, ${qRight} ${q.y}`;
                const lx = (p.x + p.w / 2 + q.x + q.w / 2) / 2, ly = (p.y + q.y) / 2;
                return (
                  <g key={bref.from + "~" + bref.to} opacity="0.8">
                    <path d={d} fill="none" stroke="var(--ok)" strokeWidth="1.3" strokeDasharray="2 3" />
                    <foreignObject x={lx - 120} y={ly - 6} width="240" height="12" style={{ pointerEvents: "none" }}>
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
                    <rect x="0" y="0" width={n.w} height={n.h} rx="2"
                      fill={tier2 ? "var(--panel)" : (pinned === n.id ? "var(--active-bg)" : "var(--paper)")}
                      stroke={pinned === n.id ? "var(--ink)" : "var(--rule)"}
                      strokeWidth={pinned === n.id ? 1.6 : 1}
                      strokeDasharray={tier2 ? "3 2" : "0"} />
                    <foreignObject x="0" y="0" width={n.w} height={n.h}>
                      <div style={{ width: n.w, height: n.h, overflow: "hidden" }}>
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
