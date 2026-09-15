#!/usr/bin/env bash
#
# O CORE NAO PODE CONHECER O FONTE DO MODELO.
#
# Esta e a guarda do invariante que a separacao existe para criar. Ela nao
# afirma que o binario nao contem o modelo (isso ja era verdade antes da
# separacao: o modelo nunca aparecia nos simbolos do executavel mesmo quando
# o .cpp dele era compilado junto). Ela afirma a coisa que a separacao de
# fato muda: que o BUILD do core nao referencia, nao inclui e nao compila
# uma linha do modelo.
#
# E o que torna verificavel o cenario pedido: um terceiro entrega so o .so.
#
# Os tres checks abaixo descobrem caminhos por 'find' (extensao de arquivo
# ou nome de pasta do MODELO), nunca por glob/lista fixa de subprojeto —
# um glob de profundidade fixa ('src/*/src') ou uma lista nomeada
# ('single-thread multi-thread') fica cega a qualquer subprojeto que não
# siga exatamente esse layout (ex.: src/node/ tem .cpp/.hpp direto na
# própria pasta, sem subpasta 'src'/'include'), e um caminho novo nasce
# desprotegido até alguém notar.
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1
fail=0

# 1) nenhum arquivo de build do core cita fonte do modelo
#
# NAO estender pra 'tests/' aqui: tests/meson.build referencia os proprios
# arquivos da suite do core por convencao 'domain/test_*.cpp' (a pasta
# tests/domain/, nao o domain/ do modelo) — o mesmo regex que funciona por
# CONTEUDO de #include na checagem 2 vira falso positivo aqui, porque aqui
# o alvo e um CAMINHO DE ARQUIVO em files(), e 'domain/test_xmsg_rules.cpp'
# bate no regex sem ter nada a ver com o modelo.
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
   echo "  OK   nenhum meson.build do core lista fonte do modelo"
fi

# 2) nenhum .cpp/.hpp do core inclui header do modelo
#    (xtrack/, xboard/, xlog/ e xrlbridge/ sao do SDK, nao do modelo -- por
#    isso o regex abaixo, restrito aos 4 prefixos do modelo, nao os pega)
#
# Busca por EXTENSAO de arquivo (.cpp/.hpp) direto sob src/, app/ e tests/ —
# nao por NOME de diretorio pai ('src'/'include'/'bindings'): src/node/ (o
# runner headless, "peer enxuto de ./app") tem seus .cpp/.hpp direto na
# propria pasta, sem nenhuma subpasta com esses nomes, entao ficaria
# invisivel para uma busca por nome de pasta. tests/ entra tambem: e parte
# legitima do build do CORE. Checagem 3 (por NOME de pasta, nao conteudo)
# continua so sob src/app — estende-la pra tests/ criaria falso positivo
# contra a propria pasta tests/domain/ (nome de suite do core, coincidencia
# textual com o nome do modelo).
achados="$(find src app tests -mindepth 1 \( -name node_modules -o -name .venv -o -name __pycache__ \) -prune -o -type f \( -name '*.cpp' -o -name '*.hpp' \) -print 2>/dev/null \
   | xargs -r grep -nE '#include "(domain|bt|ubf|xnative)/' 2>/dev/null || true)"
if [ -n "$achados" ]; then
   echo "  FALHA codigo do core incluindo header do modelo:"
   echo "$achados" | sed 's/^/        /'
   fail=1
else
   echo "  OK   nenhum fonte do core inclui header do modelo"
fi

# 3) as arvores do modelo nao existem mais dentro das pocs
#
# 'find' sob src/ e app/ inteiros, sem depender de lista de poc nenhuma —
# uma lista fixa de nomes de poc fica cega a qualquer poc nova.
achados_arvore="$(find src app -mindepth 1 \( -name node_modules -o -name .venv -o -name __pycache__ \) -prune -o -type d \( -name domain -o -name bt -o -name ubf -o -name xnative \) -print 2>/dev/null)"
if [ -n "$achados_arvore" ]; then
   echo "  FALHA arvore do modelo encontrada sob src/ ou app/ -- o modelo mora em models/players/A-4:"
   echo "$achados_arvore" | sed 's/^/        /'
   fail=1
else
   echo "  OK   domain/, bt/, ubf/ e xnative/ nao existem mais sob src/"
fi

[ $fail -eq 0 ] && { echo "core opaco: OK"; exit 0; }
echo "core opaco: FALHOU"
exit 1
