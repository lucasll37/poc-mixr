#!/usr/bin/env python3
"""Levanta um Ubuntu 24.04 LIMPO e roda nele os comandos que o README manda rodar.

A pergunta e uma so: *o README basta?* Alguem que nunca viu este repositorio,
numa maquina recem-instalada, consegue chegar de `git clone` a binario rodando
seguindo apenas a secao 2 (Pre-requisitos) e a secao 3 (Build)?

COMO ELE RESPONDE ISSO

O container roda `tests/docker/gates.sh`, que executa os comandos do README por
PORTOES: tenta, falha, aplica UMA correcao nao documentada, tenta de novo. Cada
correcao necessaria vira um ACHADO. O resultado nao e "buildou / nao buildou" --
e a lista ordenada do que falta escrever no README.

O contexto de build e `git archive HEAD`, nao a arvore de trabalho: e o que um
clone limpo entrega. Testar a arvore de trabalho responderia a pergunta errada,
porque ela ja tem `build/`, `dist/` e o cache do Conan que o recem-chegado nao
tem.

DOIS MODOS

  --modo rapido    (default) vai ate o portao do remote privado. Minutos.
                   Ja basta para provar a insuficiencia e listar os achados
                   1 a 5, que sao os que travam qualquer recem-chegado.

  --modo completo  segue ate `make build` / `make install`. Exige credenciais
                   do remote privado no ambiente e HORAS de relogio -- ver o
                   achado 6 (o remote publica binario so para gcc 11; nesta
                   imagem, gcc 13, o Conan recompila boost/onnxruntime/openssl/
                   mixr do zero).

VEREDITO

Por default o teste compara os achados observados com `gaps_conhecidos.json` e
so fica vermelho quando o conjunto MUDA -- e uma guarda de regressao, no mesmo
espirito de `tests/guard/`: pega o dia em que o README quebrar mais um degrau,
ou o dia em que alguem consertar um e esquecer de atualizar a baseline.
`--exigir-suficiente` inverte isso e cobra a resposta absoluta (verde so com
zero achados), que e o alvo de verdade.
"""

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[2]
AQUI = Path(__file__).resolve().parent
BASELINE = AQUI / "gaps_conhecidos.json"

# ------------------------------------------------------------------------------
# Classificacao: de log cru para ACHADO.
#
# A classificacao e por ASSINATURA no texto, nao por codigo de retorno, e a
# razao e medida: `conan install` devolve rc=1 para uma duzia de causas
# diferentes (perfil ausente, pacote nao encontrado, 401, falha de compilacao).
# O rc sozinho nao distingue "falta configurar o remote" de "o build do boost
# quebrou" -- e a diferenca entre as duas e o teste inteiro.
#
# A ordem importa: a primeira assinatura que casar vence, entao as mais
# especificas vem antes.
# ------------------------------------------------------------------------------
ASSINATURAS = [
    (
        "conan-profile-detect",
        re.compile(
            r"default build profile.*(doesn't|does not) exist"
            r"|profile.*default.*not found"
            r"|conan profile detect",
            re.I,
        ),
    ),
    (
        "conan-credenciais",
        # ARMADILHA (medida, nao redescobrir): num container sem TTY o sintoma
        # NAO e um 401 legivel. O Conan detecta que o remote exige autenticacao,
        # tenta PERGUNTAR o usuario, encontra stdin fechado e morre com
        # "Package 'mixr/1.0.5' not resolved: EOF when reading a line" -- que
        # nao tem a palavra 'credencial' em lugar nenhum e passa facil por erro
        # de rede. Casar so em /401|Unauthorized/ dava falso negativo aqui.
        re.compile(
            r"\b401\b|Unauthorized|Invalid credentials"
            r"|needs authentication|obtaining credentials"
            r"|EOF when reading a line"
            r"|authentication.*(fail|required)|login.*required",
            re.I,
        ),
    ),
    (
        "conan-remote-privado",
        # Montada em tempo de execucao a partir do conanfile.py -- ver
        # padrao_pacotes_privados(). Nenhum nome de pacote e de registry fica
        # escrito aqui: o achado e "o README nomeia pacotes que o conancenter
        # nao tem e nao diz de onde vem", e isso independe de QUAIS pacotes e de
        # QUAL registry -- continua valendo se o projeto trocar de um ou de
        # outro.
        None,
    ),
    (
        "pkg-config-ausente",
        re.compile(
            r"[Pp]kg-config.*(not found|binary.*missing|could not be found)"
            r"|Pkg-config for machine.*not found",
            re.I,
        ),
    ),
    (
        "sudo-ausente",
        re.compile(r"sudo: (not found|command not found)|/bin/sh: .*sudo", re.I),
    ),
    (
        "git-ausente",
        re.compile(r"git: (not found|command not found)|Cannot find.*\bgit\b", re.I),
    ),
]

def padrao_pacotes_privados():
    """Monta a assinatura de 'pacote nao resolvido' a partir do conanfile.py.

    AGNOSTICO AO REGISTRY, e tambem aos pacotes: le os `self.requires(...)` do
    conanfile.py e monta o padrao com os nomes que estiverem la. Uma lista
    escrita aqui envelheceria em silencio -- trocar de dependencia (ou de
    fornecedor) deixaria o teste procurando um nome que ninguem mais pede, e ele
    ficaria verde por nao achar nada.
    """
    texto = (RAIZ / "conanfile.py").read_text(encoding="utf-8")
    nomes = re.findall(r"""self\.requires\(\s*["']([^/"']+)/""", texto)
    if not nomes:
        nomes = ["mixr"]
    alt = "|".join(re.escape(n) for n in sorted(set(nomes)))
    return re.compile(
        rf"(Package|Recipe).*'?({alt})/"
        rf"|Unable to find '?({alt})"
        rf"|({alt})/[\w.]+.*not (found|resolved)",
        re.I,
    )


# Achados que nao vem de uma falha, e sim de um FATO do ambiente ou do custo
# observado -- descobertos pelo retrato do ambiente e pelo log do Conan.
def achados_de_ambiente(ambiente, texto_completo):
    achados = []
    if ambiente.get("pkg-config", "AUSENTE") == "AUSENTE":
        achados.append("pacotes-de-sistema-ausentes")
    # O README nomeia Conan e Meson mas nao diz como instalar; no 24.04 o
    # caminho obvio (pip do sistema) e bloqueado pelo PEP 668. A imagem so
    # chegou a ter conan porque o Dockerfile contornou isso por conta propria.
    # A sonda do PEP 668 roda na construcao da imagem e o seu log e reproduzido
    # entre os marcadores SONDA_PIP_*. Ela mede o caminho ingenuo -- exatamente
    # o que um recem-chegado digita ao ler "Conan >= 2.0" sem instrucao nenhuma.
    sonda = re.search(r"##SONDA_PIP_INICIO(.*?)##SONDA_PIP_FIM", texto_completo, re.S)
    if sonda and re.search(r"externally-managed-environment|EXTERNALLY-MANAGED"
                           r"|rc=[1-9]", sonda.group(1), re.I):
        achados.append("ferramentas-sem-receita")
    # Recompilacao integral da arvore de dependencias.
    if re.search(r"Building from source|--build=missing.*\bboost\b|\bboost/.*: Building\b",
                 texto_completo, re.I):
        achados.append("sem-binarios-para-gcc13")
    return achados


# Um portao PULADO nao mede nada -- e "nao sei", nunca "esta resolvido". Sem
# este mapa, rodar sem credenciais faria o teste anunciar que o achado das
# credenciais foi corrigido, que e o oposto da verdade: ele so nao foi
# observado. Cada motivo de pulo diz quais achados ficam INDETERMINADOS.
INDETERMINADOS_POR_PULO = {
    "remote:nao-informado": ["conan-credenciais", "sem-binarios-para-gcc13"],
    "credenciais:modo-rapido": ["conan-credenciais", "sem-binarios-para-gcc13"],
    "credenciais:sem-credenciais": ["conan-credenciais", "sem-binarios-para-gcc13"],
}


def classifica(logs_por_portao):
    """Devolve (achados_ordenados, portao_que_parou)."""
    achados, parou = [], None
    assinaturas = [(nome, padrao or padrao_pacotes_privados())
                   for nome, padrao in ASSINATURAS]
    for portao, (rc, texto) in logs_por_portao.items():
        if rc == 0:
            continue
        if parou is None:
            parou = portao
        for nome, padrao in assinaturas:
            if padrao.search(texto) and nome not in achados:
                achados.append(nome)
    return achados, parou


# ------------------------------------------------------------------------------
# Execucao
# ------------------------------------------------------------------------------
def roda(cmd, **kw):
    print(f"  $ {' '.join(cmd)}", flush=True)
    return subprocess.run(cmd, **kw)


def monta_contexto(destino_dir):
    """Materializa o contexto de build: `git archive HEAD` + o harness local.

    As duas metades tem razoes diferentes:

    - o REPOSITORIO vem de `git archive HEAD` porque e o que um clone limpo
      entrega. Mandar a pasta crua enviaria build/, dist/ e contexts/src/ --
      gigabytes de artefato que o recem-chegado nao tem, e que fariam o teste
      mentir a favor do repositorio.

    - o HARNESS (esta pasta) vem da arvore de trabalho, por cima do que veio do
      HEAD. Sem isto o teste so rodaria depois de commitado -- nao daria para
      editar o Dockerfile e ver o efeito, que e justamente o ciclo de quem mexe
      nele. O harness nao e o objeto do teste; o README e.

    ARMADILHA (medida): passar o tar por stdin com '-f' nao funciona --
    o BuildKit recusa com "ambiguous Dockerfile source: both stdin and flag
    correspond to Dockerfiles". Materializar num diretorio evita a ambiguidade
    e funciona igual no builder classico e no BuildKit.
    """
    import io, shutil, tarfile

    bruto = subprocess.run(
        ["git", "archive", "HEAD"], cwd=RAIZ, stdout=subprocess.PIPE, check=True
    ).stdout
    with tarfile.open(fileobj=io.BytesIO(bruto)) as tar:
        tar.extractall(destino_dir)

    alvo = Path(destino_dir) / "tests" / "docker"
    alvo.mkdir(parents=True, exist_ok=True)
    for arquivo in sorted(AQUI.iterdir()):
        if arquivo.is_file():
            shutil.copy2(arquivo, alvo / arquivo.name)

    return sum(f.stat().st_size for f in Path(destino_dir).rglob("*") if f.is_file())


def constroi_imagem(tag, perfil, sem_cache):
    import tempfile

    print(f"[1/3] construindo imagem '{tag}' (perfil={perfil})")
    with tempfile.TemporaryDirectory(prefix="poc-mixr-docs-ctx-") as ctx:
        tamanho = monta_contexto(ctx)
        print(f"      contexto: {tamanho / 1048576:.1f} MB "
              f"(clone limpo de HEAD + harness da arvore de trabalho)")
        cmd = [
            "docker", "build",
            "-f", "tests/docker/Dockerfile.ubuntu-24.04",
            "--build-arg", f"PERFIL={perfil}",
            "-t", tag,
        ]
        if sem_cache:
            cmd.append("--no-cache")
        cmd.append(ctx)
        return subprocess.run(cmd).returncode


def roda_container(tag, modo, timeout_configure, timeout_build, cache_conan):
    print(f"[2/3] rodando os comandos do README no container (modo={modo})")
    cmd = ["docker", "run", "--rm",
           "-e", f"MODO={modo}",
           "-e", f"TIMEOUT_CONFIGURE={timeout_configure}",
           "-e", f"TIMEOUT_BUILD={timeout_build}"]

    # Onde fica o remote privado e quem pode entrar nele sao dados de QUEM RODA,
    # nao do teste: nenhum endereco, nome de remote ou credencial esta escrito
    # neste repositorio. Sem eles o portao correspondente e PULADO e o achado e
    # reportado assim mesmo -- nao poder adivinhar o endereco e, precisamente, o
    # achado.
    #
    # Credenciais NUNCA entram na imagem (ficariam gravadas numa camada) -- so
    # no ambiente do container, e so se ja estiverem no ambiente de quem chamou.
    for var in ("CONAN_REMOTE_NOME", "CONAN_REMOTE_URL",
                "CONAN_REMOTE_USUARIO", "CONAN_REMOTE_SENHA"):
        if os.environ.get(var):
            cmd += ["-e", var]

    # Cache do Conan como volume nomeado: sem isto, o modo completo recompila
    # a arvore inteira a CADA execucao (ver o achado 6). Com ele, so a primeira
    # paga o preco. O volume nao afeta o veredito -- os achados 1 a 5 acontecem
    # antes de qualquer download.
    if cache_conan:
        cmd += ["-v", f"{cache_conan}:/root/.conan2"]

    cmd.append(tag)

    saida = []
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    for linha in proc.stdout:
        sys.stdout.write(linha)
        saida.append(linha)
    proc.wait()
    return proc.returncode, "".join(saida)


def analisa(saida):
    """Extrai os marcadores '##' que gates.sh emitiu."""
    ambiente, logs, correcoes, pulados = {}, {}, [], []
    rcs, portao_atual, acumulando = {}, None, []
    em_ambiente = False

    for linha in saida.splitlines():
        crua = linha.rstrip("\n")
        if crua.startswith("##AMBIENTE_INICIO"):
            em_ambiente = True
            continue
        if crua.startswith("##AMBIENTE_FIM"):
            em_ambiente = False
            continue
        if em_ambiente and "=" in crua:
            k, _, v = crua.partition("=")
            ambiente[k.strip()] = v.strip()
            continue
        m = re.match(r"##GATE:([^:]+):rc=(-?\d+)", crua)
        if m:
            rcs[m.group(1)] = int(m.group(2))
            continue
        m = re.match(r"##GATE_LOG_INICIO:(.+)", crua)
        if m:
            portao_atual, acumulando = m.group(1), []
            continue
        if crua.startswith("##GATE_LOG_FIM:") and portao_atual:
            logs[portao_atual] = (rcs.get(portao_atual, 1), "\n".join(acumulando))
            portao_atual = None
            continue
        if portao_atual is not None:
            acumulando.append(crua)
            continue
        m = re.match(r"##CORRECAO:(.+)", crua)
        if m:
            correcoes.append(m.group(1))
            continue
        m = re.match(r"##PULADO:(.+)", crua)
        if m:
            pulados.append(m.group(1))

    return ambiente, logs, correcoes, pulados


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--modo", choices=["rapido", "completo"], default="rapido")
    p.add_argument("--tag", default="poc-mixr-docs-check:ubuntu-24.04")
    p.add_argument("--perfil", choices=["readme", "completo"], default="readme",
                   help="'readme' instala so o que a secao 2 lista; 'completo' "
                        "acrescenta os pacotes de sistema que os achados apontam "
                        "-- e assim que se PROVA que a lista de achados basta")
    p.add_argument("--sem-cache", action="store_true", help="docker build --no-cache")
    p.add_argument("--cache-conan", default=None,
                   help="volume docker para /root/.conan2 (acelera reexecucoes "
                        "do modo completo; nao afeta o veredito)")
    p.add_argument("--timeout-configure", type=int, default=3600)
    p.add_argument("--timeout-build", type=int, default=3600)
    p.add_argument("--remote-nome", default=None,
                   help="nome do remote Conan que hospeda as dependencias "
                        "privadas (ou a variavel CONAN_REMOTE_NOME). Sem ele o "
                        "portao do remote e pulado -- o teste nao inventa endereco")
    p.add_argument("--remote-url", default=None,
                   help="endereco desse remote (ou CONAN_REMOTE_URL)")
    p.add_argument("--exigir-suficiente", action="store_true",
                   help="verde SO com zero achados (a resposta absoluta), em vez "
                        "de comparar com a baseline")
    p.add_argument("--atualizar-baseline", action="store_true")
    args = p.parse_args()

    if args.remote_nome:
        os.environ["CONAN_REMOTE_NOME"] = args.remote_nome
    if args.remote_url:
        os.environ["CONAN_REMOTE_URL"] = args.remote_url

    if subprocess.run(["docker", "version"], capture_output=True).returncode != 0:
        print("ERRO: docker indisponivel (daemon parado, ou usuario fora do grupo 'docker').")
        return 2

    if constroi_imagem(args.tag, args.perfil, args.sem_cache) != 0:
        print("\nERRO: a imagem nao construiu. Isto ja e um resultado: o ambiente "
              "minimo descrito pelo README nao se monta sozinho no Ubuntu 24.04.")
        return 1

    _, saida = roda_container(args.tag, args.modo, args.timeout_configure,
                              args.timeout_build, args.cache_conan)

    ambiente, logs, correcoes, pulados = analisa(saida)
    achados, parou = classifica(logs)
    for extra in achados_de_ambiente(ambiente, saida):
        if extra not in achados:
            achados.append(extra)
    # As correcoes que gates.sh precisou aplicar sao achados por definicao.
    for c in correcoes:
        if c not in achados:
            achados.append(c)

    conhecidos = json.loads(BASELINE.read_text(encoding="utf-8")) if BASELINE.exists() else {}
    catalogo = conhecidos.get("achados", {})

    print("\n[3/3] " + "=" * 66)
    print("VEREDITO -- o README secao 2+3 basta num Ubuntu 24.04 limpo?")
    print("=" * 72)
    print("\nAmbiente do container:")
    for k, v in ambiente.items():
        print(f"  {k:12} {v}")

    print(f"\nPortoes (rc=0 e sucesso):")
    for portao, (rc, _) in logs.items():
        print(f"  {'OK  ' if rc == 0 else 'FALHA'} {portao} (rc={rc})")
    for pl in pulados:
        print(f"  PULADO {pl}")

    if not achados:
        print("\n  >> NENHUM achado: a documentacao BASTA neste ambiente.")
    else:
        print(f"\n  >> {len(achados)} ACHADO(S) -- a documentacao NAO basta:\n")
        for i, a in enumerate(achados, 1):
            info = catalogo.get(a, {})
            print(f"  {i}. [{a}]")
            print(f"     {info.get('resumo', '(achado novo -- ainda sem descricao na baseline)')}")
            if info.get("correcao"):
                print(f"     falta no README: {info['correcao']}")
            print()

    if parou:
        print(f"  primeiro portao a falhar: {parou}")

    if args.atualizar_baseline:
        conhecidos["observados"] = achados
        BASELINE.write_text(json.dumps(conhecidos, indent=2, ensure_ascii=False) + "\n",
                            encoding="utf-8")
        print(f"\n  baseline atualizada: {BASELINE}")
        return 0

    if args.exigir_suficiente:
        ok = not achados
        print("\n" + ("RESULTADO: documentacao suficiente." if ok else
                      "RESULTADO: documentacao insuficiente (--exigir-suficiente)."))
        return 0 if ok else 1

    esperados = conhecidos.get("observados", [])
    indeterminados = set()
    for pl in pulados:
        indeterminados.update(INDETERMINADOS_POR_PULO.get(pl, []))
    indeterminados -= set(achados)

    if indeterminados:
        print(f"  nao observados nesta execucao (portao pulado, veredito "
              f"INDETERMINADO): {', '.join(sorted(indeterminados))}")

    novos = [a for a in achados if a not in esperados]
    # Um achado da baseline que nao apareceu SO conta como resolvido se o portao
    # que o mediria de fato rodou.
    sumidos = [a for a in esperados if a not in achados and a not in indeterminados]
    if novos:
        print(f"\nRESULTADO: REGRESSAO -- achado(s) NOVO(S): {', '.join(novos)}")
        return 1
    if sumidos:
        print(f"\nRESULTADO: achado(s) RESOLVIDO(S): {', '.join(sumidos)}."
              f"\n  Atualize a baseline: --atualizar-baseline")
        return 1
    print("\nRESULTADO: conjunto de achados INALTERADO em relacao a baseline."
          "\n  (a documentacao segue insuficiente -- mas nada piorou)"
          if achados else "\nRESULTADO: sem achados.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
