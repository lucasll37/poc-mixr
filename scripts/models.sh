#!/usr/bin/env bash
#
# Gera um modelo novo em models/player/<nome>/ a partir de um ponto de
# partida copiavel (fixtures/stub, achatado, ou template/, em camadas).
#
# Automatiza a receita MECANICA ja documentada em models/README.md secao 2 e
# em models/player/template/docs/PRIMEIROS-PASSOS.md -- nao inventa passo
# novo, so elimina os erros manuais mais citados no repositorio: a linha ROOT
# do Makefile (calculada aqui pela PROFUNDIDADE REAL do destino, nunca
# copiada -- ver models/README.md secao 5, armadilha 7) e o namespace C++
# aninhado (models/player/fixtures/stub/docs/CONTRATO.md secao 6) esquecido pela
# metade.
#
# O QUE ESTE SCRIPT NAO FAZ, de proposito:
#   - nao escreve a logica de dominio (a razao do modelo existir);
#   - nao escreve o cenario que carrega o modelo (models/README.md secao 4 --
#     o .so entra sozinho em 'make models'/'make test' por descoberta via
#     find, mas so aparece num cenario rodavel depois de um '.edl.in' novo
#     apontar pra ele; nao ha catalogo pra registrar, so o arquivo);
#   - nao adiciona a linha em models/REGISTRO.md;
#   - nao faz commit nenhum.
# Tudo isso fica no checklist impresso ao final.
#
# Uso:
#   scripts/models.sh --name meu_modelo --kind stub
#   scripts/models.sh --name meu_modelo --kind template
#   scripts/models.sh --name meu_modelo --kind stub --dest algum/lugar --no-build
#
# Pre-requisito (uma vez por maquina, igual a qualquer modelo deste
# repositorio): 'make configure && make sdk' na raiz.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NAME=""
KIND="stub"
DEST=""
NO_BUILD=0

while [ $# -gt 0 ]; do
    case "$1" in
        --name) NAME="$2"; shift 2 ;;
        --kind) KIND="$2"; shift 2 ;;
        --dest) DEST="$2"; shift 2 ;;
        --no-build) NO_BUILD=1; shift ;;
        *) echo "argumento desconhecido: $1" >&2; exit 1 ;;
    esac
done

if [ -z "$NAME" ]; then
    echo "uso: scripts/models.sh --name meu_modelo --kind stub|template [--dest pasta] [--no-build]" >&2
    exit 1
fi

case "$KIND" in
    stub|template) : ;;
    *) echo "kind invalido: '$KIND' -- use 'stub' ou 'template'" >&2; exit 1 ;;
esac

# minusculas/digitos/underscore, comecando por letra -- mesma regra do gerador
# anterior (NOME_RE).
if ! [[ "$NAME" =~ ^[a-z][a-z0-9_]*$ ]]; then
    echo "nome invalido: '$NAME' -- use minusculas/digitos/underscore, comecando por letra" >&2
    exit 1
fi

if [ "$KIND" = "stub" ]; then
    ORIGEM="$REPO_ROOT/models/player/fixtures/stub"
else
    ORIGEM="$REPO_ROOT/models/player/template"
fi
ORIGEM_NOME="$(basename "$ORIGEM")"

if [ -n "$DEST" ]; then
    case "$DEST" in
        /*) DEST_ABS="$DEST" ;;
        *) DEST_ABS="$REPO_ROOT/$DEST" ;;
    esac
else
    DEST_ABS="$REPO_ROOT/models/player/$NAME"
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
# '*'/'?'/'[' -- kind e' sempre 'stub'/'template' e --name ja foi validado
# contra ^[a-z][a-z0-9_]*$. O 'cat arquivo; echo x' + '%x' preserva a quebra
# de linha final que 'command substitution' descartaria sozinha.
# ---------------------------------------------------------------------------
substituir() {
    local caminho="$1" de="$2" para="$3" obrigatorio="${4:-1}"
    local texto
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

# linha_root DEST -- 'ROOT := $(abspath ..N vezes..)' calculado da
# PROFUNDIDADE REAL do destino em relacao a raiz do repo -- elimina a
# armadilha mais citada de models/README.md (copiar o Makefile do stub, 4
# niveis por morar em fixtures/, para um destino de 3 niveis, e esquecer de
# tirar um '../').
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

  [ ] a regra de negocio de verdade (docs/PRIMEIROS-PASSOS.md, passo 5, se veio do
      template -- domain -> ubf -> xnative)
  [ ] preservar as chamadas ao xboard em ubf/*Action::execute() (a UNICA obrigacao que
      falha em silencio -- ver models/player/fixtures/stub/docs/CONTRATO.md secao 3)
  [ ] atualizar xnative/factory.cpp (NOMES[]/METAS[]) se classes forem renomeadas/removidas
  [ ] revisar a prosa de README.md/docs/*.md -- so o titulo foi trocado, o resto ainda
      descreve a origem (${nome} copiou de fixtures/stub ou template/)
  [ ] o bloco \`provides:\` do .edl do SEU cenario (tem que bater EXATAMENTE com o que o
      .so exporta)
  [ ] git add models/player/${nome}/ (este script nao commita nada)
  [ ] este modelo ja entra sozinho em 'make models'/'make test' da raiz (descoberta por
      'find' -- nao ha lista pra editar); falta so escrever um CENARIO pra ele: um
      '.edl.in' novo em src/poc/${nome}/configs/ (ja alcancavel por '-folder'/'-f', sem
      registrar em lugar nenhum) e, se fizer sentido, cobertura em tests/meson.build --
      ver models/README.md, secoes 4.1 e 4.2
  [ ] acrescentar sua linha em models/REGISTRO.md (nome, pasta, status, responsavel)
EOF
}

echo "copiando ${ORIGEM#"$REPO_ROOT"/} -> ${DEST_ABS#"$REPO_ROOT"/} ..."
# rsync (nao 'cp -r' + apagar depois) para nao copiar build//dist/ a toa --
# os dois projetos copiaveis (fixtures/stub e template/) sao autocontidos e
# costumam ter build/dist LOCAIS de terem sido compilados sozinhos.
if ! command -v rsync >/dev/null 2>&1; then
    echo "rsync nao encontrado -- necessario para copiar o scaffold" >&2
    exit 1
fi
mkdir -p "$DEST_ABS"
rsync -a \
    --exclude='/build' --exclude='/dist' --exclude='__pycache__' \
    --exclude='.configure-args' --exclude='*.o' --exclude='*.so' \
    "$ORIGEM/" "$DEST_ABS/"

# 1. meson.build -- mesma receita ja documentada em models/README.md secao 2
#    (substituicao sobre a string entre aspas simples, que cobre project() E
#    shared_module() num passo so).
MESON="$DEST_ABS/meson.build"
substituir "$MESON" "'$ORIGEM_NOME'" "'$NAME'"

if [ "$KIND" = "stub" ]; then
    # 2a. renomeia o arquivo fonte e a referencia files(...) em meson.build
    OLD_CPP="$DEST_ABS/src/$ORIGEM_NOME.cpp"
    NEW_CPP="$DEST_ABS/src/$NAME.cpp"
    mv "$OLD_CPP" "$NEW_CPP"
    substituir "$MESON" "files('src/$ORIGEM_NOME.cpp')" "files('src/$NAME.cpp')"
    PLUGIN_CPP="$NEW_CPP"
    ARQUIVOS_NS="$NEW_CPP"
else
    # 2b. template: namespace C++ em TODOS os arquivos de uma vez (a mesma
    #     receita de docs/PRIMEIROS-PASSOS.md passo 2, "grep -rl | xargs sed").
    PLUGIN_CPP="$DEST_ABS/src/plugin.cpp"
    ARQUIVOS_NS="$(arquivos_contendo "$DEST_ABS" "x$ORIGEM_NOME" include src tests)"
fi

# 3. namespace aninhado -- CONTRATO.md secao 6: 'xstub'/'xtemplate' -> 'x<nome>'
NS_VELHO="x$ORIGEM_NOME"
NS_NOVO="x$(printf '%s' "$NAME" | tr -cd 'a-z0-9')"
if [ -n "$ARQUIVOS_NS" ]; then
    while IFS= read -r f; do
        [ -n "$f" ] && substituir "$f" "$NS_VELHO" "$NS_NOVO"
    done <<< "$ARQUIVOS_NS"
fi

# 4. MIXR_PLUGIN_DEFINE -- o primeiro argumento e a string que o host usa
#    para identificar o plugin no descritor; tem que bater com o novo nome.
substituir "$PLUGIN_CPP" "MIXR_PLUGIN_DEFINE(\"$ORIGEM_NOME\"" "MIXR_PLUGIN_DEFINE(\"$NAME\""

# 5. ROOT do Makefile -- calculado, nunca copiado (ver linha_root()).
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

# 6a. lib<origem>.so -- literal em Makefile/README/docs (comentarios do alvo
#     'install', echo de sucesso, ldd de verificacao). Esta ultima e'
#     FUNCIONAL, nao so prosa: o Makefile copiado confere
#     'ldd .../lib{origem}.so' dentro do proprio alvo 'install' -- sem este
#     passo, 'make install' checaria o arquivo ERRADO.
while IFS= read -r -d '' f; do
    substituir "$f" "lib$ORIGEM_NOME.so" "lib$NAME.so" 0
done < <(find "$DEST_ABS" -type f \( -name 'Makefile' -o -name '*.md' \) -print0)

# 6b. CHANGELOG.md -- esvaziado e recomecado (docs/PRIMEIROS-PASSOS.md
#     passo 3), com a versao lida do PROPRIO meson.build copiado.
VERSAO="$(grep -oP "version:\s*'\K[^']+" "$MESON" | head -1)"
[ -z "$VERSAO" ] && VERSAO="0.1.0"
if [ "$KIND" = "template" ]; then
    ORIGIN_REL="$ORIGEM_NOME"
else
    ORIGIN_REL="fixtures/$ORIGEM_NOME"
fi
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

Gerado a partir de \`models/player/$ORIGIN_REL\` por \`scripts/models.sh\` (kind=$KIND).
Substitua esta entrada pela primeira decisão real deste modelo antes do primeiro commit.
EOF

# 7. README.md -- so o titulo (H1), a prosa fica para o passo manual
#    (models/README.md secao 2 / docs/PRIMEIROS-PASSOS.md passo 5).
substituir "$DEST_ABS/README.md" "\`$ORIGEM_NOME\`" "\`$NAME\`" 0

# 8. confere que nada do nome/namespace antigo sobrou (mesmo grep que o
#    passo 2 do PRIMEIROS-PASSOS.md sugere rodar a mao)
SOBRAS="$(arquivos_contendo "$DEST_ABS" "$NS_VELHO" include src tests)"
SOBRAS_MESON="$(arquivos_contendo "$DEST_ABS" "'$ORIGEM_NOME'" . | grep -F 'meson.build' || true)"
if [ -n "$SOBRAS" ] || [ -n "$SOBRAS_MESON" ]; then
    echo "  aviso: ainda ha ocorrencias do nome/namespace antigo em:"
    { [ -n "$SOBRAS" ] && printf '%s\n' "$SOBRAS"; [ -n "$SOBRAS_MESON" ] && printf '%s\n' "$SOBRAS_MESON"; } \
        | sort -u | while IFS= read -r f; do echo "    - ${f#"$REPO_ROOT"/}"; done
fi

if [ "$NO_BUILD" = "1" ]; then
    echo "scaffold de models/player/$NAME/ pronto (build de verificacao PULADO, --no-build)."
    imprimir_checklist "$NAME"
    exit 0
fi

# 9. build de fumaca REAL -- prova que o scaffold compila, testa E instala
#    (== popula ./dist/lib/mixr-plugins/) antes de devolver ao usuario.
#    'test: build' NAO chama 'meson install' (ver os dois Makefiles) -- por
#    isso os dois alvos, nao so 'test'. Requer 'make configure && make sdk'
#    ja rodado na raiz (mesmo pre-requisito de qualquer modelo deste
#    repositorio).
echo "compilando, testando e instalando o scaffold (make test install) ..."
if ! make -C "$DEST_ABS" test install; then
    echo "
FALHOU o build/teste de verificacao -- o scaffold ficou em models/player/$NAME/,
incompleto. NAO apague a pasta: compare com models/player/$([ "$KIND" = "stub" ] && echo fixtures/stub || echo template)/
para achar o que sobrou, ou confira se 'make configure && make sdk' ja rodou na raiz." >&2
    exit 1
fi

SO="$DEST_ABS/dist/lib/mixr-plugins/lib$NAME.so"
if [ ! -f "$SO" ]; then
    echo "  aviso: ${SO#"$REPO_ROOT"/} nao foi gerado -- confira o nome do shared_module() em meson.build" >&2
else
    FORTES="$(nm -D --defined-only "$SO" 2>/dev/null | grep -c ' T ')"
    if [ "$FORTES" != "1" ]; then
        echo "  aviso: esperava exatamente 1 simbolo T exportado, achei $FORTES" >&2
    fi
    LDD_OUT="$(ldd "$SO" 2>&1)"
    if printf '%s' "$LDD_OUT" | grep -q 'not found'; then
        echo "  aviso: ldd reporta dependencia nao resolvida:"$'\n'"$LDD_OUT" >&2
    fi
fi

echo ""
echo "scaffold de models/player/$NAME/ pronto e compilando/testando verde."
imprimir_checklist "$NAME"
