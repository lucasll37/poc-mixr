#!/usr/bin/env bash
#
# O HOST NAO PODE CONHECER O FONTE DO MODELO.
#
# Esta e a guarda do invariante que a separacao existe para criar. Ela nao
# afirma que o binario nao contem o modelo (isso ja era verdade antes da
# separacao -- medido: 'nm -C' no executavel dava 0 simbolos do modelo mesmo
# quando o .cpp dele era compilado junto). Ela afirma a coisa que a separacao
# de fato mudou: que o BUILD do host nao referencia, nao inclui e nao compila
# uma linha do modelo.
#
# E o que torna verificavel o cenario pedido: um terceiro entrega so o .so.
#
# ARMADILHA CONFIRMADA (nao redescobrir): os globs originais ('src/*/src',
# 'src/*/include') so alcancavam UM nivel abaixo de src/ -- cobriam
# src/server/{src,include}, mas nao src/poc/<poc>/{src,include} (dois niveis,
# desde a renomeacao poc/ -> src/poc/) nem src/rl/bindings (nem 'src/' nem
# 'include/' como nome de pasta). O check 3, por sua vez, testava caminhos
# 'src/$p' que nunca existiram (sempre foi 'src/poc/$p') -- vacuamente
# verdadeiro. Os tres achados por 'find', nao por glob fixo, para sobreviver
# a proximo subprojeto novo sem precisar editar este arquivo.
#
# ARMADILHA 2 CONFIRMADA (nao redescobrir): o check 2 ainda buscava por NOME
# de diretorio pai ('src'/'include'/'bindings'), a mesma classe de erro do
# paragrafo acima -- src/node/ (o runner headless) tem .cpp/.hpp DIRETO na
# propria pasta, sem subpasta nenhuma com esses nomes, e ficava invisivel
# (reproduzido plantando um '#include "domain/..."' ali, nao detectado).
# Trocado pra buscar por EXTENSAO de arquivo, o mesmo criterio do check 3.
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1
fail=0

# 1) nenhum arquivo de build do host cita fonte do modelo
mbs="$(find src app -name meson.build 2>/dev/null)
meson.build"
achados_mb=""
while IFS= read -r mb; do
   [ -z "$mb" ] && continue
   if grep -nE "'(domain|bt|ubf|xnative)/" "$mb" > /dev/null 2>&1; then
      achados_mb="$achados_mb
  FALHA $mb voltou a listar fonte do modelo:
$(grep -nE "'(domain|bt|ubf|xnative)/" "$mb" | sed 's/^/        /')"
      fail=1
   fi
done <<< "$mbs"
if [ -n "$achados_mb" ]; then
   echo "$achados_mb"
else
   echo "  OK   nenhum meson.build do host lista fonte do modelo"
fi

# 2) nenhum .cpp/.hpp do host inclui header do modelo
#    (xtrack/, xboard/, xlog/ e xrlbridge/ sao do SDK, nao do modelo -- por
#    isso o regex abaixo, restrito aos 4 prefixos do modelo, nao os pega)
#
# CORRIGIDO (nao redescobrir): esta checagem buscava por NOME de diretorio
# pai ('src'/'include'/'bindings'), nao por arquivo -- src/node/ (o runner
# headless, "peer enxuto de ./app") tem seus .cpp/.hpp DIRETO em src/node/,
# sem nenhuma subpasta com esses nomes, entao ficava INVISIVEL pra esta
# checagem (reproduzido: um '#include "domain/Foo.hpp"' plantado em
# src/node/main.cpp nao era detectado). Trocado pra buscar por EXTENSAO de
# arquivo (.cpp/.hpp) direto sob src/ e app/, o mesmo criterio ja usado na
# checagem 3 abaixo -- cobre qualquer subprojeto host futuro, com qualquer
# nome de pasta.
achados="$(find src app -mindepth 1 \( -name node_modules -o -name .venv -o -name __pycache__ \) -prune -o -type f \( -name '*.cpp' -o -name '*.hpp' \) -print 2>/dev/null \
   | xargs -r grep -nE '#include "(domain|bt|ubf|xnative)/' 2>/dev/null || true)"
if [ -n "$achados" ]; then
   echo "  FALHA codigo do host incluindo header do modelo:"
   echo "$achados" | sed 's/^/        /'
   fail=1
else
   echo "  OK   nenhum fonte do host inclui header do modelo"
fi

# 3) as arvores do modelo nao existem mais dentro das pocs
#
# ARMADILHA CONFIRMADA (nao redescobrir): esta checagem chegou a usar uma
# lista fixa ('for p in single-thread multi-thread'), o mesmo erro ja
# registrado no cabecalho para os checks 1/2 -- e ela nao pegava
# python-flight/onnx-policy/app (pocs mais novas que a lista). Achado
# reproduzindo: uma pasta 'src/domain/' plantada em python-flight passava
# batido. Trocado por 'find' sob src/ e app/ inteiros, sem depender de nome
# de poc nenhum.
achados_arvore="$(find src app -mindepth 1 \( -name node_modules -o -name .venv -o -name __pycache__ \) -prune -o -type d \( -name domain -o -name bt -o -name ubf -o -name xnative \) -print 2>/dev/null)"
if [ -n "$achados_arvore" ]; then
   echo "  FALHA arvore do modelo encontrada sob src/ ou app/ -- o modelo mora em models/players/A-4:"
   echo "$achados_arvore" | sed 's/^/        /'
   fail=1
else
   echo "  OK   domain/, bt/, ubf/ e xnative/ nao existem mais sob src/"
fi

[ $fail -eq 0 ] && { echo "host opaco: OK"; exit 0; }
echo "host opaco: FALHOU"
exit 1
