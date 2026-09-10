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
# abrir a mao -- mas sai com codigo 1, para o alvo do make ACUSAR que nada
# abriu em vez de terminar verde. Imprimir a URL e' rede de seguranca, nao
# sucesso.
#
# ARMADILHA CONFIRMADA -- nao redescobrir: 'explorer.exe' devolve codigo de
# saida 1 mesmo quando abre a pagina com sucesso (comportamento conhecido do
# proprio Windows, nao um erro deste script). Por isso o degrau 4 nao pode ser
# julgado por "codigo != 0".
#
# SEGUNDA ARMADILHA, MEDIDA DEPOIS -- e a razao de o teste nao ser mais um
# '|| true' cru: ignorar o codigo INTEIRO tambem engole o 126 ("cannot execute
# binary file: Exec format error"), que e' o que sai quando o interop
# Windows<->WSL esta desligado (sem a entrada 'WSLInterop' em
# /proc/sys/fs/binfmt_misc/, o kernel nao lanca binario PE nenhum). Nesse
# estado o script imprimia "aberto via explorer.exe" para uma falha TOTAL --
# 'make open-docs'/'open-presentation'/'open-edl-builder' saiam 0 sem abrir
# nada. Hoje 126/127 (nao consegui EXECUTAR) sao separados de 1 (executou e
# retornou 1), e so os dois primeiros derrubam o degrau.
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

# 4. explorer.exe (WSL2 sem 'wslu') -- ver as DUAS armadilhas de codigo de
# saida no cabecalho: 1 nao e' falha, 126/127 sao.
if command -v explorer.exe >/dev/null 2>&1 && command -v wslpath >/dev/null 2>&1; then
    CAMINHO_WIN=$(wslpath -w "$ABS" 2>/dev/null)
    if [ -n "$CAMINHO_WIN" ]; then
        # A saida e' capturada (em vez de descartada) so para ecoar o erro do
        # kernel no diagnostico abaixo -- no caminho de sucesso ela e' lixo.
        SAIDA_EXPLORER=$(explorer.exe "$CAMINHO_WIN" 2>&1)
        CODIGO=$?
        if [ "$CODIGO" -ne 126 ] && [ "$CODIGO" -ne 127 ]; then
            echo "aberto via explorer.exe (navegador do Windows): $CAMINHO_WIN"
            exit 0
        fi
        printf "${AMARELO}open_browser:${SEM_COR} 'explorer.exe' existe mas NAO executa (codigo %s): %s\n" \
            "$CODIGO" "${SAIDA_EXPLORER:-sem saida}" >&2
        printf "  O interop Windows<->WSL esta desligado -- nenhum .exe roda nesta VM.\n" >&2
        printf "  Confira com: ls /proc/sys/fs/binfmt_misc/WSLInterop\n" >&2
        printf "  Restaurar:   sudo systemctl restart systemd-binfmt\n" >&2
        printf "  Se nao voltar:\n" >&2
        printf "    sudo sh -c 'echo \":WSLInterop:M::MZ::/init:P\" > /proc/sys/fs/binfmt_misc/register'\n" >&2
        printf "  (instalar 'wslu' NAO resolve -- o 'wslview' tambem depende do interop.)\n" >&2
    fi
fi

printf "${AMARELO}open_browser:${SEM_COR} nenhum abridor funcionou (\$BROWSER/xdg-open/wslview/explorer.exe) -- abra manualmente: %s\n" "$URL" >&2
printf "  Linux nativo (e WSL2 com WSLg): sudo apt install xdg-utils firefox\n" >&2
printf "  WSL2 pelo navegador do Windows: sudo apt install wslu (exige interop vivo)\n" >&2
exit 1
