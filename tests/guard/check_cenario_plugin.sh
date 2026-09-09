#!/usr/bin/env bash
#
# TODO 'file:' DE UM ( PluginModule ) TEM DE APONTAR PARA UM .so QUE EXISTE.
#
# Esta e a unica verificacao deste repositorio sobre a inconsistencia que de
# fato QUEBRA a aplicacao quando um modelo e removido. A ordem importa:
#
#   - um .so ORFAO (sobrou em plugins/ de um modelo apagado) e peso morto em
#     tempo de simulacao -- libs/xplugin/PluginRegistry.cpp so' RESOLVE um
#     caminho NOMEADO por um ( PluginModule ) (funcao resolve(): concatena
#     dir + "/" + file e chama realpath), nunca varre diretorio. Ninguem o
#     carrega sozinho;
#   - o INVERSO e' fatal: um cenario que nomeia um .so que nao existe mais
#     morre ao carregar, e o erro so' aparece RODANDO. Nem
#     src/ui/scripts/edl_lint.py (que nao olha 'file:') nem o binario
#     'edlcheck' (que valida a GRAMATICA, nao a existencia do arquivo)
#     cobrem isso.
#
# Descoberta por 'find', nunca lista fixa -- cenario novo ja nasce coberto,
# mesma licao registrada no cabecalho de check_host_opaco.sh.
#
# *.generated.edl fica de fora de proposito: e' artefato de RUNTIME
# (app::generateScenario()), gitignorado e regenerado sozinho a cada
# execucao -- um match ali nao e' inconsistencia de FONTE, e cobrar por causa
# dele seria um falso positivo permanente. '.gitlab-ci-local/' e 'contexts/'
# tambem: o primeiro e' a copia que 'make test-ci' faz do repositorio inteiro,
# o segundo e' fonte VENDORIZADA de terceiro.
#
# SEGUNDO MODO, '--refs <padrao-ere>': em vez de checar, LISTA os cenarios que
# casam o padrao, um por linha. E' o que 'scripts/models.sh --remove' consome
# para recusar a remocao de um modelo ainda referenciado -- uma implementacao
# da varredura, dois consumidores, para as duas nunca divergirem sobre o que
# conta como "cenario de fonte".
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$RAIZ" || exit 1

# cenarios_de_fonte -- os .edl/.edl.in que sao FONTE deste repositorio.
cenarios_de_fonte() {
   find . \
        -type d \( -name build -o -name 'build-*' -o -name dist -o -name .git \
                   -o -name node_modules -o -name .venv -o -name .gitlab-ci-local \
                   -o -name contexts -o -name .conan2-cache \) -prune -o \
        -type f \( -name '*.edl' -o -name '*.edl.in' \) -print 2>/dev/null \
     | grep -v '\.generated\.edl$' \
     | sed 's|^\./||' \
     | LC_ALL=C sort
}

# --- modo consulta -------------------------------------------------------
if [ "${1:-}" = "--refs" ]; then
   if [ -z "${2:-}" ]; then
      echo "uso: $0 --refs <padrao-ere>" >&2
      exit 2
   fi
   cenarios_de_fonte | xargs -r grep -lE -- "$2" 2>/dev/null || true
   exit 0
fi

# --- modo guarda ---------------------------------------------------------
fail=0
checados=0

while IFS= read -r cenario; do
   [ -n "$cenario" ] || continue
   # Um 'file:' so' conta se for de um ( PluginModule ) -- mas o slot 'file:'
   # tambem existe em ( SrtmHgtFile ) e companhia, com valor que NAO e' .so.
   # Filtrar por '\.so"' basta e nao depende de parsear a estrutura do EDL
   # (que exigiria o parser de verdade; ver edlcheck).
   while IFS= read -r base; do
      [ -n "$base" ] || continue
      checados=$((checados + 1))
      if [ ! -e "plugins/$base" ]; then
         echo "  FALHA $cenario nomeia '$base', que nao existe em plugins/"
         fail=1
      fi
   done < <(grep -oE 'file:[[:space:]]*"[^"]+\.so"' "$cenario" 2>/dev/null \
              | sed 's/.*"\(.*\)"/\1/' | LC_ALL=C sort -u)
done <<< "$(cenarios_de_fonte)"

if [ "$fail" -ne 0 ]; then
   echo
   echo "Um cenario aponta para um plugin que nao esta mais no deposito."
   echo "  - se o modelo ainda existe:   make models   (reconstroi e deposita)"
   echo "  - se o modelo foi REMOVIDO:   apague ou reaponte o 'file:'/'provides:'"
   echo "    desses cenarios -- 'make rm-model' recusa justamente para isto nao"
   echo "    acontecer, entao aqui alguem usou --force ou removeu a pasta a mao."
   exit 1
fi

echo "OK -- $checados referencia(s) de ( PluginModule ) conferida(s), todas presentes em plugins/."
exit 0
