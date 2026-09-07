#!/usr/bin/env python3
"""Regressao do catalogo do editor grafico de .edl (src/ui/scripts/generate_edl_catalog.py).

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
  * Os 10 'papeis primarios' que Player::updateSystemPointers() resolve por
    TIPO (dynamicsModel/pilot/navigation/datalink/radio/gimbal/rfSensor/
    irSystem/onboardComputer/storesMgr) tem que aparecer em
    primaryComponents, herdados por qualquer descendente de Player -- e a
    unica coisa deste catalogo que NAO vem de slot nenhum (ver o comentario
    de extract_primary_components() no proprio gerador).
  * introspect_thirdparty_plugins() (o caminho de RUNTIME, via o binario
    'plugininfo', para .so de plugins/ sem fonte C++ neste repositorio):
    um nome de fabrica ja conhecido pelo scan estatico nao pode duplicar
    entrada; um nome NOVO tem que aparecer, com slot herdado de um
    ancestral CONHECIDO (ex.: 'components') recebendo o TIPO de verdade
    (nao 'typeUnknown'), e um slot PROPRIO da classe (sem fonte) caindo no
    fallback texto-ou-numero com 'typeUnknown': True.
  * LIST_SLOT_TYPE_OVERRIDES/TEXT_ONLY_LIST_SLOTS -- os ~35 slots-lista cujo
    ON_SLOT so declara 'base::PairStream' (o dynamic_cast/isClassType() de
    cada item mora dentro do CORPO do setter, nao na assinatura) precisam
    sair do catalogo com objectTypes preenchido OU textOnly=True -- nunca os
    dois vazios, que seria um slot sem NENHUM jeito de preencher na UI
    (isCompatible() nao tem mais fallback permissivo).
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "src" / "ui" / "scripts"))

import generate_edl_catalog as ext  # noqa: E402

REAL_SCENARIOS = [
    "src/poc/dis/single-thread/configs/scenario.edl.in",
    "src/poc/dis/single-thread/configs/scenario_missile_demo.edl.in",
    "src/poc/dis/multi-thread/configs/scenario.edl.in",
    "src/poc/built-in_mixr_1/configs/scenario_max_player.edl.in",
    "src/poc/onnx-policy/configs/scenario.edl.in",
    "src/poc/python-flight/configs/scenario.edl.in",
    "src/poc/dis/bandit/configs/scenario.edl",
    "src/poc/rl-training/configs/scenario_rl.edl",
    "src/rl/configs/scenario_rl.edl",
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

    # -- primaryComponents (papeis por TIPO, nao por slot) de Player --------
    EXPECTED_ROLES = {
        "dynamicsModel": "DynamicsModel", "pilot": "Pilot", "navigation": "Navigation",
        "datalink": "Datalink", "radio": "Radio", "gimbal": "Gimbal",
        "rfSensor": "RfSensor", "irSystem": "IrSystem",
        "onboardComputer": "OnboardComputer", "storesMgr": "StoresMgr",
    }
    player = by_factory.get("Player")
    check(player is not None, "Player nao esta no catalogo")
    if player:
        roles = {r["role"]: r["baseClass"] for r in player.get("primaryComponents", [])}
        check(roles == EXPECTED_ROLES,
              f"Player.primaryComponents == {roles}, esperado {EXPECTED_ROLES} -- regressao na "
              f"extracao mecanica de Player::updateSystemPointers() (Player.cpp)")
    aircraft = by_factory.get("Aircraft")
    check(aircraft is not None, "Aircraft nao esta no catalogo")
    if aircraft:
        roles = {r["role"]: r["baseClass"] for r in aircraft.get("primaryComponents", [])}
        check(roles == EXPECTED_ROLES,
              f"Aircraft.primaryComponents == {roles}, esperado {EXPECTED_ROLES} -- deveria herdar "
              f"de Player (Aircraft nao sobrescreve updateSystemPointers())")
    antenna_primary = (by_factory.get("Antenna") or {}).get("primaryComponents", [])
    check(antenna_primary == [], "Antenna.primaryComponents deveria ser vazio -- Antenna nao e um Player")

    # -- origem de QUALQUER coisa sob ./models/, nao so models/players/<x>/ ---
    a4_entries = [f for f, e in by_factory.items() if e["origin"] == "plugin:A-4"]
    check(len(a4_entries) > 0, "nenhuma classe com origin=='plugin:A-4' -- models/players/A-4 sumiu do catalogo")
    missile_entries = [f for f, e in by_factory.items() if e["origin"] == "plugin:missile"]
    check(len(missile_entries) > 0, "nenhuma classe com origin=='plugin:missile' -- models/players/missile sumiu do catalogo")
    tactical_alert = by_factory.get("TacticalAlert")
    check(tactical_alert is not None, "TacticalAlert nao esta no catalogo")
    if tactical_alert:
        check(tactical_alert["origin"] == "plugin:events",
              f"TacticalAlert.origin == {tactical_alert['origin']!r}, esperado 'plugin:events' -- "
              f"regressao: uma classe sob models/ que NAO e models/players/<nome>/ "
              f"(aqui, models/events/payloads/EID_ALERT/) caindo de volta pra 'builtin' por engano")

    # -- primeiro-achado-vence entre models/players/A-4 e fixtures/stub -------
    bt_behavior = by_factory.get("BtBehavior")
    check(bt_behavior is not None, "BtBehavior nao esta no catalogo")
    if bt_behavior:
        check(slot(bt_behavior, "patrolHeading") is not None,
              "BtBehavior.patrolHeading nao encontrado -- regressao: o stub "
              "(fixtures/stub, varrido depois de A-4) pode ter sobrescrito os slots reais")

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

    # -- LIST_SLOT_TYPE_OVERRIDES/TEXT_ONLY_LIST_SLOTS: nenhum slot-lista ----
    # sobra com objectTypes vazio E textOnly=False -- isCompatible() nao tem
    # mais fallback permissivo (edl_builder_core.js), entao um slot assim
    # seria um beco sem saida (nao aceita classe nenhuma, nem tem "+texto"
    # oferecido pela UI). Ver o comentario de LIST_SLOT_TYPE_OVERRIDES no
    # proprio gerador para a lista dos ~35 casos ja cobertos.
    orphans = sorted({
        (s["declaredIn"], s["name"])
        for e in catalog
        for s in e["slots"]
        if s.get("acceptsChildList") and not s.get("objectTypes") and not s.get("textOnly")
    })
    check(not orphans,
          f"slot-lista sem objectTypes e sem textOnly (beco sem saida na UI): {orphans} -- "
          f"acrescente em LIST_SLOT_TYPE_OVERRIDES ou TEXT_ONLY_LIST_SLOTS")

    # -- spot-check de dois casos reais de cada categoria --------------------
    station = by_factory.get("Station")
    check(station is not None, "Station nao esta no catalogo")
    if station:
        networks = slot(station, "networks")
        check(networks is not None and networks["objectTypes"] == ["AbstractNetIO"],
              f"Station.networks.objectTypes == {networks and networks['objectTypes']}, esperado ['AbstractNetIO'] "
              f"-- Station::setSlotNetworks() faz dynamic_cast<AbstractNetIO*> (simulation/Station.cpp)")
    tacview = by_factory.get("TacviewOutput")
    check(tacview is not None, "TacviewOutput nao esta no catalogo")
    if tacview:
        type_map = slot(tacview, "typeMap")
        check(type_map is not None and type_map.get("textOnly") is True and type_map["objectTypes"] == [],
              f"TacviewOutput.typeMap deveria ser textOnly com objectTypes vazio, veio {type_map}")

    # -- toda classe usada nos 9 cenarios reais aparece no catalogo ---------
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

    # -- introspeccao de plugin de terceiro (runtime, via plugininfo) --------
    # Nao depende do binario 'plugininfo' de verdade (pode nao estar
    # compilado nesta maquina) -- injeta um PLUGININFO_CANDIDATES falso que
    # devolve JSON fixo, do mesmo jeito que o binario real devolveria.
    inheritance = ext.build_inheritance(ext.EDL_CATALOG_INCLUDE_ROOTS + ext.EDL_CATALOG_SRC_ROOTS)
    descendants = ext.build_descendants(inheritance)
    slot_names_map = ext.extract_slots(ext.EDL_CATALOG_SRC_ROOTS)
    slot_types_map = ext.extract_slot_types(ext.EDL_CATALOG_SRC_ROOTS)
    factory_map_test = ext.build_factory_map(ext.EDL_CATALOG_SRC_ROOTS)
    class_to_factory = {}
    for fname, cls in factory_map_test.items():
        class_to_factory.setdefault(cls, fname)
    reference_slots = set(ext.load_edl_catalog_overrides().get("referenceSlots", {}))
    primary = ext.extract_primary_components()

    fake_plugins_dir = REPO_ROOT / "build" / "tests-tmp-fake-plugins"
    fake_plugins_dir.mkdir(parents=True, exist_ok=True)
    (fake_plugins_dir / "libAcmeThirdParty.so").write_bytes(b"")
    fake_binary = fake_plugins_dir / "fake_plugininfo.py"
    fake_binary.write_text(
        "#!/usr/bin/env python3\n"
        "import json\n"
        "print(json.dumps({'classes': [{'factory': 'AcmeRadar', 'class': 'AcmeRadar',"
        " 'chain': ['AcmeRadar', 'RfSensor', 'System', 'Component', 'Object'],"
        " 'slots': ['components', 'acmeGain']}]}))\n",
        encoding="utf-8",
    )
    fake_binary.chmod(0o755)
    try:
        old_candidates = ext.PLUGININFO_CANDIDATES
        old_dir = ext.PLUGINS_DIR
        ext.PLUGININFO_CANDIDATES = [fake_binary]
        ext.PLUGINS_DIR = fake_plugins_dir
        thirdparty = ext.introspect_thirdparty_plugins(
            set(factory_map_test.keys()), inheritance, slot_names_map, slot_types_map,
            descendants, class_to_factory, reference_slots, primary,
        )
    finally:
        ext.PLUGININFO_CANDIDATES = old_candidates
        ext.PLUGINS_DIR = old_dir
        fake_binary.unlink()
        (fake_plugins_dir / "libAcmeThirdParty.so").unlink()
        fake_plugins_dir.rmdir()

    check(len(thirdparty) == 1, f"esperava 1 classe nova (AcmeRadar), achou {len(thirdparty)}")
    if thirdparty:
        acme = thirdparty[0]
        check(acme["origin"] == "plugin:AcmeThirdParty",
              f"origin deveria vir do nome do arquivo (libAcmeThirdParty.so -> plugin:AcmeThirdParty), veio {acme['origin']!r}")
        check(acme.get("runtimeOnly") is True, "classe de terceiro deveria vir marcada runtimeOnly")
        slots_by_name = {s["name"]: s for s in acme["slots"]}
        comp = slots_by_name.get("components")
        check(comp is not None and comp["acceptsChildList"] and not comp.get("typeUnknown"),
              f"'components' deveria herdar o TIPO de verdade de base::Component, veio {comp}")
        gain = slots_by_name.get("acmeGain")
        check(gain is not None and gain.get("typeUnknown") is True and gain["acceptsText"] and gain["acceptsNumber"],
              f"'acmeGain' (sem fonte) deveria cair no fallback texto-ou-numero com typeUnknown, veio {gain}")

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
