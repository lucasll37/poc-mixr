#!/usr/bin/env bash
#
# Abre um arquivo local (uma pagina HTML estatica) no navegador do usuario,
# funcionando tanto em Linux nativo quanto dentro do WSL2. Usado por
# 'make open-docs' e 'make open-edl-builder' -- separado em script proprio
# porque a cadeia de tentativas nao cabe numa linha de receita, e porque as
# duas receitas carregavam a MESMA copia dela.
#
# A ordem, e o porque de cada degrau:
#
#   1. $BROWSER     -- o escape hatch do proprio usuario (sessao por ssh,
#                      navegador que nao e' o padrao do sistema). E' a mesma
#                      convencao que o xdg-open respeita por dentro.
#   2. xdg-open     -- o caminho de Linux NATIVO: pacote 'xdg-utils' mais uma
#                      sessao grafica com handler registrado para text/html.
#   3. wslview      -- WSL2 com o pacote 'wslu' instalado; abre no navegador
#                      do WINDOWS, nao num navegador Linux.
#   4. explorer.exe -- WSL2 sem 'wslu' -- o caso da imagem padrao do Ubuntu no
#                      WSL2, que nao traz NEM 'xdg-utils' NEM 'wslu'. Exige o
#                      caminho em formato Windows: 'wslpath -w' devolve o
#                      \\wsl.localhost\<distro>\... que o navegador consegue
#                      ler de dentro do sistema de arquivos da VM.
#
# Nenhum degrau e' obrigatorio: falhando todos, imprime a URL file:// para
# abrir a mao -- exatamente o que as receitas ja faziam antes, so que agora
# depois de TENTAR, em vez de desistir so por 'xdg-open' nao existir.
#
# ARMADILHA CONFIRMADA -- nao redescobrir: 'explorer.exe' devolve codigo de
# saida 1 mesmo quando abre a pagina com sucesso (comportamento conhecido do
# proprio Windows, nao um erro deste script). Por isso este degrau NUNCA e'
# julgado pelo codigo de saida -- ele imprime a URL junto, para o usuario ter
# o caminho a mao se nada abrir.
#
# LIMITE CONHECIDO: $BROWSER e' tratado como o nome de UM comando
# ("BROWSER=firefox", "BROWSER=wslview"). A forma com placeholder da
# convencao original ("BROWSER=firefox\ %s") nao e' expandida -- ela
# simplesmente nao casa no 'command -v' e o script cai no degrau seguinte,
# que e' degradacao, nao quebra.
set -u

AMARELO='\033[1;33m'
SEM_COR='\033[0m'

if [ $# -ne 1 ]; then
    echo "uso: $(basename "$0") <arquivo>" >&2
    exit 2
fi

ALVO=$1

if [ ! -e "$ALVO" ]; then
    echo "open_browser: arquivo nao encontrado: $ALVO" >&2
    exit 1
fi

# Caminho ABSOLUTO: 'xdg-open' aceitaria o relativo, mas 'wslpath -w' e o
# proprio explorer.exe (que roda com o CWD do lado Windows) nao.
ABS=$(readlink -f "$ALVO")
URL="file://$ABS"

# 1. $BROWSER
if [ -n "${BROWSER:-}" ] && command -v "$BROWSER" >/dev/null 2>&1; then
    if "$BROWSER" "$URL" >/dev/null 2>&1; then
        echo "aberto via \$BROWSER ($BROWSER): $ABS"
        exit 0
    fi
fi

# 2. xdg-open (Linux nativo)
if command -v xdg-open >/dev/null 2>&1; then
    if xdg-open "$ABS" >/dev/null 2>&1; then
        echo "aberto via xdg-open: $ABS"
        exit 0
    fi
fi

# 3. wslview (WSL2 com 'wslu')
if command -v wslview >/dev/null 2>&1; then
    if wslview "$ABS" >/dev/null 2>&1; then
        echo "aberto via wslview: $ABS"
        exit 0
    fi
fi

# 4. explorer.exe (WSL2 sem 'wslu') -- ver a armadilha do codigo de saida 1
# no cabecalho: aqui o sucesso nao e' julgado pelo 'exit code'.
if command -v explorer.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
    CAMINHO_WIN=$(wslpath -w "$ABS" 2>/dev/null)
    if [ -n "$CAMINHO_WIN" ]; then
        explorer.exe "$CAMINHO_WIN" >/dev/null 2>&1 || true
        echo "aberto via explorer.exe (navegador do Windows): $CAMINHO_WIN"
        exit 0
    fi
fi

printf "${AMARELO}open_browser:${SEM_COR} nenhum abridor encontrado (xdg-open/wslview/explorer.exe) -- abra manualmente: %s\n" "$URL"
printf "  Linux nativo: sudo apt install xdg-utils   |   WSL2: sudo apt install wslu\n"
exit 0
