#!/usr/bin/env python3
"""Regressao do catalogo da aba "Catalogo" de docs/manual/doc.jsx
(tools/generate_manual_catalog.py).

Sem framework nenhum (nem pytest, nem gtest) -- mesmo estilo dos demais
scripts deste repositorio (ver tests/tools/test_edl_catalog.py): uma lista
de casos, cada um um assert com mensagem, um contador de falhas e um exit
code. Roda em segundos, sem MIXR, sem Station, so o fonte de
contexts/src/mixr/ e models/.

O que se prova aqui:

  * A soma das classes das 7 factories NATIVAS (base/models/simulation/
    terrain/interop::dis/linkage/recorder) bate com os 225 que
    models/BUILT-IN.md ja documenta como total -- trava contra regressao
    silenciosa se o fork do MIXR mudar de versao ou o recorte de modulos
    mudar sem querer.
  * 'plugin:A-4' aparece com EXATAMENTE as 9 classes que
    models/players/A-4/src/xnative/factory.cpp de fato despacha -- nem
    mais (um nome de outro plugin vazando), nem menos (uma classe nova do
    A-4 esquecida de registrar aqui).
  * Nenhum MODEL[c]['m'] aparece fora dos 8 rotulos esperados (os 7 modulos
    nativos + o plugin) -- o universo e' fechado por construcao.
  * Todo MODEL[c]['r'] e' True -- por definicao deste catalogo (so entra
    classe com despacho REAL), nunca deveria haver excecao.
  * STATS['classes'] == STATS['registered'] == len(MODEL) -- os tres tem
    que concordar, e o segundo e' estruturalmente igual ao primeiro neste
    universo ja filtrado (nao existe mais "declarada mas nao registrada"
    aqui -- essa nocao saiu do catalogo, nao so da UI).
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools"))

import generate_manual_catalog as gen  # noqa: E402

EXPECTED_NATIVE_TOTAL = 225
EXPECTED_PLUGIN_A4_CLASSES = {
    "AlertDatalink", "AltitudeSafetyBehavior", "BtBehavior", "FlightAction",
    "FlightAgentTC", "FlightState", "RLBridgeBehavior", "TacticalAlert",
    "ThreadTagProbe",
}
ALLOWED_MODULES = {
    "base", "models", "simulation", "terrain", "interop/dis", "linkage",
    "recorder", "plugin:A-4",
}
NATIVE_MODULE_LABELS = {label for label, _ in gen.NATIVE_MODULES}

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def main():
    model, factories, snippets, stats = gen.build_all()

    # -- as 7 factories nativas somam 225 (o total de models/BUILT-IN.md) ---
    native_total = sum(
        len(entry["classes"]) for label, entry in factories.items()
        if label in NATIVE_MODULE_LABELS
    )
    check(native_total == EXPECTED_NATIVE_TOTAL,
          f"soma das 7 factories nativas == {native_total}, esperado {EXPECTED_NATIVE_TOTAL} "
          f"(o total que models/BUILT-IN.md documenta) -- regressao no recorte de modulos "
          f"ou o fork do MIXR mudou de versao")
    check(set(factories.keys()) - NATIVE_MODULE_LABELS == {"plugin:A-4"},
          f"FACTORIES tem modulos alem dos 7 nativos + plugin:A-4: "
          f"{sorted(set(factories.keys()) - NATIVE_MODULE_LABELS - {'plugin:A-4'})}")

    # -- plugin:A-4 tem EXATAMENTE as 9 classes que o factory.cpp despacha --
    check("plugin:A-4" in factories, "'plugin:A-4' nao esta em FACTORIES")
    if "plugin:A-4" in factories:
        got = set(factories["plugin:A-4"]["classes"])
        check(got == EXPECTED_PLUGIN_A4_CLASSES,
              f"FACTORIES['plugin:A-4'].classes == {sorted(got)}, esperado "
              f"{sorted(EXPECTED_PLUGIN_A4_CLASSES)}")

    # -- nenhum modulo fora de escopo aparece em MODEL[c]['m'] -------------
    seen_modules = {e["m"] for e in model.values()}
    check(seen_modules <= ALLOWED_MODULES,
          f"MODEL tem classes com modulo fora do universo esperado: "
          f"{sorted(seen_modules - ALLOWED_MODULES)}")

    # -- nenhuma classe orfa (sem despacho real) vaza para o catalogo ------
    ORPHAN_CLASSES = {
        "AirAngleOnlyTrkMgrPT", "ExternalStore", "RfTrack", "IrTrack", "Action",
        "IrSystem", "DynamicsModel", "Component", "BaseStoresMgr",
    }
    leaked = ORPHAN_CLASSES & set(model.keys())
    check(not leaked,
          f"classes orfas (sem despacho em nenhum factory.cpp) vazaram pro catalogo: {sorted(leaked)}")

    # -- toda entrada e' 'registrada' -- por definicao deste universo -------
    unregistered = [c for c, e in model.items() if e["r"] is not True]
    check(not unregistered,
          f"classes com r != True (nao deveria existir neste catalogo, ja filtrado por "
          f"despacho real): {sorted(unregistered)}")

    # -- STATS concorda com o proprio MODEL ---------------------------------
    check(stats["classes"] == len(model),
          f"STATS['classes'] == {stats['classes']}, esperado len(MODEL) == {len(model)}")
    check(stats["registered"] == stats["classes"],
          f"STATS['registered'] == {stats['registered']}, esperado == STATS['classes'] "
          f"({stats['classes']}) -- universo ja filtrado, toda entrada e registrada")
    check(stats["registered"] == len(model),
          f"STATS['registered'] == {stats['registered']}, esperado len(MODEL) == {len(model)}")

    # -- toda classe usada em algum FACTORIES[*].classes tem entrada MODEL,
    # com uma excecao conhecida (documentada no proprio gerador): duas
    # classes C++ DIFERENTES com o MESMO nome barra (base::FileReader,
    # fabrica "FileReader", e recorder::FileReader, fabrica
    # "RecorderFileReader") colidem na mesma chave de MODEL -- o primeiro
    # modulo processado (base) vence, o segundo (recorder) fica sem entrada
    # PROPRIA (mas continua listado em FACTORIES['recorder'].classes).
    all_factory_classes = {
        cls for entry in factories.values() for cls in entry["classes"]
    }
    missing = sorted(all_factory_classes - set(model.keys()))
    check(missing == ["FileReader"] or not missing,
          f"classes em FACTORIES sem entrada em MODEL (alem da colisao conhecida de "
          f"FileReader): {missing}")

    # -- SNIPPETS cobre TODOS os modulos, nao so mixr::models ---------------
    check(any(k.startswith("FlightAgentTC::") for k in snippets),
          "SNIPPETS nao tem nenhum metodo de FlightAgentTC (plugin:A-4) -- deveria cobrir "
          "TODOS os modulos, nao so mixr::models")
    check(any(k.startswith("BtBehavior::") for k in snippets),
          "SNIPPETS nao tem nenhum metodo de BtBehavior (plugin:A-4)")

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print(f"OK -- {len(model)} classes no catalogo ({native_total} nativas + "
          f"{len(factories.get('plugin:A-4', {}).get('classes', []))} de plugin:A-4), "
          f"{len(snippets)} trechos de metodo extraidos.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
