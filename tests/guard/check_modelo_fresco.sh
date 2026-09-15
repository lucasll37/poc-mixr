#!/usr/bin/env bash
#
# Os .so DE PRODUCAO instalados sao mais novos que o fonte deles?
#
# Existe por causa de um efeito colateral da separacao, e ele e silencioso: o
# meson do core NAO tem aresta de dependencia ate o plugin (ele vem pronto de
# dist/, construido por 'make models', noutro projeto). Sem esta guarda, a
# suite inteira do core passa contra um plugin VELHO, e uma regressao no
# modelo fica invisivel -- as assercoes de cenario sao semanticas e grossas.
#
# Conferir mtime nao e conhecer o fonte: e conhecer um CAMINHO, exatamente
# como o 'searchPaths:' do cenario ja faz.
#
# Descobre os modelos por find sob QUALQUER subpasta de models/ (mesma
# filosofia de check_modelo_estrutura.sh/o 'MODELOS_PRODUCAO' do Makefile
# raiz: um modelo novo em models/systems/ ou models/others/ ja entra na
# checagem, sem editar este arquivo).
#
# A descoberta é por 'find' sob QUALQUER subpasta de models/ (mesma filosofia
# de check_modelo_estrutura.sh) — um glob restrito a um nível fixo (ex.: só
# models/players/*/) ficaria cego a um modelo nascido em models/systems/ ou
# models/others/ (o destino real de 'make new-model CATEGORY=system|others').
#
# 'template/' entra nesta checagem como qualquer modelo real, sem exceção:
# 'make models' builda o template explicitamente e deposita libtemplate.so/
# libtemplate_mirror.so em plugins/ igual a qualquer modelo de produção;
# 'make install' (sync-plugins) copia os dois para dist/lib/mixr-plugins/
# junto com libA-4.so. libtemplate_mirror.so não é decorativo: é o que os
# testes 'plugin-modelo-estranho'/'plugin-deposito-terceiro' carregam para
# provar que o CONTRATO (não o fonte do modelo de produção) basta — sem
# checar o frescor dele, editar mirror.cpp sem rebuildar deixaria aqueles
# dois testes passando contra um .so requentado, em silêncio.
#
# Os basenames de .so a checar de CADA modelo vem do PROPRIO './dist' local
# dele (populado por 'make build'/'make install-core' daquele projeto) -- nao
# de uma lista escrita a mao, que envelheceria em silencio (um modelo futuro
# pode produzir mais de um artefato, como o template faz hoje com
# libtemplate.so + libtemplate_mirror.so).
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

fail=0
checados=0

# Mesmo padrao de find de check_modelo_estrutura.sh: qualquer subpasta de
# models/ com um meson.build de projeto de verdade (project() na raiz) --
# nao filtra 'template/' por path (ele CONTINUA fazendo parte desta
# checagem, ver o comentario grande acima).
modelos="$(find models -mindepth 2 -name meson.build \
              -not -path '*/build/*' -not -path '*/dist/*' \
              -not -path '*/subprojects/*' 2>/dev/null \
           | xargs -r -n1 dirname | sort -u)"

while IFS= read -r modelo; do
   [ -z "$modelo" ] && continue
   grep -q '^project(' "$modelo/meson.build" || continue

   locais="$modelo/dist/lib/mixr-plugins"
   if [ ! -d "$locais" ] || [ -z "$(ls -A "$locais"/*.so 2>/dev/null)" ]; then
      echo "  FALHA $modelo nunca foi construido localmente -- rode 'make models' (ou 'make -C $modelo build')"
      fail=1
      continue
   fi

   for so_local in "$locais"/*.so; do
      base="$(basename "$so_local")"
      so_core="dist/lib/mixr-plugins/$base"
      checados=$((checados + 1))

      if [ ! -f "$so_core" ]; then
         echo "  FALHA $so_core nao existe -- rode 'make install'"
         fail=1
         continue
      fi

      novo="$(find "$modelo/src" "$modelo/include" "$modelo/configs" -type f -newer "$so_core" 2>/dev/null | head -5)"
      if [ -n "$novo" ]; then
         echo "  FALHA fonte de $modelo mais novo que $so_core:"
         echo "$novo" | sed 's/^/        /'
         echo "        rode 'make models' antes de testar o core"
         fail=1
      fi
   done

   # Os .so de VARIANTE de teste do core (ex.: libmodel_leak.so/
   # libmodel_variant_{a,b}.so de A-4, atrás da opção 'variants' — ver
   # models/players/A-4/meson.build, consumidos por
   # 'memory-controle-negativo'/'plugin-hotswap' em tests/meson.build) nunca
   # são instalados — só existem direto em "$modelo/build/", fora do alcance
   # do loop acima (que só varre "$modelo/dist/..."). Cobre qualquer '.so'
   # que fique direto em build/ sem hardcodar nome — um modelo futuro pode
   # ter variantes próprias com outros nomes.
   #
   # Só o que NÃO tem par instalado em "$locais": os dois loops comparam
   # contra a MESMA árvore inteira de fonte (src/include/configs), não
   # por-alvo — um projeto com dois artefatos de sources DISJUNTOS (template:
   # libtemplate.so vem de domain/ubf/xnative/plugin.cpp; libtemplate_mirror.so
   # vem só de mirror.cpp) faria este segundo loop acusar libtemplate.so de
   # "desatualizado" só porque mirror.cpp mudou, se checasse também os .so já
   # cobertos pelo loop acima. Restringir a .so's sem par instalado evita esse
   # falso positivo sem resolver dependência por-alvo em bash (isso é o que
   # 'meson introspect --targets' faria de verdade, fora de escopo aqui).
   build_dir="$modelo/build"
   if [ -d "$build_dir" ]; then
      for so_build in "$build_dir"/*.so; do
         [ -e "$so_build" ] || continue
         base="$(basename "$so_build")"
         [ -e "$locais/$base" ] && continue
         checados=$((checados + 1))

         novo="$(find "$modelo/src" "$modelo/include" "$modelo/configs" -type f -newer "$so_build" 2>/dev/null | head -5)"
         if [ -n "$novo" ]; then
            echo "  FALHA fonte de $modelo mais novo que $so_build (variante de teste, nunca instalada):"
            echo "$novo" | sed 's/^/        /'
            echo "        rode 'make -C $modelo build VARIANTS=true' antes de testar o core"
            fail=1
         fi
      done
   fi
done <<< "$modelos"

if [ "$fail" -ne 0 ]; then
   echo "modelo fresco: FALHOU"
   exit 1
fi

echo "  OK   $checados .so verificado(s), todos mais novos que o proprio fonte"
echo "modelo fresco: OK"
