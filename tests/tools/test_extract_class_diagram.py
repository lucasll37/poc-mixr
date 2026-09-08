#!/usr/bin/env python3
"""Regressao do diagrama de classe estrutural (tools/extract_class_diagram.py).

Sem framework nenhum -- mesmo estilo de tests/tools/test_edl_catalog.py: uma
lista de casos, cada um um assert com mensagem, um contador de falhas e um
exit code. Roda em milissegundos, so o fonte de contexts/src/mixr/.

Cada caso aqui e a prova de um fato CONFIRMADO lendo o header real (nao
hipotetico) -- ver os comentarios de tools/extract_class_diagram.py para o
raciocinio completo do algoritmo:

  * `Component.components -> PairStream` (many, membro direto,
    `safe_ptr<PairStream> components;`).
  * `Player` tem exatamente os 10 componentes via `typeid()`
    (mesmo `EXPECTED_ROLES` ja hard-coded em test_edl_catalog.py) mais
    exatamente UM alvo "outro" descoberto (nao antecipado na semente Tier-2):
    `SynchronizedState` (aparece duas vezes -- `syncState1`/`syncState2` --
    ambos por VALOR, `SynchronizedState syncState1;`/`syncState2;`).
  * `NetIO.base == "AbstractNetIO"` -- regressao direta do self-loop
    `NetIO -> NetIO` (a colisao de nome barra entre `interop::common::NetIO`
    e `interop::dis::NetIO`) que ja apareceu no `MODEL` embutido em
    `docs/manual/doc.jsx`; aqui a base e' resolvida POR ARQUIVO
    (`DECLARE_SUBCLASS(NetIO, simulation::AbstractNetIO)`), nunca por um mapa
    global indexado por nome nu.
  * `OutputHandler.queue -> List`, por VALOR (`base::List queue;`, sem `*`) --
    prova de que um atributo por valor tambem conta como composicao, nao so'
    ponteiro/safe_ptr.
  * `PairStream`: sem atributo proprio (herda armazenamento de `List`) -- o
    alvo de composicao e' inferido pelo tipo mais citado entre os PROPRIOS
    metodos (`Pair`, citado 9x em retornos/parametros -- limiar >= 3).
  * `Referenced`: sentinela dura (2 atributos, 7 metodos, 0 componentes,
    base None) -- conferida linha a linha no proprio header, nao repetida de
    lugar nenhum.
  * `Ntm.tPlayer -> Player`, via `base::safe_ptr<const models::Player>` --
    desembrulho de UM nivel de `safe_ptr` com `const` DENTRO do argumento de
    template, nao como prefixo do wrapper.
  * `Simulation.newPlayerQueue -> Pair`, via `base::safe_queue<base::Pair*>`
    -- multiplicidade 'many' (safe_queue e' sempre um container).
  * `AbstractDataRecorder`/`OutputHandler` tem base `AbstractRecorderComponent`
    -- nao curada de antemao, descoberta via a varredura ampla de
    DECLARE_SUBCLASS (exemplo citado no plano original desta feature).
  * Toda entrada de `components` tem exatamente as chaves
    name/target/targetTier/multiplicidade/resolvedVia/visibility; toda
    entrada Tier-1 tem file/base/attributes/components/methods.
  * `Pair` (Tier-1) nunca aparece em `tier2` (regressao: o caso especial de
    PairStream registrava `Pair` como "outro descoberto" mesmo sendo Tier-1).
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import extract_class_diagram as ext  # noqa: E402

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def component(entry, name):
    for c in entry["components"]:
        if c["name"] == name:
            return c
    return None


def method(entry, name):
    return [m for m in entry["methods"] if m["name"] == name]


def main():
    classes, tier2, warnings = ext.build_diagram()

    check(not warnings, f"avisos inesperados na extracao: {warnings}")

    # -- as 18 classes Tier-1 foram todas encontradas, com corpo real -------
    for name in ext.TIER1_HEADERS:
        check(name in classes, f"{name} nao esta no resultado (corpo nao encontrado)")

    for name, data in classes.items():
        check(data["_bodyLength"] > 0, f"{name}: corpo vazio")
        check(len(data["_leftover"].strip()) <= 8,
              f"{name}: sobra de texto nao classificado: {data['_leftover'].strip()[:80]!r}")

    # -- formato: todo item Tier-1 tem as 5 chaves esperadas -----------------
    REQUIRED_TIER1_KEYS = {"file", "namespace", "base", "attributes", "components", "methods"}
    for name, data in classes.items():
        public_keys = {k for k in data if not k.startswith("_")}
        check(REQUIRED_TIER1_KEYS <= public_keys,
              f"{name}: faltam chaves {REQUIRED_TIER1_KEYS - public_keys}")

    REQUIRED_COMPONENT_KEYS = {"name", "target", "targetTier", "multiplicity", "resolvedVia", "visibility"}
    for name, data in classes.items():
        for c in data["components"]:
            check(REQUIRED_COMPONENT_KEYS <= set(c),
                  f"{name}.components[{c.get('name')!r}]: faltam chaves "
                  f"{REQUIRED_COMPONENT_KEYS - set(c)}")
            check(c["targetTier"] in (1, 2),
                  f"{name}.components[{c['name']!r}].targetTier == {c['targetTier']!r}, esperado 1 ou 2")
            check(c["multiplicity"] in ("one", "many"),
                  f"{name}.components[{c['name']!r}].multiplicity == {c['multiplicity']!r}, esperado 'one'/'many'")

    # -- sentinela dura: Referenced, conferida linha a linha no header -------
    ref = classes.get("Referenced")
    check(ref is not None, "Referenced nao esta no resultado")
    if ref:
        check(ref["base"] is None, f"Referenced.base == {ref['base']!r}, esperado None (sem DECLARE_SUBCLASS nem clausula ': Base')")
        check(len(ref["attributes"]) == 2,
              f"Referenced tem {len(ref['attributes'])} atributos, esperado 2 (semaphore, refCount)")
        check(len(ref["methods"]) == 7,
              f"Referenced tem {len(ref['methods'])} metodos, esperado 7 (ctor default, ctor copia "
              f"deletado, operator= deletado, destrutor puro, getRefCount, ref, unref)")
        check(len(ref["components"]) == 0,
              f"Referenced tem {len(ref['components'])} componentes, esperado 0 "
              f"(Exception/ExpInvalidRefCount* sao tipos ANINHADOS, nao atributos)")
        destructors = method(ref, "~Referenced")
        check(len(destructors) == 1 and destructors[0]["pureVirtual"] and destructors[0]["virtual"],
              f"~Referenced deveria ser virtual E puro (=0), veio {destructors}")

    # -- Object: sem DECLARE_SUBCLASS, base cai no fallback 'class X : public Y'
    obj = classes.get("Object")
    check(obj is not None, "Object nao esta no resultado")
    if obj:
        check(obj["base"] == "Referenced",
              f"Object.base == {obj['base']!r}, esperado 'Referenced' (via fallback de clausula "
              f"'class Object : public Referenced', SEM DECLARE_SUBCLASS nesta classe)")

    # -- Component.components -> PairStream (many, membro direto) -----------
    comp = classes.get("Component")
    check(comp is not None, "Component nao esta no resultado")
    if comp:
        check(comp["base"] == "Object", f"Component.base == {comp['base']!r}, esperado 'Object'")
        children = component(comp, "components")
        check(children is not None, "Component.components nao encontrado (safe_ptr<PairStream> components;)")
        if children:
            check(children["target"] == "PairStream",
                  f"Component.components.target == {children['target']!r}, esperado 'PairStream'")
            check(children["targetTier"] == 1,
                  f"Component.components.targetTier == {children['targetTier']!r}, esperado 1 (PairStream e Tier-1)")
            check(children["multiplicity"] == "many",
                  f"Component.components.multiplicity == {children['multiplicity']!r}, esperado 'many'")
        # containerPtr/selected -> Component (auto-referencia, legitima); selection -> Object
        check(component(comp, "containerPtr") is not None and component(comp, "containerPtr")["target"] == "Component",
              "Component.containerPtr deveria compor Component (auto-referencia: 'Component* containerPtr')")
        sel_obj = component(comp, "selection")
        check(sel_obj is not None and sel_obj["target"] == "Object",
              f"Component.selection deveria compor Object, veio {sel_obj}")

    # -- PairStream: sem atributo proprio; composicao inferida = Pair -------
    ps = classes.get("PairStream")
    check(ps is not None, "PairStream nao esta no resultado")
    if ps:
        check(ps["base"] == "List", f"PairStream.base == {ps['base']!r}, esperado 'List'")
        check(len(ps["attributes"]) == 0,
              f"PairStream tem {len(ps['attributes'])} atributos, esperado 0 (herda armazenamento de List)")
        check(len(ps["components"]) == 1,
              f"PairStream tem {len(ps['components'])} componentes, esperado exatamente 1 (inferido)")
        if ps["components"]:
            inferred = ps["components"][0]
            check(inferred["target"] == "Pair",
                  f"PairStream: alvo inferido == {inferred['target']!r}, esperado 'Pair' (citado 9x "
                  f"em retorno/parametro de findByType/findByName/getPosition/get/put/remove)")
            check(inferred["resolvedVia"] == "inferred-from-methods",
                  f"PairStream: resolvedVia == {inferred['resolvedVia']!r}, esperado 'inferred-from-methods'")
            check(inferred["multiplicity"] == "many",
                  f"PairStream: multiplicity == {inferred['multiplicity']!r}, esperado 'many'")

    # -- NetIO.base == AbstractNetIO -- regressao direta do self-loop -------
    netio = classes.get("NetIO")
    check(netio is not None, "NetIO nao esta no resultado")
    if netio:
        check(netio["base"] == "AbstractNetIO",
              f"NetIO.base == {netio['base']!r}, esperado 'AbstractNetIO' -- regressao do self-loop "
              f"NetIO<->NetIO (colisao de nome barra entre interop::common::NetIO e interop::dis::NetIO); "
              f"a base tem que vir da resolucao POR ARQUIVO (DECLARE_SUBCLASS no mesmo header), nunca "
              f"de um mapa de heranca global indexado por nome nu")
        check(netio["base"] != "NetIO", "NetIO.base nao pode ser 'NetIO' (self-loop)")
        for nm, target in (("station", "Station"), ("simulation", "Simulation"),
                           ("inputEntityTypes", "Ntm"), ("outputEntityTypes", "Ntm")):
            c = component(netio, nm)
            check(c is not None and c["target"] == target,
                  f"NetIO.{nm} deveria compor {target!r}, veio {c}")
        c = component(netio, "inputNtmTree")
        check(c is not None and c["target"] == "NtmInputNode",
              f"NetIO.inputNtmTree deveria compor NtmInputNode (classe ANINHADA dentro do proprio "
              f"corpo de NetIO, com seu proprio DECLARE_SUBCLASS -- descoberta via a varredura ampla, "
              f"nao curada de antemao), veio {c}")

    # -- OutputHandler.queue -> List, por VALOR ------------------------------
    oh = classes.get("OutputHandler")
    check(oh is not None, "OutputHandler nao esta no resultado")
    if oh:
        check(oh["base"] == "AbstractRecorderComponent",
              f"OutputHandler.base == {oh['base']!r}, esperado 'AbstractRecorderComponent' -- "
              f"nao curada de antemao (nao esta em TIER1_HEADERS nem TIER2_SEED_NAMES), descoberta "
              f"via o fechamento do grafo por base")
        q = component(oh, "queue")
        check(q is not None, "OutputHandler.queue nao encontrado (base::List queue; -- por VALOR, sem '*')")
        if q:
            check(q["target"] == "List", f"OutputHandler.queue.target == {q['target']!r}, esperado 'List'")
            check(q["targetTier"] == 2, f"OutputHandler.queue.targetTier == {q['targetTier']!r}, esperado 2")
        # 'AbstractRecorderComponent' tem que ter virado um stub Tier-2 (fechamento por base)
        check("AbstractRecorderComponent" in tier2,
              "AbstractRecorderComponent deveria estar em tier2 (base de OutputHandler/AbstractDataRecorder, "
              "fora do conjunto curado de antemao)")

    adr = classes.get("AbstractDataRecorder")
    check(adr is not None, "AbstractDataRecorder nao esta no resultado")
    if adr:
        check(adr["base"] == "AbstractRecorderComponent",
              f"AbstractDataRecorder.base == {adr['base']!r}, esperado 'AbstractRecorderComponent'")
        c = component(adr, "sta")
        check(c is not None and c["target"] == "Station", f"AbstractDataRecorder.sta deveria compor Station, veio {c}")

    # -- Ntm.tPlayer -> Player, via safe_ptr<const models::Player> ----------
    ntm = classes.get("Ntm")
    check(ntm is not None, "Ntm nao esta no resultado")
    if ntm:
        check(ntm["base"] == "Object", f"Ntm.base == {ntm['base']!r}, esperado 'Object'")
        tp = component(ntm, "tPlayer")
        check(tp is not None and tp["target"] == "Player",
              f"Ntm.tPlayer deveria compor Player (base::safe_ptr<const models::Player> tPlayer;), veio {tp}")
        check(tp is not None and tp["multiplicity"] == "one",
              f"Ntm.tPlayer.multiplicity == {tp and tp['multiplicity']!r}, esperado 'one'")

    # -- Simulation.newPlayerQueue -> Pair, via safe_queue<Pair*> (many) -----
    sim = classes.get("Simulation")
    check(sim is not None, "Simulation nao esta no resultado")
    if sim:
        npq = component(sim, "newPlayerQueue")
        check(npq is not None and npq["target"] == "Pair",
              f"Simulation.newPlayerQueue deveria compor Pair (base::safe_queue<base::Pair*>), veio {npq}")
        check(npq is not None and npq["multiplicity"] == "many",
              f"Simulation.newPlayerQueue.multiplicity == {npq and npq['multiplicity']!r}, esperado 'many' "
              f"(safe_queue e' sempre um container)")

    # -- Player: exatamente os 10 papeis via typeid(), mesmo EXPECTED_ROLES -
    # ja hard-coded em test_edl_catalog.py -- mais exatamente UM alvo "outro"
    # descoberto (SynchronizedState, duas ocorrencias: syncState1/syncState2).
    EXPECTED_ROLES = {
        "dynamicsModel": "DynamicsModel", "pilot": "Pilot", "navigation": "Navigation",
        "datalink": "Datalink", "radio": "Radio", "gimbal": "Gimbal",
        "rfSensor": "RfSensor", "irSystem": "IrSystem",
        "onboardComputer": "OnboardComputer", "storesMgr": "StoresMgr",
    }
    player = classes.get("Player")
    check(player is not None, "Player nao esta no resultado")
    if player:
        check(player["base"] == "AbstractPlayer", f"Player.base == {player['base']!r}, esperado 'AbstractPlayer'")
        role_components = {c["name"]: c["target"] for c in player["components"]
                            if c["resolvedVia"] == "typeid()-in-setter-body"}
        check(role_components == EXPECTED_ROLES,
              f"Player: componentes via typeid() == {role_components}, esperado {EXPECTED_ROLES} -- "
              f"regressao na extracao mecanica de Player::updateSystemPointers() (Player.cpp)")
        for role_name in EXPECTED_ROLES:
            c = component(player, role_name)
            check(c is not None and c["multiplicity"] == "one" and c["targetTier"] == 2,
                  f"Player.{role_name} deveria ser multiplicity='one'/targetTier=2, veio {c}")
        # nenhum dos 10 campos brutos base::Pair* (dynamicsModel/datalink/gimbal/
        # nav/obc/pilot/radio/sensor/irSystem/sms) deveria sobrar como ATRIBUTO
        # generico tipo 'Pair' -- foram substituidos pelas entradas typeid() acima.
        leaked_pair_attrs = [a for a in player["attributes"] if a["type"].endswith("Pair") or a["type"] == "base::Pair"]
        check(not leaked_pair_attrs,
              f"Player: atributos com tipo bruto 'Pair' vazaram para a lista de atributos "
              f"(deveriam ter sido substituidos pelos 10 componentes typeid()): {leaked_pair_attrs}")

        other_components = [c for c in player["components"] if c["resolvedVia"] == "direct-member"
                             and c["target"] not in ext.curated_names_for_test()]
        discovered_targets = sorted({c["target"] for c in other_components})
        check(discovered_targets == ["SynchronizedState"],
              f"Player: alvos 'outros' (nao curados de antemao) == {discovered_targets}, "
              f"esperado exatamente ['SynchronizedState'] (syncState1/syncState2, por VALOR)")
        sync_entries = [c for c in player["components"] if c["target"] == "SynchronizedState"]
        check(len(sync_entries) == 2 and {c["name"] for c in sync_entries} == {"syncState1", "syncState2"},
              f"Player.syncState1/syncState2 -> SynchronizedState: veio {sync_entries}")
        check("SynchronizedState" in tier2 and tier2["SynchronizedState"]["base"] == "Object",
              f"SynchronizedState deveria estar em tier2 com base 'Object', veio {tier2.get('SynchronizedState')}")

    # -- Pair (Tier-1) nunca pode aparecer em tier2 --------------------------
    # regressao: o caso especial de PairStream (composicao inferida) registrava
    # 'Pair' como alvo "outro descoberto" mesmo sendo uma das 18 Tier-1.
    check("Pair" not in tier2, "'Pair' apareceu em tier2, mas e' uma classe Tier-1 (regressao do caso especial de PairStream)")

    # -- Toda classe Tier-1 tambem nao pode reaparecer em tier2 --------------
    tier1_in_tier2 = sorted(set(ext.TIER1_HEADERS) & set(tier2))
    check(not tier1_in_tier2, f"classes Tier-1 vazando para tier2: {tier1_in_tier2}")

    # -- Todo alvo de composicao com targetTier==2 tem que estar em tier2 ----
    missing_tier2 = sorted({
        c["target"] for data in classes.values() for c in data["components"]
        if c["targetTier"] == 2 and c["target"] not in tier2
    })
    check(not missing_tier2, f"alvos targetTier=2 sem stub correspondente em tier2: {missing_tier2}")

    # -- Todo alvo de composicao com targetTier==1 tem que estar em classes --
    missing_tier1 = sorted({
        c["target"] for data in classes.values() for c in data["components"]
        if c["targetTier"] == 1 and c["target"] not in classes
    })
    check(not missing_tier1, f"alvos targetTier=1 sem entrada Tier-1 correspondente: {missing_tier1}")

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print(f"OK -- {len(classes)} classes Tier-1, {len(tier2)} stubs Tier-2, sem sobra de texto, "
          f"sem regressao de self-loop.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
