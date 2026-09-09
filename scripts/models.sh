#!/usr/bin/env bash
#
# Gera um modelo novo em models/<categoria>/<nome>/ a partir do unico ponto
# de partida copiavel: models/players/template/.
#
# A CATEGORIA e obrigatoria (--category player|system|others) e decide a
# subpasta de destino sob models/ -- mesma taxonomia que 'MODELOS_PRODUCAO'
# do Makefile raiz ja descobre por 'find' (ver CLAUDE.md, secao "O MODELO e
# um plugin"): 'player' -> models/players/, 'system' -> models/systems/,
# 'others' -> models/others/. NAO existe categoria 'event': models/events/
# nao e uma pasta de projetos-modelo, um por evento -- e UM projeto Meson so
# (a lib 'events'), e um evento novo vira uma pasta payloads/<TOKEN>/ DENTRO
# dele, nao um scaffold de template/ novo (ver models/events/README.md);
# forcar esse fluxo por aqui produziria uma estrutura que nao bate com nada
# documentado. Continua fora do escopo deste gerador.
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
# Uso -- CRIAR:
#   scripts/models.sh --name meu_modelo --category player
#   scripts/models.sh --name F-5 --category player
#   scripts/models.sh --name meu_modelo --category system --dest algum/lugar --no-build
#
# Uso -- REMOVER (ver o bloco "MODO --remove" logo abaixo do parsing):
#   scripts/models.sh --remove --name meu_modelo --category player
#   scripts/models.sh --remove --so libx9.so [--data x9]      # orfao legado, sem fonte
#
# Pre-requisito (uma vez por maquina, igual a qualquer modelo deste
# repositorio): 'make configure && make sdk' na raiz.

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

NAME=""
CATEGORIA=""
DEST=""
NO_BUILD=0
REMOVER=0
FORCE=0
DRY_RUN=0
SO_EXPLICITOS=()
DATA_EXPLICITOS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --name) NAME="$2"; shift 2 ;;
        --category) CATEGORIA="$2"; shift 2 ;;
        --dest) DEST="$2"; shift 2 ;;
        --no-build) NO_BUILD=1; shift ;;
        --remove) REMOVER=1; shift ;;
        --so) SO_EXPLICITOS+=("$2"); shift 2 ;;
        --data) DATA_EXPLICITOS+=("$2"); shift 2 ;;
        --force) FORCE=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        *) echo "argumento desconhecido: $1" >&2; exit 1 ;;
    esac
done

USO="uso: scripts/models.sh --name meu_modelo --category player|system|others [--dest pasta] [--no-build]"
USO_REMOVE="uso: scripts/models.sh --remove --name <nome> --category player|system|others [--force] [--dry-run]
     ou: scripts/models.sh --remove --so lib<X>.so [--so ...] [--data <dir>] [--force] [--dry-run]"

# --no-build e' exclusiva da CRIACAO (pula o build de fumaca). Aceitar junto
# com --remove seria um no-op silencioso, e silencio e' exatamente o modo de
# falha que este script inteiro existe para nao ter -- ver a secao "MODO
# --remove" abaixo.
if [ "$REMOVER" = "1" ] && [ "$NO_BUILD" = "1" ]; then
    echo "--no-build nao faz sentido com --remove (ela so' pula o build de fumaca da criacao)" >&2
    exit 1
fi

if [ "$REMOVER" = "0" ] && { [ ${#SO_EXPLICITOS[@]} -gt 0 ] || [ ${#DATA_EXPLICITOS[@]} -gt 0 ] \
                             || [ "$FORCE" = "1" ] || [ "$DRY_RUN" = "1" ]; }; then
    echo "--so/--data/--force/--dry-run so' valem com --remove" >&2
    echo "$USO" >&2
    exit 1
fi

if [ "$REMOVER" = "0" ] && [ -z "$NAME" ]; then
    echo "$USO" >&2
    exit 1
fi

if [ "$REMOVER" = "0" ] && [ -z "$CATEGORIA" ]; then
    echo "$USO" >&2
    echo "  --category e obrigatorio -- decide em qual subpasta de models/ o scaffold entra" >&2
    exit 1
fi

# ===========================================================================
# MODO --remove -- remover um modelo sem deixar inconsistencia
#
# O PROBLEMA que este modo resolve, e por que a ORDEM importa mais que o
# comando: a informacao de propriedade de um modelo (quais .so e quais
# diretorios de dados ele publicou em plugins/) mora DENTRO da pasta dele --
# 'uninstall-host' itera o ./dist LOCAL do proprio modelo. Apagar a pasta
# primeiro destroi a unica fonte, e ela NAO e recuperavel depois: o
# meson.build exige a pasta; nao ha manifesto; 'plugininfo' nem devolve
# plugin_name (a saida e' so {"classes":[...]}); e a convencao de nome falha
# no caso vivo -- a pasta 'A-4' publica libflight.so em plugins/data/flight.
#
# Dai a ordem abaixo, escolhida para que TODO PREFIXO seja um estado
# consistente (um Ctrl+C no meio e' retomavel e nunca destroi a fonte):
#   1. descobrir   2. recusar se algum cenario referencia   3. uninstall-host
#   4. podar dist/   5. apagar a pasta (irreversivel, por ultimo)   6. checklist
#
# O QUE ESTE MODO NAO FAZ, de proposito:
#   - nao edita cenario nenhum (ele RECUSA e lista; corrigir e' decisao sua);
#   - nao mexe em models/REGISTRO.md, tests/meson.build nem no Makefile raiz;
#   - nao regenera os catalogos commitados do editor EDL/docs;
#   - nao faz commit nenhum.
# Tudo isso fica no checklist impresso ao final.
# ===========================================================================
if [ "$REMOVER" = "1" ]; then

    # -----------------------------------------------------------------------
    # rm_arquivo CAMINHO / rm_dir CAMINHO -- remocao com respeito a --dry-run
    # e com uma linha por acao (este script nunca apaga em silencio).
    # -----------------------------------------------------------------------
    rm_arquivo() {
        local alvo="$1"
        [ -e "$alvo" ] || return 0
        if [ "$DRY_RUN" = "1" ]; then
            echo "  [dry-run] removeria ${alvo#"$REPO_ROOT"/}"
        else
            rm -f "$alvo" && echo "  removido ${alvo#"$REPO_ROOT"/}"
        fi
    }
    rm_dir() {
        local alvo="$1"
        [ -d "$alvo" ] || return 0
        if [ "$DRY_RUN" = "1" ]; then
            echo "  [dry-run] removeria ${alvo#"$REPO_ROOT"/}/"
        else
            rm -rf "$alvo" && echo "  removido ${alvo#"$REPO_ROOT"/}/"
        fi
    }

    # -----------------------------------------------------------------------
    # artefatos_publicados DIR -- basenames dos .so que ESTE modelo publica.
    #
    # Fonte primaria: o ./dist local (a mesma que 'uninstall-host' usa). O
    # FALLBACK pelo meson.build existe para o segundo caminho de orfao, que
    # e' o mais dificil de perceber: sem ./dist local (modelo nunca
    # construido, ou 'make -C <dir> clean' ja rodado) o laco de
    # 'uninstall-host' itera VAZIO e nao remove nada, EM SILENCIO.
    # -----------------------------------------------------------------------
    artefatos_publicados() {
        local dir="$1" achou=0 so
        if [ -d "$dir/dist/lib/mixr-plugins" ]; then
            for so in "$dir"/dist/lib/mixr-plugins/*.so; do
                [ -e "$so" ] || continue
                basename "$so"
                achou=1
            done
        fi
        [ "$achou" = "1" ] && return 0
        [ -f "$dir/meson.build" ] || return 0
        sed -n "s/.*shared_module([\"']\([^\"']*\)[\"'].*/\1/p" "$dir/meson.build" \
          | while IFS= read -r alvo; do
                [ -n "$alvo" ] && echo "lib${alvo}.so"
            done
    }

    # -----------------------------------------------------------------------
    # datadirs_publicados DIR -- nomes das subpastas de plugins/data/ que ESTE
    # modelo publica. Mesmo par fonte-primaria/fallback de
    # artefatos_publicados(); o fallback le o terceiro segmento de
    # "get_option('datadir') / 'mixr-plugins' / '<nome>'".
    #
    # NAO da para derivar do nome da PASTA: models/players/A-4 publica em
    # plugins/data/flight.
    # -----------------------------------------------------------------------
    datadirs_publicados() {
        local dir="$1" achou=0 d
        if [ -d "$dir/dist/share/mixr-plugins" ]; then
            for d in "$dir"/dist/share/mixr-plugins/*/; do
                [ -d "$d" ] || continue
                basename "$d"
                achou=1
            done
        fi
        [ "$achou" = "1" ] && return 0
        [ -f "$dir/meson.build" ] || return 0
        sed -n "s|.*'mixr-plugins'[[:space:]]*/[[:space:]]*'\([^']*\)'.*|\1|p" "$dir/meson.build" | sort -u
    }

    # -----------------------------------------------------------------------
    # referenciadores PADRAO_ERE -- cenarios de FONTE que casam o padrao, um
    # por linha.
    #
    # DELEGA para tests/guard/check_cenario_plugin.sh --refs, de proposito:
    # aquela guarda ja precisa saber exatamente o que conta como "cenario de
    # fonte" (quais diretorios podar, e que *.generated.edl fica de fora), e
    # duas copias dessa regra divergiriam -- uma recusaria a remocao por um
    # arquivo que a outra nem olha. Uma implementacao, dois consumidores.
    #
    # ACHADO RODANDO, nao redescobrir: sem podar '.gitlab-ci-local' a
    # varredura devolvia CADA cenario DUAS vezes -- uma na fonte e uma em
    # .gitlab-ci-local/builds/.docker/..., a copia que 'make test-ci' deixa do
    # repositorio inteiro. A poda vive na guarda, nao aqui.
    # -----------------------------------------------------------------------
    referenciadores() {
        local padrao="$1"
        local guarda="$REPO_ROOT/tests/guard/check_cenario_plugin.sh"
        if [ ! -x "$guarda" ]; then
            echo "erro fatal: $guarda nao existe ou nao e executavel" >&2
            echo "  (e' ele quem sabe quais .edl sao FONTE -- sem ele a recusa" >&2
            echo "   por cenario referenciado nao tem como ser confiavel)" >&2
            exit 1
        fi
        "$guarda" --refs "$padrao"
    }

    # -----------------------------------------------------------------------
    # escapar_ere TEXTO -- escapa '.' para o texto virar literal numa ERE
    # (todo basename aqui tem '.so', e um '.' solto casaria qualquer letra).
    # -----------------------------------------------------------------------
    escapar_ere() { printf '%s' "${1//./\\.}"; }

    imprimir_checklist_remocao() {
        local alvo="$1"
        cat <<EOF

Falta, MANUALMENTE (nada disto e automatizavel com seguranca):

  [ ] models/REGISTRO.md -- remover a linha de ${alvo} (tabela manual, sem enforcement)
  [ ] tests/meson.build -- entradas que citam o nome do modelo
  [ ] Makefile da raiz -- alvos com o nome cravado (test, test-asan; 'test-models'
      e 'models' NAO precisam -- descobrem por find, ver MODELOS_PRODUCAO)
  [ ] .gitlab-ci.yml e .gitignore -- entradas por nome, se houver
  [ ] regenerar os catalogos COMMITADOS que carregam o rotulo 'plugin:<nome>':
      src/ui/edl_catalog.generated.json, src/ui/edl-builder.html e
      docs/manual/catalog.generated.js  ('make docs' cobre o ultimo; o do editor
      EDL e' 'make open-edl-builder', que HOJE FALHA -- chama src/ui/scripts/build.js,
      arquivo que nao existe)
  [ ] apagar *.generated.edl velhos (alguns ainda nomeiam .so que nao existem mais)
  [ ] CI: o cache por branch guarda plugins/ e dist/ -- limpar pela UI do GitLab,
      nao ha alvo make que alcance o cache remoto
  [ ] git rm -r <pasta> (este script nao commita nada)
EOF
    }

    # --- 1. resolver o ALVO: modo por nome ou modo por artefato -------------
    if [ ${#SO_EXPLICITOS[@]} -gt 0 ] && [ -n "$NAME" ]; then
        echo "--so e --name sao modos DIFERENTES e nao se combinam:" >&2
        echo "  --name  remove um modelo que ainda tem pasta (artefatos descobertos dela)" >&2
        echo "  --so    remove um artefato orfao, sem fonte -- nunca toca pasta nenhuma" >&2
        exit 1
    fi

    SOS=()
    DATAS=()
    DEST_ABS=""
    ROTULO=""

    if [ ${#SO_EXPLICITOS[@]} -gt 0 ]; then
        # ---- modo por ARTEFATO (orfao legado, sem fonte) -------------------
        SOS=("${SO_EXPLICITOS[@]}")
        [ ${#DATA_EXPLICITOS[@]} -gt 0 ] && DATAS=("${DATA_EXPLICITOS[@]}")
        ROTULO="${SOS[*]}"
    else
        # ---- modo por NOME (a pasta tem de existir) ------------------------
        if [ -z "$NAME" ] || { [ -z "$CATEGORIA" ] && [ -z "$DEST" ]; }; then
            echo "$USO_REMOVE" >&2
            exit 1
        fi
        if ! [[ "$NAME" =~ ^[A-Za-z][A-Za-z0-9_-]*$ ]]; then
            echo "nome invalido: '$NAME'" >&2
            exit 1
        fi
        if [ -n "$DEST" ]; then
            case "$DEST" in
                /*) DEST_ABS="$DEST" ;;
                *)  DEST_ABS="$REPO_ROOT/$DEST" ;;
            esac
        else
            case "$CATEGORIA" in
                player) DEST_ABS="$REPO_ROOT/models/players/$NAME" ;;
                system) DEST_ABS="$REPO_ROOT/models/systems/$NAME" ;;
                others) DEST_ABS="$REPO_ROOT/models/others/$NAME" ;;
                *) echo "categoria invalida: '$CATEGORIA' -- use player, system ou others" >&2; exit 1 ;;
            esac
        fi

        case "$DEST_ABS" in
            "$REPO_ROOT"/models/*) : ;;
            *) echo "'$DEST_ABS' nao esta sob models/ -- recusado" >&2; exit 1 ;;
        esac

        # template/ nunca e removivel: publica libtemplate_mirror.so, que os
        # testes de plugin do HOST carregam (plugin-modelo-estranho e
        # plugin-deposito-terceiro trocam so' o 'file:' do cenario de
        # producao por ele). Remover isto quebraria a suite sem nenhum aviso
        # que aponte para ca.
        case "$DEST_ABS" in
            */models/players/template)
                echo "models/players/template nao e removivel: o segundo artefato dele" >&2
                echo "  (libtemplate_mirror.so) e' o mirror de contrato que os testes de plugin" >&2
                echo "  do host usam -- ver tests/meson.build, plugin-modelo-estranho." >&2
                exit 1
                ;;
        esac

        if [ ! -d "$DEST_ABS" ]; then
            echo "'${DEST_ABS#"$REPO_ROOT"/}' nao existe." >&2
            echo "" >&2
            echo "  Se voce ja apagou a pasta a mao, a informacao de propriedade foi junto:" >&2
            echo "  o nome do artefato NAO e' derivavel do nome da pasta (models/players/A-4" >&2
            echo "  publica libflight.so em plugins/data/flight). Remova o artefato pelo nome:" >&2
            echo "" >&2
            echo "    scripts/models.sh --remove --so lib<X>.so [--data <dir>]" >&2
            echo "" >&2
            echo "  O que sobrou em plugins/ hoje:" >&2
            for so in "$REPO_ROOT"/plugins/*.so; do
                [ -e "$so" ] && echo "    $(basename "$so")" >&2
            done
            exit 1
        fi

        ROTULO="${DEST_ABS#"$REPO_ROOT"/}"
        while IFS= read -r x; do [ -n "$x" ] && SOS+=("$x"); done < <(artefatos_publicados "$DEST_ABS")
        while IFS= read -r x; do [ -n "$x" ] && DATAS+=("$x"); done < <(datadirs_publicados "$DEST_ABS")
    fi

    if [ ${#SOS[@]} -eq 0 ]; then
        echo "nao consegui descobrir nenhum .so publicado por '$ROTULO'." >&2
        echo "  (nem ./dist local, nem shared_module() no meson.build)" >&2
        echo "  Passe explicitamente: --so lib<X>.so [--data <dir>]" >&2
        exit 1
    fi

    echo "removendo: $ROTULO"
    echo "  .so:  ${SOS[*]}"
    if [ ${#DATAS[@]} -gt 0 ]; then echo "  data: ${DATAS[*]}"; else echo "  data: (nenhum)"; fi
    echo ""

    # --- 2. RECUSAR se algum cenario ainda referencia -----------------------
    # Esta e' a razao de existir do modo: um .so orfao e' peso morto em tempo
    # de simulacao (PluginRegistry so' RESOLVE caminhos nomeados por um
    # ( PluginModule ), nunca varre diretorio), mas um CENARIO apontando para
    # um .so que nao existe mais quebra a aplicacao de verdade -- e nada
    # neste repositorio valida isso estaticamente.
    PADRAO=""
    for base in "${SOS[@]}"; do
        e="$(escapar_ere "$base")"
        PADRAO="${PADRAO:+$PADRAO|}file:[[:space:]]*\"$e\""
    done
    for d in "${DATAS[@]:-}"; do
        [ -n "$d" ] || continue
        e="$(escapar_ere "$d")"
        PADRAO="${PADRAO:+$PADRAO|}mixr-plugins/$e/"
    done

    REFS="$(referenciadores "$PADRAO")"
    if [ -n "$REFS" ]; then
        echo "RECUSADO: estes cenarios ainda referenciam o modelo --" >&2
        while IFS= read -r f; do
            [ -n "$f" ] && echo "    ${f#"$REPO_ROOT"/}" >&2
        done <<< "$REFS"
        echo "" >&2
        echo "  Remover agora deixaria a aplicacao inconsistente: o cenario carrega" >&2
        echo "  por 'file:' um .so que deixaria de existir, e o erro so' aparece" >&2
        echo "  RODANDO (nem edl_lint.py nem edlcheck cobrem isso hoje)." >&2
        echo "" >&2
        echo "  Apague ou reaponte esses cenarios antes -- ou passe --force se voce" >&2
        echo "  vai remover os dois na MESMA mudanca (e entao rode 'make test' antes" >&2
        echo "  de commitar: a guarda cenario-plugin cobra exatamente isto)." >&2
        if [ "$FORCE" != "1" ]; then
            exit 1
        fi
        echo "" >&2
        echo "  --force dado: seguindo mesmo assim." >&2
        echo "" >&2
    fi

    # --- 3. tirar de plugins/ (pela via oficial, enquanto a pasta existe) ---
    if [ -n "$DEST_ABS" ] && [ "$DRY_RUN" != "1" ]; then
        echo "3. make -C ${DEST_ABS#"$REPO_ROOT"/} uninstall-host"
        if ! make -C "$DEST_ABS" uninstall-host >/dev/null 2>&1; then
            echo "  aviso: uninstall-host falhou -- caindo na remocao direta pelos nomes ja descobertos" >&2
        fi
    fi
    echo "3. plugins/"
    for base in "${SOS[@]}"; do rm_arquivo "$REPO_ROOT/plugins/$base"; done
    for d in "${DATAS[@]:-}"; do [ -n "$d" ] && rm_dir "$REPO_ROOT/plugins/data/$d"; done

    # --- 4. podar dist/ ----------------------------------------------------
    # O espelho de 'sync-plugins' faria isto sozinho no proximo 'make
    # install', mas ele e' a rede de seguranca, nao o caminho principal:
    # deixar o estado limpo agora evita que um 'make test' entre a remocao e
    # o proximo install rode contra um dist/ que ainda tem o artefato.
    echo "4. dist/"
    for base in "${SOS[@]}"; do rm_arquivo "$REPO_ROOT/dist/lib/mixr-plugins/$base"; done
    for d in "${DATAS[@]:-}"; do [ -n "$d" ] && rm_dir "$REPO_ROOT/dist/share/mixr-plugins/$d"; done

    # --- 5. apagar a pasta (irreversivel, por ultimo) ----------------------
    if [ -n "$DEST_ABS" ]; then
        echo "5. fonte"
        rm_dir "$DEST_ABS"
    fi

    if [ "$DRY_RUN" = "1" ]; then
        echo ""
        echo "dry-run: nada foi removido de verdade."
        exit 0
    fi

    echo ""
    echo "remocao de $ROTULO concluida."
    imprimir_checklist_remocao "$ROTULO"
    exit 0
fi

# Letra inicial, depois letras (as duas caixas), digitos, underscore e hifen.
#
# A regra ANTERIOR era '^[a-z][a-z0-9_]*$' e recusava exatamente a convencao
# de PASTA que este repositorio ja usa em producao: models/players/A-4 --
# designacao de aeronave, com maiuscula e hifen (ver CLAUDE.md, secao "O
# MODELO e um plugin"). Nada FORA deste script deriva identificador do nome
# do modelo -- conferido: a descoberta de MODELOS_PRODUCAO do Makefile raiz
# e por 'find', models/common.mk nao le o nome em lugar nenhum, e o Makefile
# de cada modelo evita de proposito hardcodear 'lib<nome>.so'.
#
# O unico ponto que NAO aceita hifen e o namespace C++ (nao e caractere de
# identificador); e' o unico que precisa de traducao -- ver NS_NOVO logo
# abaixo. O resto usa $NAME literal: Meson aceita hifen/maiuscula em
# project()/shared_module() (medido: '--name F-5' produz libF-5.so), e
# 'lib<nome>.so'/MIXR_PLUGIN_DEFINE("<nome>") sao strings, nunca
# identificadores.
if ! [[ "$NAME" =~ ^[A-Za-z][A-Za-z0-9_-]*$ ]]; then
    echo "nome invalido: '$NAME' -- comece por letra e use letras/digitos/underscore/hifen (ex.: F-5, A-4, meu_modelo)" >&2
    exit 1
fi

# Namespace C++ deste modelo, derivado do nome: hifen -> underscore, caixa
# PRESERVADA ('F-5' -> 'xF_5'). Derivado aqui, e nao no passo 4 la embaixo,
# so para a checagem de colisao a seguir acontecer ANTES de copiar qualquer
# arquivo.
#
# 'F-5' e 'F_5' sao pastas distintas mas dariam o MESMO namespace -- uma
# colisao que a regra antiga (sem hifen) nao tinha como criar. CONTRATO.md
# secao 6 documenta o namespace aninhado por modelo como a defesa contra
# type_info colidindo por strcmp quando dois .so RTLD_LOCAL vivem no mesmo
# processo; essa colisao e silenciosa e cara, entao e recusada aqui.
NS_NOVO="x${NAME//-/_}"
COLISAO="$(grep -rlE "^[[:space:]]*namespace[[:space:]]+${NS_NOVO}[[:space:]]*\{" \
    "$REPO_ROOT/models" 2>/dev/null || true)"
if [ -n "$COLISAO" ]; then
    echo "namespace C++ '$NS_NOVO' (derivado de '$NAME', hifen -> underscore) ja e usado por:" >&2
    while IFS= read -r f; do
        [ -n "$f" ] && echo "    - ${f#"$REPO_ROOT"/}" >&2
    done <<< "$COLISAO"
    echo "  escolha outro --name -- dois modelos com o MESMO namespace colidem em type_info" >&2
    echo "  entre .so RTLD_LOCAL (models/players/template/docs/CONTRATO.md, secao 6)" >&2
    exit 1
fi

# categoria -> subpasta de models/ (ver o comentario de cabecalho: 'event'
# nao entra aqui de proposito).
case "$CATEGORIA" in
    player) CATEGORIA_DIR="players" ;;
    system) CATEGORIA_DIR="systems" ;;
    others) CATEGORIA_DIR="others" ;;
    *)
        echo "categoria invalida: '$CATEGORIA' -- use player, system ou others" >&2
        exit 1
        ;;
esac

ORIGEM="$REPO_ROOT/models/players/template"
ORIGEM_NOME="$(basename "$ORIGEM")"

if [ -n "$DEST" ]; then
    case "$DEST" in
        /*) DEST_ABS="$DEST" ;;
        *) DEST_ABS="$REPO_ROOT/$DEST" ;;
    esac
else
    DEST_ABS="$REPO_ROOT/models/$CATEGORIA_DIR/$NAME"
fi

case "$DEST_ABS" in
    "$REPO_ROOT"/*) : ;;
    *)
        echo "'$DEST_ABS' esta fora da raiz do repositorio -- o calculo de ROOT do" >&2
        echo "Makefile (linha_root()) exige um destino dentro do repositorio" >&2
        exit 1
        ;;
esac

DEST_REL="${DEST_ABS#"$REPO_ROOT"/}"

if [ -e "$DEST_ABS" ]; then
    echo "'$DEST_ABS' ja existe -- escolha outro --name/--dest" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# substituir ARQUIVO DE PARA [obrigatorio=1]
#
# Substituicao LITERAL (nao regex): bash trata 'de'/'para' como padrao de
# glob em '${var//de/para}', mas nenhuma string usada por este script contem
# '*'/'?'/'[' -- --name ja foi validado contra ^[A-Za-z][A-Za-z0-9_-]*$
# (hifen e maiuscula nao sao metacaracteres de glob) e, de qualquer forma,
# so aparece do lado do 'para', nunca do 'de'. O
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
    local nome="$1" dest_rel="$2"
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
  [ ] git add ${dest_rel}/ (este script nao commita nada)
  [ ] este modelo ja entra sozinho em 'make models'/'make test' da raiz (descoberta por
      'find' -- nao ha lista pra editar); falta so escrever um CENARIO pra ele: um
      '.edl.in' novo em src/poc/${nome}/configs/ (ja alcancavel por '-folder'/'-f', sem
      registrar em lugar nenhum) e, se fizer sentido, cobertura em tests/meson.build --
      ver CONTRIBUTING.md, secoes 5.2 e 5.4
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
# 'tools' entra na lista junto com include/src/tests (ACHADO RODANDO, nao
# redescobrir): tools/dump_tree_model.cpp nomeia o namespace do modelo
# (mixr::models::x<nome>::bt) para montar a factory, entao um scaffold com
# 'tools/' fora desta varredura sai com 'xtemplate' cravado la e NAO COMPILA
# -- reproduzido com 'make new-model NAME=probe-bt CATEGORY=others'. O
# models/players/A-4 nao expunha isso porque os nos dele vivem num
# 'bt_nodes' solto no escopo global (a excecao historica que
# models/players/template/docs/CONTRATO.md secao 6 manda NAO copiar).
ARQUIVOS_NS="$(arquivos_contendo "$DEST_ABS" "x$ORIGEM_NOME" include src tests tools)"

# 4. namespace aninhado -- CONTRATO.md secao 6: 'xtemplate' -> 'x<nome>'.
#    $NS_NOVO ja foi derivado (e ja teve a colisao checada) logo apos a
#    validacao de --name, mais acima neste script.
#
# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): a derivacao era
# 'x$(printf %s "$NAME" | tr -cd 'a-z0-9_')', e esse 'tr -cd' removia o
# underscore junto com o resto da pontuacao -- '--name auto_pilot' e
# '--name autopilot' geravam o MESMO namespace 'xautopilot' (reproduzido).
# Hoje a traducao e' so hifen -> underscore, com caixa preservada, e o
# unico modo de duas entradas coincidirem ('F-5' e 'F_5') e recusado na
# checagem de colisao la de cima. O resto do script usa $NAME LITERAL
# (lib$NAME.so, MIXR_PLUGIN_DEFINE("$NAME")) -- so o namespace precisa ser
# um identificador C++ valido.
NS_VELHO="x$ORIGEM_NOME"
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
# MESMA lista da varredura de renomeacao acima -- se divergirem, esta guarda
# passa a dar verde sobre um diretorio que ninguem renomeou (foi exatamente o
# que aconteceu com 'tools/': o scaffold nao compilava e este check nao via).
SOBRAS="$(arquivos_contendo "$DEST_ABS" "$NS_VELHO" include src tests tools)"
SOBRAS_MESON="$(arquivos_contendo "$DEST_ABS" "'$ORIGEM_NOME'" . | grep -F 'meson.build' || true)"
if [ -n "$SOBRAS" ] || [ -n "$SOBRAS_MESON" ]; then
    echo "  aviso: ainda ha ocorrencias do nome/namespace antigo em:"
    { [ -n "$SOBRAS" ] && printf '%s\n' "$SOBRAS"; [ -n "$SOBRAS_MESON" ] && printf '%s\n' "$SOBRAS_MESON"; } \
        | sort -u | while IFS= read -r f; do echo "    - ${f#"$REPO_ROOT"/}"; done
fi

if [ "$NO_BUILD" = "1" ]; then
    echo "scaffold de $DEST_REL/ pronto (build de verificacao PULADO, --no-build)."
    imprimir_checklist "$NAME" "$DEST_REL"
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
FALHOU o build/teste de verificacao -- o scaffold ficou em $DEST_REL/,
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
scaffold de $DEST_REL/ compilou e passou nos testes, mas FALHOU nas
checagens de contrato do plugin acima -- corrija antes de considerar pronto." >&2
    exit 1
fi

echo ""
echo "scaffold de $DEST_REL/ pronto e compilando/testando verde."
imprimir_checklist "$NAME" "$DEST_REL"
