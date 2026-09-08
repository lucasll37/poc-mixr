#!/usr/bin/env bash
#
# Os .so DE PRODUCAO instalados sao mais novos que o fonte deles?
#
# Existe por causa de um efeito colateral da separacao, e ele e silencioso: o
# meson do host NAO tem aresta de dependencia ate o plugin (ele vem pronto de
# dist/, construido por 'make models', noutro projeto). Sem esta guarda, a
# suite inteira do host passa contra um plugin VELHO, e uma regressao no
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
# ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): este script tinha
# `for modelo in models/players/*/` -- so um nivel sob players/, apesar do
# comentario acima ja prometer "mesma filosofia" de check_modelo_estrutura.sh
# (que descobre em QUALQUER subpasta de models/). Falso-negativo latente: um
# modelo real nascido em models/systems/<nome>/ ou models/others/<nome>/
# (o destino real de 'make new-model CATEGORY=system|others') passaria por
# esta guarda sem checagem de frescor nenhuma.
#
# CORRIGIDO (nao redescobrir o contrario): este comentario chegou a excluir
# 'template/' daqui, com a justificativa de que ele "nunca e instalado em
# dist/lib/mixr-plugins/ do host". Medido no disco e no Makefile raiz: e
# falso -- 'make models' builda o template EXPLICITAMENTE (alvo 'models',
# linha separada da dos modelos de producao) e deposita libtemplate.so/
# libtemplate_mirror.so em plugins/ igual a qualquer modelo real; 'make
# install' (sync-plugins) copia os dois para a RAIZ dist/lib/mixr-plugins/
# junto com libflight.so -- confirmado com 'ls dist/lib/mixr-plugins/'. E
# libtemplate_mirror.so nao e decorativo: e o que os testes
# 'plugin-modelo-estranho'/'plugin-deposito-terceiro' carregam para provar
# que o CONTRATO (nao o fonte do modelo de producao) basta. Excluir
# 'template/' daqui era exatamente o buraco que esta guarda existe para
# fechar -- editar mirror.cpp sem rebuildar deixava aqueles dois testes
# passando contra um .so requentado, em silencio. 'fixtures/' tambem saiu
# da exclusao: a pasta nao existe mais neste repositorio (removida, ver
# CLAUDE.md) -- exclui-la aqui so escondia que o find nunca a acharia mesmo.
#
# Os basenames de .so a checar de CADA modelo vem do PROPRIO './dist' local
# dele (populado por 'make build'/'make install-host' daquele projeto) -- nao
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
      so_host="dist/lib/mixr-plugins/$base"
      checados=$((checados + 1))

      if [ ! -f "$so_host" ]; then
         echo "  FALHA $so_host nao existe -- rode 'make install'"
         fail=1
         continue
      fi

      novo="$(find "$modelo/src" "$modelo/include" "$modelo/configs" -type f -newer "$so_host" 2>/dev/null | head -5)"
      if [ -n "$novo" ]; then
         echo "  FALHA fonte de $modelo mais novo que $so_host:"
         echo "$novo" | sed 's/^/        /'
         echo "        rode 'make models' antes de testar o host"
         fail=1
      fi
   done

   # ACHADO POR AUDITORIA, CORRIGIDO (nao redescobrir): os .so de VARIANTE de
   # teste do host (ex.: libmodel_leak.so/libmodel_variant_{a,b}.so de A-4,
   # atras da opcao 'variants' -- ver models/players/A-4/meson.build,
   # consumidos por 'memory-controle-negativo'/'plugin-hotswap' em
   # tests/meson.build) NUNCA sao instalados -- so existem direto em
   # "$modelo/build/". O loop acima (que so varre "$modelo/dist/...") nunca
   # os alcancava: editar a fonte que eles exercitam (ex.:
   # domain/PatrolPlan.cpp) sem reconfigurar/recompilar o modelo com
   # '-Dvariants=true' rodava esses dois testes contra um .so desatualizado
   # (ou ausente, que ao menos falha alto) sem NENHUM aviso de frescor.
   # Cobre qualquer '.so' que fique direto em build/ (nao hardcoda nome —
   # um modelo futuro pode ter variantes proprias com outros nomes).
   #
   # SO' o que NAO tem par instalado em "$locais" -- ACHADO RODANDO, NA
   # PROPRIA VALIDACAO DESTE FIX (nao redescobrir): checar TAMBEM os .so ja
   # cobertos pelo loop acima (ex.: libtemplate.so) parecia "redundante e
   # inofensivo", mas nao e -- os dois loops comparam contra a MESMA arvore
   # inteira de fonte (src/include/configs), coarse, nao por-alvo. Um
   # projeto com DOIS artefatos que compartilham a mesma pasta de fonte mas
   # tem sources DISJUNTOS (template: libtemplate.so vem de
   # domain/ubf/xnative/plugin.cpp; libtemplate_mirror.so vem SO de
   # mirror.cpp) faz esse loop acusar libtemplate.so de "desatualizado"
   # so' porque mirror.cpp mudou -- falso positivo, reproduzido editando
   # mirror.cpp e rodando esta guarda sem tocar em mais nada. Restringir
   # este loop a .so's SEM par instalado (o caso que de fato motivou esta
   # secao -- variantes de teste como libmodel_leak.so, que NUNCA chegam a
   # "$locais") evita o falso positivo sem tentar resolver dependencia
   # por-alvo em bash (isso e' o que 'meson introspect --targets' faria de
   # verdade, fora de escopo aqui).
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
            echo "        rode 'make -C $modelo build VARIANTS=true' antes de testar o host"
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
