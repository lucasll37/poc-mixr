#!/usr/bin/env python3
"""Teste de scripts/fetch_srtm.sh -- o baixador de tiles SRTM1 (.hgt.gz) usado por
INSTALL.md/README.md e por libs/xterrain para cobertura de terreno fora dos cinco tiles ja
versionados em shared/data/terrain/srtm/.

Sem framework nenhum (subprocess + asserts + exit code), mesmo estilo dos irmaos em
tests/tools/ (ver test_models_sh_scaffold.py).

HERMETICO DE PROPOSITO -- nunca toca a AWS de verdade nem shared/data/terrain/srtm/:
- os testes de rede sobem um servidor HTTP local (http.server, so' na maquina do teste) e
  apontam o script para ele via a variavel de ambiente SRTM_BASE (adicionada a
  fetch_srtm.sh so' para isto -- uso normal do script nunca a declara).
- --dry-run nunca escreve tile nenhum em disco (so' 'mkdir -p' do destino, que ja existe no
  repositorio) -- e' por isso que so' o modo --dry-run e' exercitado aqui: o download de
  verdade (baixar_um) grava sempre em shared/data/terrain/srtm/, fixo, sem variavel de
  ambiente para redirecionar, e testa-lo aqui sujaria a arvore real do repositorio.
- os testes de 'nome_tile'/'tiles_da_caixa' avaliam so' o PREFIXO do script (as definicoes
  de funcao, antes do loop de parsing de argumentos) num 'bash -c' isolado -- nunca fazem
  'source' do arquivo inteiro, que executaria o 'exit 2' de "nenhum tile pedido" e matary
  o interpretador do teste.
"""
from __future__ import annotations

import contextlib
import http.server
import os
import re
import subprocess
import sys
import threading
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "scripts" / "fetch_srtm.sh"

failures: list[str] = []


def check(condition, message):
    if not condition:
        failures.append(message)


def run(*args, env_extra=None):
    env = os.environ.copy()
    if env_extra:
        env.update(env_extra)
    return subprocess.run(
        ["bash", str(SCRIPT), *args], cwd=REPO_ROOT, capture_output=True, text=True, env=env
    )


def _prefixo_de_funcoes_puras() -> str:
    """As definicoes de 'nome_tile'/'tiles_da_caixa' (e nada do loop de argumentos/rede
    que vem depois) -- ver a nota HERMETICO no docstring do modulo."""
    texto = SCRIPT.read_text(encoding="utf-8")
    marcador = "\nwhile [ $# -gt 0 ]; do"
    idx = texto.index(marcador)
    return texto[:idx]


def _rodar_funcoes_puras(chamadas: str) -> subprocess.CompletedProcess:
    script = _prefixo_de_funcoes_puras() + "\n" + chamadas
    return subprocess.run(["bash", "-c", script], capture_output=True, text=True)


@contextlib.contextmanager
def mock_srtm_server(fixed_size: int = 12345, missing_substrings: tuple[str, ...] = ()):
    """Servidor HTTP local que responde HEAD como o espelho da AWS responderia: 200 com
    Content-Length fixo para qualquer tile, 404 para qualquer caminho que contenha um dos
    'missing_substrings' -- o suficiente para exercitar tamanho_um() em fetch_srtm.sh sem
    rede nenhuma de verdade."""

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_HEAD(self):  # noqa: N802 (nome exigido pela classe base)
            if any(s in self.path for s in missing_substrings):
                self.send_response(404)
                self.end_headers()
                return
            self.send_response(200)
            self.send_header("Content-Length", str(fixed_size))
            self.end_headers()

        def log_message(self, *_args):  # silencia o log padrao no stderr do teste
            pass

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    thread = threading.Thread(target=srv.serve_forever, daemon=True)
    thread.start()
    try:
        yield f"http://127.0.0.1:{srv.server_address[1]}"
    finally:
        srv.shutdown()
        srv.server_close()


# ---------------------------------------------------------------------------
# nome_tile / tiles_da_caixa -- funcoes puras, sem rede
# ---------------------------------------------------------------------------

def test_nome_tile_formata_hemisferios_certos():
    proc = _rodar_funcoes_puras(
        "nome_tile 5 -42\nnome_tile -23 -43\nnome_tile 0 0\nnome_tile -1 179\n"
    )
    check(proc.returncode == 0, f"nome_tile falhou: {proc.stderr}")
    linhas = proc.stdout.splitlines()
    esperado = ["N05W042", "S23W043", "N00E000", "S01E179"]
    check(linhas == esperado, f"nome_tile formatou errado: {linhas} (esperava {esperado})")


def test_tiles_da_caixa_conta_produto_cartesiano_sem_repetir():
    proc = _rodar_funcoes_puras("tiles_da_caixa -1 1 -1 0\n")
    check(proc.returncode == 0, f"tiles_da_caixa falhou: {proc.stderr}")
    linhas = proc.stdout.splitlines()
    check(len(linhas) == 3 * 2, f"esperava 6 tiles (3 lat x 2 lon), veio {len(linhas)}: {linhas}")
    check(len(set(linhas)) == len(linhas), f"tiles_da_caixa repetiu algum nome: {linhas}")
    check("S01W001" in linhas and "N01E000" in linhas,
          f"cantos da caixa nao apareceram: {linhas}")


# ---------------------------------------------------------------------------
# parsing de argumentos -- sem rede
# ---------------------------------------------------------------------------

def test_sem_argumentos_e_erro_amigavel_sem_tocar_rede():
    proc = run()
    check(proc.returncode == 2, f"esperava rc=2 sem argumentos, veio {proc.returncode}")
    check("nenhum tile pedido" in proc.stderr, f"mensagem de erro mudou: {proc.stderr!r}")


def test_help_documenta_bbox_sudeste_brasil_dry_run():
    proc = run("--help")
    check(proc.returncode == 0, f"--help deveria sair 0, saiu {proc.returncode}")
    for flag in ("--bbox", "--sudeste", "--brasil", "--dry-run"):
        check(flag in proc.stdout, f"--help nao menciona {flag}: {proc.stdout!r}")


# ---------------------------------------------------------------------------
# --dry-run -- rede mockada localmente
# ---------------------------------------------------------------------------

def test_dry_run_mostra_quantidade_e_tamanho_sem_listar_nomes():
    with mock_srtm_server(fixed_size=12345) as base:
        proc = run("--bbox", "0", "0", "0", "3", "--dry-run", env_extra={"SRTM_BASE": base})
    check(proc.returncode == 0, f"dry-run deveria sair 0: {proc.stderr}")
    saida = proc.stdout + proc.stderr
    for nome in ("N00E000", "N00E001", "N00E002", "N00E003"):
        check(nome not in saida,
              f"dry-run vazou o nome do tile {nome} -- deveria mostrar so' quantidade+tamanho, "
              f"nunca a relacao nominal (saida: {saida!r})")
    check("4 tiles pedidos" in proc.stdout, f"contagem errada na saida: {proc.stdout!r}")
    check("49380 bytes" in proc.stdout,
          f"tamanho total errado (esperava 4*12345=49380 bytes): {proc.stdout!r}")
    check("dry-run, nada baixado" in proc.stdout,
          f"saida nao deixa claro que nada foi baixado: {proc.stdout!r}")


def test_dry_run_tile_ausente_nao_entra_no_tamanho():
    with mock_srtm_server(fixed_size=1000, missing_substrings=("N00E000",)) as base:
        proc = run("--bbox", "0", "0", "0", "1", "--dry-run", env_extra={"SRTM_BASE": base})
    check(proc.returncode == 0, f"tile ausente nao deveria falhar o dry-run: {proc.stderr}")
    check("2 tiles pedidos" in proc.stdout, f"contagem errada: {proc.stdout!r}")
    check("1000 bytes" in proc.stdout,
          f"deveria contar so' o tile presente (1000 bytes), nao o ausente: {proc.stdout!r}")
    check("1 sem dado no espelho" in proc.stdout,
          f"nao avisou sobre o tile sem dado: {proc.stdout!r}")


def test_dry_run_com_erro_de_rede_sai_com_falha():
    # porta 1 e privilegiada e quase certamente fechada -- conexao recusada na hora,
    # sem esperar timeout nenhum.
    proc = run("S23W043", "--dry-run", env_extra={"SRTM_BASE": "http://127.0.0.1:1"})
    check(proc.returncode == 1, f"erro de rede deveria sair 1, saiu {proc.returncode}")
    check("ERRO" in proc.stderr, f"stderr nao reporta o erro de sondagem: {proc.stderr!r}")


# ---------------------------------------------------------------------------
# --sudeste -- flag nova
# ---------------------------------------------------------------------------

def test_sudeste_bate_com_a_caixa_declarada_no_script():
    texto = SCRIPT.read_text(encoding="utf-8")
    m = re.search(
        r"--sudeste\)\s*mapfile[^\n]*tiles_da_caixa (-?\d+) (-?\d+) (-?\d+) (-?\d+)", texto
    )
    check(m is not None, "nao encontrei a caixa de --sudeste em fetch_srtm.sh (regex desatualizada?)")
    if m is None:
        return
    lat0, lat1, lon0, lon1 = (int(g) for g in m.groups())
    esperado = (lat1 - lat0 + 1) * (lon1 - lon0 + 1)

    with mock_srtm_server(fixed_size=100) as base:
        proc = run("--sudeste", "--dry-run", env_extra={"SRTM_BASE": base})
    check(proc.returncode == 0, f"--sudeste --dry-run falhou: {proc.stderr}")
    check(f"{esperado} tiles pedidos" in proc.stdout,
          "--sudeste pediu um numero de tiles diferente do produto "
          f"(lat1-lat0+1)*(lon1-lon0+1)={esperado} calculado da propria caixa declarada no "
          f"script: {proc.stdout!r}")

    # SP/RJ/MG/ES nao cabem numa faixa estreita -- guarda contra alguem apertar a caixa
    # demais um dia e ela silenciosamente parar de cobrir os quatro estados.
    check(lat1 - lat0 + 1 >= 10,
          f"caixa do --sudeste parece estreita demais em latitude: {lat0}..{lat1}")
    check(lon1 - lon0 + 1 >= 10,
          f"caixa do --sudeste parece estreita demais em longitude: {lon0}..{lon1}")


def test_sudeste_e_brasil_produzem_conjuntos_diferentes():
    """--sudeste tem que ser de fato um SUBCONJUNTO menor, nao um alias disfarcado de
    --brasil (o bug mais facil de introduzir copiando/colando o case)."""
    with mock_srtm_server(fixed_size=1) as base:
        sudeste = run("--sudeste", "--dry-run", env_extra={"SRTM_BASE": base})
        brasil = run("--brasil", "--dry-run", env_extra={"SRTM_BASE": base})
    check(sudeste.returncode == 0, f"--sudeste --dry-run falhou: {sudeste.stderr}")
    check(brasil.returncode == 0, f"--brasil --dry-run falhou: {brasil.stderr}")

    m_sudeste = re.search(r"(\d+) tiles pedidos", sudeste.stdout)
    m_brasil = re.search(r"(\d+) tiles pedidos", brasil.stdout)
    check(m_sudeste is not None and m_brasil is not None,
          f"nao consegui ler a contagem de tiles: sudeste={sudeste.stdout!r} brasil={brasil.stdout!r}")
    if m_sudeste and m_brasil:
        n_sudeste, n_brasil = int(m_sudeste.group(1)), int(m_brasil.group(1))
        check(0 < n_sudeste < n_brasil,
              f"--sudeste ({n_sudeste} tiles) deveria ser um subconjunto estritamente menor "
              f"que --brasil ({n_brasil} tiles)")


def main() -> int:
    if not SCRIPT.is_file():
        sys.exit(f"{SCRIPT} nao existe -- este teste esta desatualizado?")

    test_nome_tile_formata_hemisferios_certos()
    test_tiles_da_caixa_conta_produto_cartesiano_sem_repetir()
    test_sem_argumentos_e_erro_amigavel_sem_tocar_rede()
    test_help_documenta_bbox_sudeste_brasil_dry_run()
    test_dry_run_mostra_quantidade_e_tamanho_sem_listar_nomes()
    test_dry_run_tile_ausente_nao_entra_no_tamanho()
    test_dry_run_com_erro_de_rede_sai_com_falha()
    test_sudeste_bate_com_a_caixa_declarada_no_script()
    test_sudeste_e_brasil_produzem_conjuntos_diferentes()

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print("OK -- scripts/fetch_srtm.sh: nome_tile/tiles_da_caixa, argumentos, --dry-run "
          "(quantidade+tamanho, sem listar nomes) e --sudeste.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
