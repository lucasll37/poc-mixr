#!/usr/bin/env python3
"""Teste de integracao leve de scripts/models.sh (--name/--category/--no-build/--remove).

Achado por auditoria (revisao completa do repositorio): este script (~850 linhas, o proprio
gerador/removedor de modelo por tras de 'make new-model'/'make rm-model') nao tinha NENHUMA
cobertura automatizada -- nem em tests/, nem em .gitlab-ci.yml. A logica que ele executa nao e
trivial (deteccao de colisao de namespace C++, reescrita de profundidade de caminhos relativos,
um modo --remove de varios passos pensado pra ser seguro a Ctrl+C no meio) e ja teve um bug REAL
encontrado so por auditoria manual, nunca por teste: a derivacao de namespace via 'tr -cd'
colidia 'auto_pilot' e 'autopilot' no mesmo namespace 'xautopilot' (corrigido; ver o comentario
de NS_NOVO no proprio scripts/models.sh).

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
        # Mesma classe de bug ja encontrada por auditoria (auto_pilot/autopilot
        # colidindo) -- travando o valor CERTO, nao so' "existe algum namespace".
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
              "models/players/A-4), mas devolveu rc=0")
        check("xA_4" in criar.stderr, "mensagem de recusa nao menciona o namespace colidido 'xA_4'")
        check(not dest.exists(), f"{dest} foi criado apesar da colisao de namespace esperada")
    finally:
        cleanup("A_4")


def main() -> int:
    if not SCRIPT.is_file():
        sys.exit(f"{SCRIPT} nao existe -- este teste esta desatualizado?")

    test_criar_estrutura_namespace_remover()
    test_colisao_de_namespace_e_recusada()

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
