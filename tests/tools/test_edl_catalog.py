#!/usr/bin/env python3
"""Regressao do catalogo de --edl-catalog (tools/extract_execution_chain.py).

Sem framework nenhum (nem pytest, nem gtest) -- mesmo estilo dos demais
scripts deste repositorio: uma lista de casos, cada um um assert com
mensagem, um contador de falhas e um exit code. Roda em milissegundos,
sem MIXR, sem Station, so o fonte de contexts/src/mixr/ e models/.

Cada caso aqui e a prova de uma armadilha JA CONFIRMADA lendo o fonte real
(nao hipotetica) -- ver os comentarios de build_edl_catalog() no proprio
extrator para o raciocinio completo:

  * Antenna.beamWidth / RfSensor.beamWidth -- ON_SLOT duplicado no mesmo
    indice (Angle OU Number) precisa virar UM slot com unidade opcional,
    nao dois slots ou um erro.
  * SrtmHgtFile nao declara slot nenhum -- os que ele expoe (file/path) sao
    herdados de Terrain; sem achatar a cadeia, o catalogo mostraria a classe
    sem nenhum campo editavel.
  * IrSensor.trackManagerName -- IrSensor.cpp tem DUAS linhas ON_SLOT
    COMENTADAS no indice 8 (setSlotElevationBin, base::Number) alem da real
    (setSlotTrackManagerName, base::String); sem mascarar comentario antes
    do regex, o slot sairia aceitando Number OU String por engano.
  * RfSensor.modes -- ON_SLOT(2, ..., PairStream) e ON_SLOT(2, ...,
    RfSensor) no mesmo indice: lista de filhos nomeados OU um filho unico,
    sem tabela de curadoria (a mesma regra generica de 'PairStream + classe
    concreta' resolve os dois casos).
  * Component.select -- String OU Number no mesmo indice (nome ou indice do
    filho): os dois kinds coexistem no mesmo slot, sem crash.
  * DisNetIO/DisNtm tem que sobreviver ao recorte de modulos (que existe
    para tirar interop/hla e interop/rprfom, fonte de colisao de nome barra
    com Aircraft/GroundVehicle/NetIO/Nib/Ntm) -- e nenhum outro nome de
    fabrica pode aparecer duas vezes no catalogo inteiro.
  * Toda classe citada nos cenarios REAIS do repositorio (os 10
    .edl/.edl.in de producao) tem que aparecer no catalogo -- senao a
    ferramenta grafica nao consegue montar nem o que ja existe hoje.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import extract_execution_chain as ext  # noqa: E402

REAL_SCENARIOS = [
    "src/poc/dis/single-thread/configs/scenario.edl.in",
    "src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in",
    "src/poc/dis/multi-thread/configs/scenario.edl.in",
    "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
    "src/poc/onnx-policy/configs/scenario.edl.in",
    "src/poc/python-flight/configs/scenario.edl.in",
    "app/configs/scenario_intercept_missile.edl.in",
    "src/poc/dis/bandit/configs/scenario.edl",
    "src/poc/rl-training/configs/scenario_rl.edl",
    "src/rl/configs/scenario_rl.edl",
    "app/configs/fragments/tacview_recorder.edl.frag",
]

STRING_RE = re.compile(r'"[^"]*"')
TOKEN_RE = re.compile(r"\(\s*([A-Z]\w+)\b")

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def slot(entry, name):
    for s in entry["slots"]:
        if s["name"] == name:
            return s
    return None


def main():
    catalog = ext.build_edl_catalog()
    by_factory = {e["factory"]: e for e in catalog}

    # -- unidade-ou-numero (ON_SLOT duplicado no mesmo indice) ---------------
    antenna = by_factory.get("Antenna")
    check(antenna is not None, "Antenna nao esta no catalogo")
    if antenna:
        bw = slot(antenna, "beamWidth")
        check(bw is not None, "Antenna.beamWidth nao encontrado")
        if bw:
            check(bw["acceptsNumber"], "Antenna.beamWidth deveria aceitar Number")
            families = {u["family"] for u in bw["unitFamilies"]}
            check(families == {"Angle"}, f"Antenna.beamWidth.unitFamilies == {families}, esperado {{'Angle'}}")
            for u in bw["unitFamilies"]:
                check("Degrees" in u["options"], f"Angle deveria oferecer 'Degrees', tem {u['options']}")

    # -- slots herdados (SrtmHgtFile nao declara nenhum slot proprio) --------
    srtm = by_factory.get("SrtmHgtFile")
    check(srtm is not None, "SrtmHgtFile nao esta no catalogo")
    if srtm:
        file_slot = slot(srtm, "file")
        path_slot = slot(srtm, "path")
        check(file_slot is not None and path_slot is not None,
              "SrtmHgtFile deveria herdar os slots 'file'/'path' de Terrain")
        if file_slot:
            check(file_slot["declaredIn"] == "Terrain", f"file.declaredIn == {file_slot['declaredIn']}, esperado 'Terrain'")
        if path_slot:
            check(path_slot["declaredIn"] == "Terrain", f"path.declaredIn == {path_slot['declaredIn']}, esperado 'Terrain'")

    # -- regressao do falso-positivo de ON_SLOT comentado (IrSensor.cpp) ----
    ir_sensor = by_factory.get("IrSensor")
    check(ir_sensor is not None, "IrSensor nao esta no catalogo")
    if ir_sensor:
        tmn = slot(ir_sensor, "trackManagerName")
        check(tmn is not None, "IrSensor.trackManagerName nao encontrado")
        if tmn:
            check(tmn["acceptsText"], "IrSensor.trackManagerName deveria aceitar texto (String)")
            check(not tmn["acceptsNumber"],
                  "IrSensor.trackManagerName aceitando Number -- sinal de que um ON_SLOT "
                  "COMENTADO (setSlotElevationBin, IrSensor.cpp) vazou por falta de mascaramento")

    # -- self-loop de interop/dis (DisNetIO/DisNtm perdiam slot herdado) ----
    dis_net_io = by_factory.get("DisNetIO")
    check(dis_net_io is not None, "DisNetIO nao esta no catalogo")
    if dis_net_io:
        for name in ("inputEntityTypes", "outputEntityTypes", "netInput", "siteID"):
            check(slot(dis_net_io, name) is not None,
                  f"DisNetIO.{name} nao encontrado -- regressao do self-loop "
                  f"NetIO<->interop::NetIO (ver comentario de build_edl_catalog)")
        names = [s["name"] for s in dis_net_io["slots"]]
        check(len(names) == len(set(names)), f"DisNetIO tem slot duplicado: {names}")
    dis_ntm = by_factory.get("DisNtm")
    check(dis_ntm is not None, "DisNtm nao esta no catalogo")
    if dis_ntm:
        check(slot(dis_ntm, "template") is not None, "DisNtm.template (herdado de interop::Ntm) nao encontrado")
        check(slot(dis_ntm, "disEntityType") is not None, "DisNtm.disEntityType (proprio) nao encontrado")

    # -- origem de QUALQUER coisa sob ./models/, nao so models/player/<x>/ ---
    a4_entries = [f for f, e in by_factory.items() if e["origin"] == "plugin:A4"]
    check(len(a4_entries) > 0, "nenhuma classe com origin=='plugin:A4' -- models/player/A4 sumiu do catalogo")
    missile_entries = [f for f, e in by_factory.items() if e["origin"] == "plugin:missile"]
    check(len(missile_entries) > 0, "nenhuma classe com origin=='plugin:missile' -- models/player/missile sumiu do catalogo")
    tactical_alert = by_factory.get("TacticalAlert")
    check(tactical_alert is not None, "TacticalAlert nao esta no catalogo")
    if tactical_alert:
        check(tactical_alert["origin"] == "plugin:events",
              f"TacticalAlert.origin == {tactical_alert['origin']!r}, esperado 'plugin:events' -- "
              f"regressao: uma classe sob models/ que NAO e models/player/<nome>/ "
              f"(aqui, models/events/payloads/EID_ALERT/) caindo de volta pra 'builtin' por engano")

    # -- primeiro-achado-vence entre models/player/A4 e fixtures/stub -------
    bt_behavior = by_factory.get("BtBehavior")
    check(bt_behavior is not None, "BtBehavior nao esta no catalogo")
    if bt_behavior:
        check(slot(bt_behavior, "patrolHeading") is not None,
              "BtBehavior.patrolHeading nao encontrado -- regressao: o stub "
              "(fixtures/stub, varrido depois de A4) pode ter sobrescrito os slots reais")

    # -- multiplos slots numa linha so (estilo compacto) ---------------------
    msg_feed = by_factory.get("MsgFeed")
    check(msg_feed is not None, "MsgFeed nao esta no catalogo")
    if msg_feed:
        for name in ("trackManager", "maxPlayers", "healthEvery", "sinks", "messages"):
            check(slot(msg_feed, name) is not None,
                  f"MsgFeed.{name} nao encontrado -- regressao do estilo compacto "
                  f"('\"a\", \"b\", \"c\",' numa linha so em BEGIN_SLOTTABLE)")

    # -- header co-localizado em src/ (nao em include/) ----------------------
    usb_joystick = by_factory.get("UsbJoystick")
    check(usb_joystick is not None, "UsbJoystick nao esta no catalogo")
    if usb_joystick:
        check(slot(usb_joystick, "adapters") is not None,
              "UsbJoystick.adapters (herdado de IoDevice) nao encontrado -- regressao: "
              "o header desta classe mora em src/linkage/platform/, nao em include/")

    # -- lista-ou-filho-unico (PairStream + classe concreta, sem curadoria) --
    rf_sensor = by_factory.get("RfSensor")
    check(rf_sensor is not None, "RfSensor nao esta no catalogo")
    if rf_sensor:
        modes = slot(rf_sensor, "modes")
        check(modes is not None, "RfSensor.modes nao encontrado")
        if modes:
            check(modes["acceptsChildList"], "RfSensor.modes deveria aceitar lista de filhos (PairStream)")
            check(modes["objectTypes"] == ["RfSensor"],
                  f"RfSensor.modes.objectTypes == {modes['objectTypes']}, esperado ['RfSensor']")

    # -- texto-ou-numero (String + Number no mesmo indice) -------------------
    component = by_factory.get("Component")
    check(component is not None, "Component nao esta no catalogo")
    if component:
        select = slot(component, "select")
        check(select is not None, "Component.select nao encontrado")
        if select:
            check(select["acceptsText"] and select["acceptsNumber"],
                  f"Component.select deveria aceitar texto E numero, tem {select}")

    # -- DisNetIO/DisNtm sobrevivem ao recorte de modulos; sem duplicatas ----
    check("DisNetIO" in by_factory, "DisNetIO sumiu do catalogo (regressao do recorte de modulos)")
    check("DisNtm" in by_factory, "DisNtm sumiu do catalogo (regressao do recorte de modulos)")
    check("HlaNetIO" not in by_factory, "HlaNetIO nao deveria aparecer (interop/hla nao e encadeado por este repositorio)")
    check("RprFomNetIO" not in by_factory, "RprFomNetIO nao deveria aparecer (interop/rprfom nao e encadeado por este repositorio)")

    factory_names = [e["factory"] for e in catalog]
    dupes = sorted({f for f in factory_names if factory_names.count(f) > 1})
    check(not dupes, f"nomes de fabrica duplicados no catalogo: {dupes}")

    # -- toda classe usada nos 12 cenarios reais aparece no catalogo ---------
    used_tokens = set()
    for rel in REAL_SCENARIOS:
        path = REPO_ROOT / rel
        check(path.exists(), f"cenario de referencia nao existe mais: {rel}")
        if not path.exists():
            continue
        text = ext.mask_source(path.read_text(encoding="utf-8", errors="replace"))
        text = STRING_RE.sub(" ", text)
        used_tokens |= set(TOKEN_RE.findall(text))

    missing = sorted(t for t in used_tokens if t not in by_factory)
    check(not missing,
          f"classes usadas em cenarios reais mas ausentes do catalogo: {missing}")

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print(f"OK -- {len(catalog)} entradas no catalogo, "
          f"{len(used_tokens)} classes distintas usadas nos {len(REAL_SCENARIOS)} cenarios reais, todas cobertas.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
