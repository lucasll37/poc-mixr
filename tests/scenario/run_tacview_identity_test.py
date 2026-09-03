#!/usr/bin/env python3
"""Identidade dos objetos no stream ACMI -- tipo, cor e modelo corretos.

Roda o cenario 'intercept_missile' do ./app em '-deterministic' (hermetico,
sem TUI, sem TTY, sem rede) e abre o .acmi gerado. O que se afirma e o
contrato VISUAL com o Tacview, que nenhum outro teste cobre:

  1. nenhum objeto sai com Color=Grey -- "Grey" NAO e cor valida do formato
     ACMI (so Red/Orange/Yellow/Green/Cyan/Blue/Violet), e era o default
     antigo de tudo que nao fosse BLUE/RED;
  2. nenhum player sai com Type=Misc -- na base do Tacview "Misc" so casa
     Core.Default, que deliberadamente NAO declara <Shape>, ou seja o objeto
     aparece sem modelo 3D nenhum;
  3. o missil (nome sintetico "W%05d", dado por AbstractWeapon::release())
     sai como Weapon+Missile, com a cor do lancador e um modelo declarado;
  4. as aeronaves saem como Air+FixedWing, azuis, e o intruso vermelho.

Por que ISTO e um teste e nao uma inspecao manual: o tipo/cor/modelo de cada
objeto e emitido UMA UNICA VEZ, na primeira aparicao dele no stream. Uma
regressao na ordem em que TacviewOutput::publishIdentities() e chamado (ela
tem de vir ANTES do updateData que declara) nao quebra nada visivel em
nenhum outro teste -- so faz o Tacview desenhar um quadrado cinza.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]

# "<idhex>,T=...,Name=...,Type=...,Color=...,CallSign=..."
CAMPO = re.compile(r"(Name|Type|Color|CallSign)=([^,\n]+)")

CORES_ACMI_VALIDAS = {"Red", "Orange", "Yellow", "Green", "Cyan", "Blue", "Violet"}
NOME_DE_ARMA = re.compile(r"^W\d{5}$")


def declaracoes(acmi: Path):
    """Um dict por objeto declarado -- so as linhas que trazem Type=."""
    saida = []
    for linha in acmi.read_text(errors="replace").splitlines():
        if "Type=" not in linha:
            continue
        campos = dict(CAMPO.findall(linha))
        if campos:
            saida.append(campos)
    return saida


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binario", required=True)
    parser.add_argument("--frames", type=int, default=1200)
    args = parser.parse_args()

    acmi = RAIZ / "app" / "data" / "recordings" / "mission-intercept-missile.acmi"
    if acmi.exists():
        acmi.unlink()   # senao o teste leria a declaracao de uma execucao antiga

    proc = subprocess.run(
        [args.binario, "-scenario", "intercept_missile", "-threads", "1",
         "-deterministic", str(args.frames)],
        cwd=RAIZ, capture_output=True, text=True, timeout=300,
    )
    if proc.returncode != 0:
        print(f"FALHA: binario saiu com {proc.returncode}\n{proc.stderr[-2000:]}")
        return 1
    if not acmi.exists():
        print(f"FALHA: {acmi} nao foi gerado")
        return 1

    objetos = declaracoes(acmi)
    if not objetos:
        print("FALHA: nenhuma declaracao com Type= no .acmi")
        return 1

    erros = []

    for obj in objetos:
        cor = obj.get("Color")
        if cor is not None and cor not in CORES_ACMI_VALIDAS:
            erros.append(f"cor invalida no ACMI: {cor!r} em {obj}")
        if obj.get("Type") == "Misc":
            erros.append(f"objeto sem taxonomia util (Type=Misc, sem modelo 3D): {obj}")

    aeronaves = {o["CallSign"]: o for o in objetos
                 if "CallSign" in o and not NOME_DE_ARMA.match(o["CallSign"])}
    for nome in ("falcon1", "falcon2", "falcon3", "falcon4"):
        obj = aeronaves.get(nome)
        if obj is None:
            erros.append(f"{nome} nao foi declarado no .acmi")
            continue
        if obj.get("Type") != "Air+FixedWing":
            erros.append(f"{nome}: Type={obj.get('Type')!r}, esperado 'Air+FixedWing'")
        if obj.get("Color") != "Blue":
            erros.append(f"{nome}: Color={obj.get('Color')!r}, esperado 'Blue'")

    bandit = aeronaves.get("bandit1")
    if bandit is None:
        erros.append("bandit1 nao foi declarado no .acmi")
    elif bandit.get("Color") != "Red":
        erros.append(f"bandit1: Color={bandit.get('Color')!r}, esperado 'Red'")

    # O missil e o caso que motivou tudo isto: nome sintetico, fora de
    # qualquer mapa do EDL, resolvido so pela identidade que o host publica.
    misseis = [o for o in objetos if NOME_DE_ARMA.match(o.get("CallSign", ""))]
    if not misseis:
        erros.append("nenhum objeto de arma (CallSign W#####) no .acmi -- "
                     f"o missil nao foi lancado em {args.frames} frames?")
    for m in misseis:
        if m.get("Type") != "Weapon+Missile":
            erros.append(f"{m.get('CallSign')}: Type={m.get('Type')!r}, esperado 'Weapon+Missile'")
        if m.get("Color") != "Blue":
            erros.append(f"{m.get('CallSign')}: Color={m.get('Color')!r} -- "
                         "deveria herdar o lado do lancador")
        if not m.get("Name"):
            erros.append(f"{m.get('CallSign')}: sem Name= -- sem modelo 3D no Tacview")

    if erros:
        print(f"FALHA ({len(erros)}):")
        for e in erros:
            print("  -", e)
        print("\nobjetos declarados:")
        for o in objetos:
            print("  ", o)
        return 1

    print(f"OK: {len(objetos)} objetos declarados, {len(misseis)} arma(s), "
          "tipos/cores/modelos validos")
    return 0


if __name__ == "__main__":
    sys.exit(main())
