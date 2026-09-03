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
   if grep -nE "'(domain|bt|ubf)/" "$mb" > /dev/null 2>&1; then
      achados_mb="$achados_mb
  FALHA $mb voltou a listar fonte do modelo:
$(grep -nE "'(domain|bt|ubf)/" "$mb" | sed 's/^/        /')"
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
# ARMADILHA MEDIDA: 'find src app -type d -name src' TAMBEM casa a propria
# raiz 'src' (find testa o proprio ponto de partida) -- sem '-mindepth 1' o
# achado duplicava (uma vez pela raiz recursiva, outra pelo diretorio
# especifico) e a busca descia em node_modules/.venv (lento, e sem
# relevancia nenhuma pro invariante).
host_src_dirs="$(find src app -mindepth 1 \( -name node_modules -o -name .venv -o -name __pycache__ \) -prune -o -type d \( -name src -o -name include -o -name bindings \) -print 2>/dev/null)"
achados="$(echo "$host_src_dirs" | xargs -r grep -rnE '#include "(domain|bt|ubf|xnative)/' 2>/dev/null || true)"
if [ -n "$achados" ]; then
   echo "  FALHA codigo do host incluindo header do modelo:"
   echo "$achados" | sed 's/^/        /'
   fail=1
else
   echo "  OK   nenhum fonte do host inclui header do modelo"
fi

# 3) as arvores do modelo nao existem mais dentro das pocs
for d in domain bt ubf xnative; do
   for p in single-thread multi-thread; do
      if [ -e "src/poc/$p/src/$d" ] || [ -e "src/poc/$p/include/$d" ]; then
         echo "  FALHA src/poc/$p ainda tem a arvore '$d' -- o modelo mora em models/flight"
         fail=1
      fi
   done
done
[ $fail -eq 0 ] && echo "  OK   domain/, bt/, ubf/ e xnative/ nao existem mais sob src/"

[ $fail -eq 0 ] && { echo "host opaco: OK"; exit 0; }
echo "host opaco: FALHOU"
exit 1
