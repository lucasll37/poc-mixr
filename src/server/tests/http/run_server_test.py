#!/usr/bin/env python3
"""Contrato HTTP do 'server' ponta a ponta -- sobe o binario de verdade
(com o 'sim-runner' irmao ja instalado em dist/, mesma dependencia que
scenario-app-* ja tem: precisa de 'make install' rodado antes, o que
'meson test'/'make test' ja garantem) e exercita os casos descritos em
../../README.md: health, uma simulacao valida (fixture ao lado), e os
cinco caminhos de erro (PluginLoader bloqueado, corpo vazio, frames fora
do limite, EDL malformado, rota desconhecida).

So biblioteca padrao (urllib) -- nenhuma dependencia Python nova.
"""

import argparse
import json
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[4]
FIXTURE_VALIDA = Path(__file__).resolve().parent / "fixtures" / "valid_players.epp"

# Vocabulario aberto de proposito: o que importa e que o campo exista e seja
# uma string conhecida OU o default "--" (sem agente) -- nao travar aqui o
# vocabulario inteiro da arvore de comportamento do modelo 'flight'.
ROTULOS_CONHECIDOS = {"PATROL", "EVADE", "BREAK", "SUPPORT", "RTB", "HOME", "SAFETY", "--", "?"}


def porta_livre():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def chamar(metodo, url, corpo=None, timeout=30):
    dados = corpo.encode("utf-8") if corpo is not None else None
    req = urllib.request.Request(url, data=dados, method=metodo)
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.status, resp.read().decode("utf-8")
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8")


def esperar_pronto(base, tentativas=50, intervalo=0.2):
    for _ in range(tentativas):
        try:
            status, _ = chamar("GET", base + "/health", timeout=1)
            if status == 200:
                return True
        except Exception:
            pass
        time.sleep(intervalo)
    return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--server-binario", required=True)
    ap.add_argument("--runner-binario", required=True)
    args = ap.parse_args()

    porta = porta_livre()
    base = f"http://127.0.0.1:{porta}"

    print(f"  subindo server na porta {porta} (runner={args.runner_binario}) ...")
    proc = subprocess.Popen(
        [args.server_binario, "-port", str(porta), "-runner", args.runner_binario,
         "-timeout", "60"],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, cwd=RAIZ,
    )

    falhas = []
    try:
        if not esperar_pronto(base):
            saida_err = proc.stderr.read() if proc.stderr else ""
            print(f"  FALHA server nao respondeu /health a tempo. stderr:\n{saida_err}")
            return 1

        # 1) health
        status, corpo = chamar("GET", base + "/health")
        if status != 200 or json.loads(corpo).get("status") != "ok":
            falhas.append(f"/health: esperava 200 status=ok, veio {status} {corpo}")

        # 2) simulacao valida -- a fixture ja foi exercitada manualmente
        # antes de virar arquivo (ver o cabecalho dela); aqui so confere
        # que a API entrega o mesmo resultado pelo caminho HTTP completo.
        cenario = FIXTURE_VALIDA.read_text()
        status, corpo = chamar("POST", base + "/simulate?frames=50", cenario)
        if status != 200:
            falhas.append(f"/simulate valido: esperava 200, veio {status} {corpo}")
        else:
            d = json.loads(corpo)
            por_nome = {p["name"]: p for p in d.get("players", [])}
            if set(por_nome) != {"alpha1", "bandit1"}:
                falhas.append(f"/simulate valido: esperava players {{alpha1,bandit1}}, veio {set(por_nome)}")
            if por_nome.get("alpha1", {}).get("behaviorLabel") not in ROTULOS_CONHECIDOS:
                falhas.append("/simulate valido: alpha1.behaviorLabel fora do vocabulario conhecido")
            if por_nome.get("bandit1", {}).get("decisions", -1) != 0:
                falhas.append("/simulate valido: bandit1 nao tem agente -- decisions deveria ser 0")
            if d.get("framesRun") != 50:
                falhas.append(f"/simulate valido: framesRun esperado 50, veio {d.get('framesRun')}")

        # 3) corpo com PluginLoader -> 400 (bloqueia dlopen arbitrario)
        status, corpo = chamar("POST", base + "/simulate", "x: ( PluginLoader )")
        if status != 400 or "PluginLoader" not in corpo:
            falhas.append(f"/simulate com PluginLoader: esperava 400 citando PluginLoader, veio {status} {corpo}")

        # 4) corpo vazio -> 400
        status, corpo = chamar("POST", base + "/simulate", "")
        if status != 400:
            falhas.append(f"/simulate vazio: esperava 400, veio {status} {corpo}")

        # 5) frames fora do limite -> 400
        status, corpo = chamar("POST", base + "/simulate?frames=999999", "x: (Aircraft)")
        if status != 400:
            falhas.append(f"/simulate frames invalido: esperava 400, veio {status} {corpo}")

        # 6) EDL malformado -> 422, com o stderr do edl_parser visivel
        status, corpo = chamar("POST", base + "/simulate?frames=5", "isto nao e edl {{{")
        if status != 422:
            falhas.append(f"/simulate EDL malformado: esperava 422, veio {status} {corpo}")
        elif "exitCode" not in json.loads(corpo):
            falhas.append("/simulate EDL malformado: resposta 422 sem 'exitCode'")

        # 7) rota desconhecida -> 404
        status, _ = chamar("GET", base + "/rota-que-nao-existe")
        if status != 404:
            falhas.append(f"rota desconhecida: esperava 404, veio {status}")

    finally:
        proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=10)

    if falhas:
        print()
        for f in falhas:
            print(f"  FALHA {f}")
        print("server/http: FALHOU")
        return 1

    print("server/http: OK (health, simulacao valida, e os 5 casos de erro do README)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
