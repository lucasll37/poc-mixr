#!/usr/bin/env python3
"""
Compara o ESQUELETO (nomes de slot + nomes de fabrica + estrutura de
parenteses/chaves, SEM valores) de blocos EDL do tipo "( Aircraft ... )"
repetidos dentro do MESMO arquivo (falcon1..4, em
src/poc/{single-thread,multi-thread}/configs/scenario.edl.in).

POR QUE ISTO NAO E UMA COMPARACAO BYTE A BYTE: falcon1 tem ~200 linhas com
comentario explicando cada slot; falcon2/3/4 tem ~75 linhas cada, a MESMA
estrutura mas sem comentario e com varios campos condensados numa linha.
Alem disso, MUITOS valores numericos diferem de proposito entre os quatro
(posicao/altitude/rumo de cada aviao no circuito de patrulha,
minAltitude/recoverAltitude/patrolHeading/patrolAltitude/rtbAltitude --
calibrados por aviao, nao so id/posicao). Uma comparacao byte-a-byte nunca
bateria aqui, mesmo se os quatro blocos estiverem estruturalmente
IDENTICOS -- por isso a comparacao e so da FORMA (quais slots existem, em
que ordem, dentro de que fabrica), ignorando todo numero/string.

O parser EDL despacha cada 'chave: valor' por NOME (SlotTable::index() ->
setSlotByIndex(), ver contexts/MIXR-CONTEXT.md), nao por posicao -- entao
uma divergencia de ORDEM entre os blocos e inocua em runtime, mas ainda
assim um sinal de que a duplicacao comentada-vs-terse entre falcon1 e os
demais ja divergiu uma vez (foi o caso encontrado e corrigido em
src/poc/dis/multi-thread/configs/scenario.edl.in -- a ordem de
evadeSpeed/supportSpeed/evadeHold/terrainClearance no BtBehavior de
falcon1 nao batia com falcon2/3/4). Este guard existe para essa
divergencia nao voltar a acontecer em silencio.

Uso:
    python3 skeleton_diff.py <arquivo.edl.in> falcon1 falcon2 falcon3 falcon4

Devolve rc=0 se os esqueletos batem entre TODOS os blocos nomeados
(exceto o PRIMEIRO nome do bloco, que e o unico token que sempre difere
de proposito -- "falconN:" vira parte do esqueleto tambem, entao e
normalizado a parte). rc=1 e imprime o primeiro ponto de divergencia.
"""
import re
import sys


def extract_balanced_block(text: str, marker: str) -> str:
    """Acha 'marker' e devolve o conteudo ate o parenteses seguinte
    fechar, contando profundidade -- mesma tecnica ja usada no script de
    migracao citado na "nona passada" da secao "./app" do CLAUDE.md: ignora
    '//' e "..." ao contar profundidade, nao regex ingenuo.

    ARMADILHA CONFIRMADA (nao redescobrir): uma primeira versao contava
    parenteses char a char SEM pular comentario/string -- ao contrario do
    que o proprio docstring ja dizia fazer. Um unico parentese
    desbalanceado dentro de um comentario '//' (o tipo de coisa que se
    escreve sem pensar em prosa) fazia a extracao NUNCA fechar no lugar
    certo e engolir o bloco seguinte inteiro, comparando lixo com lixo.
    Reproduzido com um comentario de teste antes de corrigir.
    """
    start = text.index(marker)
    i = text.index("(", start)
    depth = 0
    j = i
    while j < len(text):
        if text.startswith("//", j):
            nl = text.find("\n", j)
            j = len(text) if nl < 0 else nl
            continue
        if text[j] == '"':
            j += 1
            while j < len(text) and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            j += 1
            continue
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                j += 1
                break
        j += 1
    return text[start:j]


TOKEN_RE = re.compile(
    r"""
    //[^\n]*                     |  # comentario de linha -- descartado
    "(?:[^"\\]|\\.)*"            |  # string entre aspas -- vira <STR>
    -?\d+\.?\d*(?:[eE][+-]?\d+)? |  # numero -- vira <NUM>
    [A-Za-z_][A-Za-z0-9_]*       |  # identificador -- mantido
    [(){}\[\]:]                     # pontuacao estrutural -- mantida
    """,
    re.VERBOSE,
)


def skeleton(block_text: str) -> list:
    """Tokeniza e reduz a forma: identificadores e pontuacao estrutural
    ficam, numero/string viram um placeholder generico, comentario some."""
    tokens = []
    for m in TOKEN_RE.finditer(block_text):
        tok = m.group(0)
        if tok.startswith("//"):
            continue
        if tok.startswith('"'):
            tokens.append("<STR>")
        elif re.fullmatch(r"-?\d+\.?\d*(?:[eE][+-]?\d+)?", tok):
            tokens.append("<NUM>")
        else:
            tokens.append(tok)
    return tokens


def compare_skeletons(path: str, block_names: list):
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()

    skeletons = {}
    for name in block_names:
        block = extract_balanced_block(text, f"{name}: (")
        toks = skeleton(block)
        # o proprio nome do bloco ("falcon1", "falcon2", ...) e o UNICO
        # identificador que sempre difere de proposito -- normaliza para
        # <SELF> antes de comparar, senao toda comparacao falharia so por
        # causa do nome.
        toks = ["<SELF>" if t == name else t for t in toks]
        skeletons[name] = toks

    reference_name = block_names[0]
    reference = skeletons[reference_name]
    ok = True
    for name in block_names[1:]:
        other = skeletons[name]
        if other != reference:
            ok = False
            print(f"DIVERGENCIA: esqueleto de '{name}' difere de '{reference_name}'")
            n = min(len(reference), len(other))
            for i in range(n):
                if reference[i] != other[i]:
                    lo, hi = max(0, i - 5), i + 6
                    print(f"  primeira diferenca no token {i}:")
                    print(f"    {reference_name}: ... {' '.join(reference[lo:hi])} ...")
                    print(f"    {name}: ... {' '.join(other[lo:hi])} ...")
                    break
            else:
                print(f"  um esqueleto e prefixo do outro (tamanhos {len(reference)} vs {len(other)})")
    return ok


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    file_path = sys.argv[1]
    names = sys.argv[2:]
    result = compare_skeletons(file_path, names)
    if result:
        print(f"OK: esqueleto de {names} identico em '{file_path}' (ignorando valores/comentarios)")
        sys.exit(0)
    else:
        sys.exit(1)
