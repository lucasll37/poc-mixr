#!/usr/bin/env bash
#
# Gera um modelo novo em models/players/<nome>/ a partir do unico ponto de
# partida copiavel: models/players/template/.
#
# Automatiza a receita MECANICA ja documentada em
# models/players/template/docs/PRIMEIROS-PASSOS.md -- nao inventa passo
# novo, so elimina os erros manuais mais citados no repositorio: a linha ROOT
# do Makefile (calculada aqui pela PROFUNDIDADE REAL do destino, nunca
# copiada) e o namespace C++ aninhado (models/players/template/docs/
# CONTRATO.md secao 6) esquecido pela metade -- e apaga o mirror de contrato
# (src/mirror.cpp + os blocos MIRROR-BLOCK-START/END em meson.build/tests/
# meson.build), que NAO faz parte do scaffold e colidiria em nome de fabrica
# com models/players/A-4 se um modelo novo continuasse exportando os mesmos
# 9 nomes por acidente.
#
# O QUE ESTE SCRIPT NAO FAZ, de proposito:
#   - nao escreve a logica de dominio (a razao do modelo existir);
#   - nao escreve o cenario que carrega o modelo (models/players/template/
#     docs/PRIMEIROS-PASSOS.md, passo 6 -- o .so entra sozinho em
#     'make models'/'make test' por descoberta via find, mas so aparece num
#     cenario rodavel depois de um '.edl.in' novo apontar pra ele; nao ha
#     catalogo pra registrar, so o arquivo);
#   - nao adiciona a linha em models/REGISTRO.md;
#   - nao faz commit nenhum.
# Tudo isso fica no checklist impresso ao final.
#
# Uso:
#   scripts/models.sh --name meu_modelo
#   scripts/models.sh --name meu_modelo --dest algum/lugar --no-build
#
# Pre-requisito (uma vez por maquina, igual a qualquer modelo deste
# repositorio): 'make configure && make sdk' na raiz.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NAME=""
DEST=""
NO_BUILD=0

while [ $# -gt 0 ]; do
    case "$1" in
        --name) NAME="$2"; shift 2 ;;
        --dest) DEST="$2"; shift 2 ;;
        --no-build) NO_BUILD=1; shift ;;
        *) echo "argumento desconhecido: $1" >&2; exit 1 ;;
    esac
done

if [ -z "$NAME" ]; then
    echo "uso: scripts/models.sh --name meu_modelo [--dest pasta] [--no-build]" >&2
    exit 1
fi

# minusculas/digitos/underscore, comecando por letra -- mesma regra do gerador
# anterior (NOME_RE).
if ! [[ "$NAME" =~ ^[a-z][a-z0-9_]*$ ]]; then
    echo "nome invalido: '$NAME' -- use minusculas/digitos/underscore, comecando por letra" >&2
    exit 1
fi

ORIGEM="$REPO_ROOT/models/players/template"
ORIGEM_NOME="$(basename "$ORIGEM")"

if [ -n "$DEST" ]; then
    case "$DEST" in
        /*) DEST_ABS="$DEST" ;;
        *) DEST_ABS="$REPO_ROOT/$DEST" ;;
    esac
else
    DEST_ABS="$REPO_ROOT/models/players/$NAME"
fi

case "$DEST_ABS" in
    "$REPO_ROOT"/*) : ;;
    *)
        echo "'$DEST_ABS' esta fora da raiz do repositorio -- o calculo de ROOT do" >&2
        echo "Makefile (linha_root()) exige um destino dentro do repositorio" >&2
        exit 1
        ;;
esac

if [ -e "$DEST_ABS" ]; then
    echo "'$DEST_ABS' ja existe -- escolha outro --name/--dest" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# substituir ARQUIVO DE PARA [obrigatorio=1]
#
# Substituicao LITERAL (nao regex): bash trata 'de'/'para' como padrao de
# glob em '${var//de/para}', mas nenhuma string usada por este script contem
# '*'/'?'/'[' -- --name ja foi validado contra ^[a-z][a-z0-9_]*$. O
# 'cat arquivo; echo x' + '%x' preserva a quebra de linha final que
# 'command substitution' descartaria sozinha.
# ---------------------------------------------------------------------------
substituir() {
    local caminho="$1" de="$2" para="$3" obrigatorio="${4:-1}"
    local texto
    if [ ! -f "$caminho" ]; then
        echo "erro fatal: '$caminho' nao existe (a copia do scaffold falhou antes deste ponto?)" >&2
        exit 1
    fi
    texto="$(cat "$caminho"; echo x)"
    texto="${texto%x}"
    if [ "$obrigatorio" = "1" ] && [[ "$texto" != *"$de"* ]]; then
        echo "  aviso: '$de' nao encontrado em ${caminho#"$REPO_ROOT"/}" >&2
    fi
    texto="${texto//$de/$para}"
    printf '%s' "$texto" > "$caminho"
}

# arquivos_contendo RAIZ TOKEN SUBPASTA...
# Mesmo papel do arquivos_contendo() do gerador anterior: lista (uma por
# linha) os arquivos, sob as subpastas dadas, que contem TOKEN.
arquivos_contendo() {
    local raiz="$1" token="$2"
    shift 2
    local sub
    for sub in "$@"; do
        [ -d "$raiz/$sub" ] && grep -rlF -- "$token" "$raiz/$sub" 2>/dev/null
    done
}

# apagar_bloco_mirror ARQUIVO -- remove, inclusive, tudo entre
# '# >>> MIRROR-BLOCK-START' e '# <<< MIRROR-BLOCK-END' (ver
# models/players/template/meson.build e tests/meson.build). Nao-fatal se o
# marcador nao existir (o arquivo pode nao ter bloco de mirror nenhum).
apagar_bloco_mirror() {
    local caminho="$1"
    [ -f "$caminho" ] || return 0
    grep -q 'MIRROR-BLOCK-START' "$caminho" || return 0
    awk '
        /# >>> MIRROR-BLOCK-START/ { pulando = 1; next }
        /# <<< MIRROR-BLOCK-END/   { pulando = 0; next }
        !pulando { print }
    ' "$caminho" > "$caminho.tmp" && mv "$caminho.tmp" "$caminho"
}

# linha_root DEST -- 'ROOT := $(abspath ..N vezes..)' calculado da
# PROFUNDIDADE REAL do destino em relacao a raiz do repo -- elimina a
# armadilha mais citada de models/players/template/docs/PRIMEIROS-PASSOS.md
# (copiar o Makefile do template, 3 niveis, para um destino de outra
# profundidade, e esquecer de ajustar os '../').
linha_root() {
    local rel dots i partes
    rel="${1#"$REPO_ROOT"/}"
    IFS='/' read -ra partes <<< "$rel"
    dots=""
    for ((i = 0; i < ${#partes[@]}; i++)); do
        dots="${dots}../"
    done
    dots="${dots%/}"
    echo "\$(abspath $dots)"
}

imprimir_checklist() {
    local nome="$1"
    cat <<EOF

Falta, MANUALMENTE (nada disto e automatizavel):

  [ ] a regra de negocio de verdade (docs/PRIMEIROS-PASSOS.md, passo 5 --
      domain -> ubf/State -> ubf/Behavior -> ubf/Action -> xnative/factory)
  [ ] preservar as chamadas ao xboard em ubf/*Action::execute() (a UNICA obrigacao que
      falha em silencio -- ver models/players/template/docs/CONTRATO.md secao 3)
  [ ] atualizar xnative/factory.cpp (NOMES[]/METAS[]) se classes forem renomeadas/removidas
  [ ] revisar a prosa de README.md/docs/*.md -- so o titulo foi trocado, o resto ainda
      descreve a origem (${nome} copiou de template/)
  [ ] o bloco \`provides:\` do .edl do SEU cenario (tem que bater EXATAMENTE com o que o
      .so exporta)
  [ ] git add models/players/${nome}/ (este script nao commita nada)
  [ ] este modelo ja entra sozinho em 'make models'/'make test' da raiz (descoberta por
      'find' -- nao ha lista pra editar); falta so escrever um CENARIO pra ele: um
      '.edl.in' novo em src/poc/${nome}/configs/ (ja alcancavel por '-folder'/'-f', sem
      registrar em lugar nenhum) e, se fizer sentido, cobertura em tests/meson.build --
      ver CONTRIBUTING.md, secoes 5.2 e 5.3
  [ ] acrescentar sua linha em models/REGISTRO.md (nome, pasta, status, responsavel)
EOF
}

echo "copiando ${ORIGEM#"$REPO_ROOT"/} -> ${DEST_ABS#"$REPO_ROOT"/} ..."
# rsync (nao 'cp -r' + apagar depois) para nao copiar build//dist/ a toa --
# o template e autocontido e costuma ter build/dist LOCAIS de ter sido
# compilado sozinho.
if ! command -v rsync >/dev/null 2>&1; then
    echo "rsync nao encontrado -- necessario para copiar o scaffold" >&2
    exit 1
fi
if ! mkdir -p "$DEST_ABS"; then
    echo "erro fatal: nao consegui criar '$DEST_ABS' (permissao? disco cheio?)" >&2
    exit 1
fi
if ! rsync -a \
    --exclude='/build' --exclude='/dist' --exclude='__pycache__' \
    --exclude='.configure-args' --exclude='*.o' --exclude='*.so' \
    "$ORIGEM/" "$DEST_ABS/"; then
    echo "erro fatal: rsync de '$ORIGEM' para '$DEST_ABS' falhou (disco cheio? permissao?)." >&2
    echo "'$DEST_ABS' pode ter ficado com uma copia PARCIAL -- confira 'df -h' antes de tentar" >&2
    echo "de novo (nao apague sem olhar, pode ter algo aproveitavel)." >&2
    exit 1
fi

# 1. O mirror de contrato (src/mirror.cpp + os blocos template_mirror_lib/
#    contrato-...-mirror) NAO faz parte do scaffold -- ver o aviso no topo
#    do proprio arquivo. Apaga ANTES de qualquer substituicao de nome, para
#    as etapas seguintes (namespace, MIXR_PLUGIN_DEFINE, verificacoes de
#    sobra) nunca verem esse arquivo.
rm -f "$DEST_ABS/src/mirror.cpp"
apagar_bloco_mirror "$DEST_ABS/meson.build"
apagar_bloco_mirror "$DEST_ABS/tests/meson.build"

# 2. meson.build -- mesma receita ja documentada em docs/PRIMEIROS-PASSOS.md
#    (substituicao sobre a string entre aspas simples, que cobre project() E
#    shared_module() num passo so).
MESON="$DEST_ABS/meson.build"
substituir "$MESON" "'$ORIGEM_NOME'" "'$NAME'"

# 3. namespace C++ em TODOS os arquivos de uma vez (a mesma receita de
#    docs/PRIMEIROS-PASSOS.md passo 2, "grep -rl | xargs sed"). Roda DEPOIS
#    do passo 1 (mirror.cpp ja apagado), entao 'xtemplate_mirror' nunca
#    entra nesta lista.
ARQUIVOS_NS="$(arquivos_contendo "$DEST_ABS" "x$ORIGEM_NOME" include src tests)"

# 4. namespace aninhado -- CONTRATO.md secao 6: 'xtemplate' -> 'x<nome>'
NS_VELHO="x$ORIGEM_NOME"
NS_NOVO="x$(printf '%s' "$NAME" | tr -cd 'a-z0-9')"
if [ -n "$ARQUIVOS_NS" ]; then
    while IFS= read -r f; do
        [ -n "$f" ] && substituir "$f" "$NS_VELHO" "$NS_NOVO"
    done <<< "$ARQUIVOS_NS"
fi

# 5. MIXR_PLUGIN_DEFINE -- o primeiro argumento e a string que o host usa
#    para identificar o plugin no descritor; tem que bater com o novo nome.
PLUGIN_CPP="$DEST_ABS/src/plugin.cpp"
substituir "$PLUGIN_CPP" "MIXR_PLUGIN_DEFINE(\"$ORIGEM_NOME\"" "MIXR_PLUGIN_DEFINE(\"$NAME\""

# 6. ROOT do Makefile -- calculado, nunca copiado (ver linha_root()).
MAKEFILE="$DEST_ABS/Makefile"
NOVA_ROOT="$(linha_root "$DEST_ABS")"
if grep -q '^ROOT[[:space:]]*:=' "$MAKEFILE"; then
    # sed aqui e' seguro: NOVA_ROOT so contem '$(abspath .../.../..)', sem
    # caractere de delimitador ('#') nem '&' -- os dois unicos especiais do
    # lado da substituicao.
    sed -i "s#^ROOT[[:space:]]*:=.*#ROOT      := ${NOVA_ROOT}#" "$MAKEFILE"
else
    echo "  aviso: linha 'ROOT :=' nao encontrada em ${MAKEFILE#"$REPO_ROOT"/}" >&2
fi

# 7a. lib<origem>.so -- literal em Makefile/README/docs (comentarios do alvo
#     'install', echo de sucesso, ldd de verificacao). Esta ultima e'
#     FUNCIONAL, nao so prosa: o Makefile copiado confere
#     'ldd .../lib{origem}.so' dentro do proprio alvo 'install' -- sem este
#     passo, 'make install' checaria o arquivo ERRADO. Tambem cobre a
#     mencao residual a 'libtemplate_mirror.so' que sobrar em README/docs
#     apos o passo 1 apagar o artefato em si.
while IFS= read -r -d '' f; do
    substituir "$f" "lib${ORIGEM_NOME}_mirror.so" "" 0
    substituir "$f" "lib$ORIGEM_NOME.so" "lib$NAME.so" 0
done < <(find "$DEST_ABS" -type f \( -name 'Makefile' -o -name '*.md' \) -print0)

# 7b. CHANGELOG.md -- esvaziado e recomecado (docs/PRIMEIROS-PASSOS.md
#     passo 3), com a versao lida do PROPRIO meson.build copiado.
VERSAO="$(grep -oP "version:\s*'\K[^']+" "$MESON" | head -1)"
[ -z "$VERSAO" ] && VERSAO="0.1.0"
DATA_HOJE="$(date +%Y-%m-%d)"
cat > "$DEST_ABS/CHANGELOG.md" <<EOF
# Changelog — \`$NAME\`

Todo projeto de modelo deste repositório tem \`tests/\`, \`docs/\`, \`README.md\` e **este arquivo** —
a regra, e o porquê dela, estão em [\`../../README.md\`](../../README.md); a guarda
[\`tests/guard/check_modelo_estrutura.sh\`](../../../tests/guard/check_modelo_estrutura.sh) a
trava (ela descobre projetos por \`find\`, então este diretório já nasce coberto).

Formato adaptado de [Keep a Changelog](https://keepachangelog.com/pt-br/1.1.0/).

**A versão é a do \`project()\` em [\`meson.build\`](meson.build)** — hoje \`$VERSAO\`. Não existe
outra: não há tag de git, e o descritor do plugin não carrega versão do modelo (\`PluginDescV1\` tem
\`plugin_name\`, \`mixr_pkg_version\` e \`build_id\`, e nada mais — ver
[\`../../../libs/xplugin/PluginAbi.hpp\`](../../../libs/xplugin/PluginAbi.hpp)).

**As datas saem da data de COMMIT, nunca da mensagem** — ver
[\`CONTRIBUTING.md\`](../../../CONTRIBUTING.md), na raiz do repositório, para a convenção de
mensagem de commit em uso.

---

## [$VERSAO] — $DATA_HOJE

Gerado a partir de \`models/players/$ORIGEM_NOME\` por \`scripts/models.sh\`.
Substitua esta entrada pela primeira decisão real deste modelo antes do primeiro commit.
EOF

# 8. README.md -- so o titulo (H1), a prosa fica para o passo manual
#    (docs/PRIMEIROS-PASSOS.md passo 5).
substituir "$DEST_ABS/README.md" "\`$ORIGEM_NOME\`" "\`$NAME\`" 0

# 9. confere que nada do nome/namespace antigo sobrou (mesmo grep que o
#    passo 2 do PRIMEIROS-PASSOS.md sugere rodar a mao)
SOBRAS="$(arquivos_contendo "$DEST_ABS" "$NS_VELHO" include src tests)"
SOBRAS_MESON="$(arquivos_contendo "$DEST_ABS" "'$ORIGEM_NOME'" . | grep -F 'meson.build' || true)"
if [ -n "$SOBRAS" ] || [ -n "$SOBRAS_MESON" ]; then
    echo "  aviso: ainda ha ocorrencias do nome/namespace antigo em:"
    { [ -n "$SOBRAS" ] && printf '%s\n' "$SOBRAS"; [ -n "$SOBRAS_MESON" ] && printf '%s\n' "$SOBRAS_MESON"; } \
        | sort -u | while IFS= read -r f; do echo "    - ${f#"$REPO_ROOT"/}"; done
fi

if [ "$NO_BUILD" = "1" ]; then
    echo "scaffold de models/players/$NAME/ pronto (build de verificacao PULADO, --no-build)."
    imprimir_checklist "$NAME"
    exit 0
fi

# 10. build de fumaca REAL -- prova que o scaffold compila, testa E instala
#     (== popula ./dist/lib/mixr-plugins/) antes de devolver ao usuario.
#     'test: build' NAO chama 'meson install' (ver o Makefile) -- por isso
#     os dois alvos, nao so 'test'. Requer 'make configure && make sdk' ja
#     rodado na raiz (mesmo pre-requisito de qualquer modelo deste
#     repositorio).
echo "compilando, testando e instalando o scaffold (make test install) ..."
if ! make -C "$DEST_ABS" test install; then
    echo "
FALHOU o build/teste de verificacao -- o scaffold ficou em models/players/$NAME/,
incompleto. NAO apague a pasta: compare com models/players/template/ para achar o que
sobrou, ou confira se 'make configure && make sdk' ja rodou na raiz." >&2
    exit 1
fi

#   CORRIGIDO (nao redescobrir): as tres checagens abaixo so' imprimiam
# "aviso: ..." e seguiam em frente -- nenhuma setava codigo de saida
# diferente de zero. Um scaffold com o .so VAZANDO simbolos (quebrando o
# isolamento RTLD_LOCAL entre plugins, ver a "armadilha 3" do SDK de plugin
# no CLAUDE.md raiz) ou com dependencia de linkedicao quebrada saia
# reportado como "pronto e verde", igual a um scaffold perfeito.
QUEBRADO=0
SO="$DEST_ABS/dist/lib/mixr-plugins/lib$NAME.so"
if [ ! -f "$SO" ]; then
    echo "  FALHA: ${SO#"$REPO_ROOT"/} nao foi gerado -- confira o nome do shared_module() em meson.build" >&2
    QUEBRADO=1
else
    FORTES="$(nm -D --defined-only "$SO" 2>/dev/null | grep -c ' T ')"
    if [ "$FORTES" != "1" ]; then
        echo "  FALHA: esperava exatamente 1 simbolo T exportado, achei $FORTES" >&2
        QUEBRADO=1
    fi
    LDD_OUT="$(ldd "$SO" 2>&1)"
    if printf '%s' "$LDD_OUT" | grep -q 'not found'; then
        echo "  FALHA: ldd reporta dependencia nao resolvida:"$'\n'"$LDD_OUT" >&2
        QUEBRADO=1
    fi
fi

if [ "$QUEBRADO" -ne 0 ]; then
    echo "
scaffold de models/players/$NAME/ compilou e passou nos testes, mas FALHOU nas
checagens de contrato do plugin acima -- corrija antes de considerar pronto." >&2
    exit 1
fi

echo ""
echo "scaffold de models/players/$NAME/ pronto e compilando/testando verde."
imprimir_checklist "$NAME"
