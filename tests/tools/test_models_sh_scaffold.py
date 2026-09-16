#!/usr/bin/env python3
"""Teste de integracao leve de scripts/models.sh (--name/--category/--no-build/--remove).

scripts/models.sh (~850 linhas, o gerador/removedor de modelo por tras de
'make new-model'/'make rm-model') nao tinha cobertura automatizada, apesar
de a logica nao ser trivial (deteccao de colisao de namespace C++,
reescrita de profundidade de caminhos relativos, um modo --remove seguro
a Ctrl+C no meio) e ja ter tido um bug real: a derivacao de namespace via
'tr -cd' colidia 'auto_pilot' e 'autopilot' no mesmo namespace
'xautopilot' (ver o comentario de NS_NOVO em scripts/models.sh).

Sem framework nenhum (subprocess + asserts + exit code), mesmo estilo dos irmaos em tests/tools/.
Usa --no-build (pula o build de fumaca -- so' interessa aqui a logica de texto/caminho, nao
compilar nada) e sempre limpa o proprio scaffold em um 'finally', porque cria sob a arvore REAL
de models/ (a validacao de --dest do proprio script recusa qualquer destino fora do repositorio,
e o modo --remove --name recusa qualquer destino fora de models/ -- nao ha como isolar isto num
/tmp comum sem reimplementar meio script).
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "scripts" / "models.sh"

# Nome obviamente de teste (prefixo 'zzteste-' nao usado por nenhum modelo real) e com hifen, pra
# exercitar a MESMA traducao hifen->underscore do namespace que o bug real ja mordeu.
NOME_TESTE = "zzteste-scaffold-models-sh"
NAMESPACE_ESPERADO = "xzzteste_scaffold_models_sh"

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)


def run(*args):
    return subprocess.run(
        ["bash", str(SCRIPT), *args], cwd=REPO_ROOT, capture_output=True, text=True
    )


def cleanup(nome, categoria="others"):
    """Remove o scaffold se ainda existir -- chamado de dentro de um finally, entao
    tolerante: nunca deixa uma excecao daqui mascarar a falha real do teste."""
    dest = REPO_ROOT / "models" / categoria / nome
    if not dest.exists():
        return
    proc = run("--remove", "--name", nome, "--category", categoria, "--force")
    if proc.returncode != 0 or dest.exists():
        # Ultimo recurso: nao deixar lixo em models/ mesmo se --remove falhar.
        shutil.rmtree(dest, ignore_errors=True)


def test_criar_estrutura_namespace_remover():
    dest = REPO_ROOT / "models" / "others" / NOME_TESTE
    try:
        criar = run("--name", NOME_TESTE, "--category", "others", "--no-build")
        check(criar.returncode == 0, f"criacao falhou (rc={criar.returncode}): {criar.stderr}")
        check(dest.is_dir(), f"{dest} nao foi criado")
        if not dest.is_dir():
            return  # nada mais a checar sem a pasta

        # -- as cinco pecas que check_modelo_estrutura.sh tambem cobra -----------------
        for peca in ("tests", "docs"):
            check((dest / peca).is_dir(), f"falta {peca}/ em {NOME_TESTE}")
        for peca in ("README.md", "CHANGELOG.md", "Makefile", "meson.build"):
            check((dest / peca).is_file(), f"falta {peca} em {NOME_TESTE}")

        # -- namespace derivado corretamente (hifen -> underscore, prefixo 'x') --------
        # Trava o valor certo do namespace derivado (nao so 'existe algum
        # namespace') -- mesma classe de bug ja vista com auto_pilot/autopilot
        # colidindo no mesmo namespace.
        achou_namespace = any(
            NAMESPACE_ESPERADO in p.read_text(encoding="utf-8", errors="ignore")
            for p in dest.rglob("*.cpp")
        )
        check(achou_namespace, f"namespace '{NAMESPACE_ESPERADO}' nao apareceu em nenhum .cpp gerado")

        # -- profundidade de ROOT certa para models/<categoria>/<nome>/ (3 niveis) -----
        makefile = (dest / "Makefile").read_text(encoding="utf-8")
        check("$(abspath ../../..)" in makefile,
              "Makefile nao tem 'ROOT := $(abspath ../../..)' -- profundidade errada para "
              "models/others/<nome>/")

        # -- o mirror de contrato (nao faz parte do scaffold copiavel) foi removido -----
        check(not any(dest.rglob("*mirror*")),
              f"sobrou arquivo com 'mirror' no nome em {NOME_TESTE} -- devia ter sido removido "
              "(nao faz parte do scaffold copiavel, so do proprio models/template/)")

        # -- remocao real (o caminho que um usuario de verdade usaria) ------------------
        remover = run("--remove", "--name", NOME_TESTE, "--category", "others", "--force")
        check(remover.returncode == 0,
              f"remocao falhou (rc={remover.returncode}): {remover.stderr}")
        check(not dest.exists(), f"{dest} ainda existe depois de --remove --force")
    finally:
        cleanup(NOME_TESTE)


def test_colisao_de_namespace_e_recusada():
    # 'A_4' (underscore) deriva para o MESMO namespace que 'A-4' (hifen) ja usa de
    # verdade em producao -- exatamente a classe de bug (duas grafias, mesmo
    # namespace) que a checagem de colisao existe para pegar ANTES de copiar
    # qualquer arquivo. Nada deve ser criado.
    dest = REPO_ROOT / "models" / "others" / "A_4"
    try:
        criar = run("--name", "A_4", "--category", "others", "--no-build")
        check(criar.returncode != 0,
              "criar '--name A_4' deveria ter sido RECUSADO (colide com o namespace de "
              "models/players/air/A-4), mas devolveu rc=0")
        check("xA_4" in criar.stderr, "mensagem de recusa nao menciona o namespace colidido 'xA_4'")
        check(not dest.exists(), f"{dest} foi criado apesar da colisao de namespace esperada")
    finally:
        cleanup("A_4")


def test_categoria_aninhada_cria_e_remove():
    """--category aceita QUALQUER caminho relativo a models/, inclusive um
    NOVO com mais de um nivel (ex.: 'players/air') -- nao um enum fixo. Sem
    isso, mover um modelo entre subpastas de models/ (como a reorganizacao
    por taxonomia MIXR fez: players/<nome>/ -> players/<subcategoria>/
    <nome>/) exigiria editar este script."""
    categoria = "players/air"
    dest = REPO_ROOT / "models" / categoria / NOME_TESTE
    try:
        criar = run("--name", NOME_TESTE, "--category", categoria, "--no-build")
        check(criar.returncode == 0, f"criacao em categoria aninhada falhou (rc={criar.returncode}): {criar.stderr}")
        check(dest.is_dir(), f"{dest} nao foi criado")
        if not dest.is_dir():
            return

        # profundidade de ROOT certa para models/<subcat1>/<subcat2>/<nome>/ (4 niveis)
        makefile = (dest / "Makefile").read_text(encoding="utf-8")
        check("$(abspath ../../../..)" in makefile,
              "Makefile nao tem 'ROOT := $(abspath ../../../..)' -- profundidade errada para "
              f"models/{categoria}/<nome>/")

        remover = run("--remove", "--name", NOME_TESTE, "--category", categoria, "--force")
        check(remover.returncode == 0,
              f"remocao de categoria aninhada falhou (rc={remover.returncode}): {remover.stderr}")
        check(not dest.exists(), f"{dest} ainda existe depois de --remove --force")
    finally:
        cleanup(NOME_TESTE, categoria)


def test_categoria_reservada_e_recusada():
    """'template'/'events' sao subpastas RESERVADAS de models/ (a primeira e'
    o unico ponto de partida copiavel, a segunda e' o projeto Meson do
    contrato de eventos) -- nunca destino de scaffold, com ou sem sufixo."""
    for categoria in ("template", "events", "template/sub", "events/sub"):
        dest = REPO_ROOT / "models" / categoria / NOME_TESTE
        try:
            criar = run("--name", NOME_TESTE, "--category", categoria, "--no-build")
            check(criar.returncode != 0,
                  f"criar '--category {categoria}' deveria ter sido RECUSADO, devolveu rc=0")
            check(not dest.exists(), f"{dest} foi criado apesar de a categoria ser reservada")
        finally:
            if dest.exists():
                shutil.rmtree(dest, ignore_errors=True)


def main() -> int:
    if not SCRIPT.is_file():
        sys.exit(f"{SCRIPT} nao existe -- este teste esta desatualizado?")

    test_criar_estrutura_namespace_remover()
    test_colisao_de_namespace_e_recusada()
    test_categoria_aninhada_cria_e_remove()
    test_categoria_reservada_e_recusada()

    if failures:
        print(f"FALHOU ({len(failures)}):")
        for f in failures:
            print(" -", f)
        return 1

    print("OK -- scripts/models.sh: criacao + estrutura + namespace + remocao, e a colisao de "
          "namespace continua sendo recusada.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
