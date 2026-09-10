#!/usr/bin/env bash
# Guarda: a THREAD DE DESENHO do ./app nao pode tocar o grafo VIVO do MIXR.
#
# POR QUE ESTE INVARIANTE EXISTE
# ------------------------------
# 'app/src/app/DashboardLoop.cpp' roda em duas threads: 'simThread' (que chama
# station->updateData() e captura o DashboardState) e a thread PRINCIPAL, que
# roda o Loop() do FTXUI -- todo lambda de Renderer/CatchEvent esta nela.
#
# Enquanto isso, a thread de tempo critico NATIVA e o pool de 'numTcThreads'
# criam e destroem objetos MIXR. Percorrer o grafo vivo da thread de desenho,
# sem nenhuma sincronizacao com o frame, tem duas saidas ruins:
#
#   * ler pagina ja liberada                              -> SIGSEGV
#   * ver refCount==0 dentro de safe_ptr::getRefPtr() ->
#     Referenced::ref() -> 'throw new ExpInvalidRefCount()' -> SIGABRT
#     (e o MIXR lanca um PONTEIRO fora de std::exception, entao so
#      'catch (...)' pegaria)
#
# A correcao foi mover TODA leitura de objeto MIXR para 'simThread', dentro de
# captureState(): o DashboardState atravessa o 'stateMutex' por valor e nao
# carrega ponteiro nenhum do framework. Esta guarda trava isso.
#
# O que ela procura: chamadas de MIXR vivo depois do ponto em que os lambdas de
# UI comecam. A busca e por texto de proposito -- e barata, roda em toda
# execucao da suite, e o modo de falha que ela impede (alguem acrescentar um
# 'station->' novo dentro do Renderer) e exatamente textual.
set -u

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ALVO="$RAIZ/app/src/app/DashboardLoop.cpp"

if [ ! -f "$ALVO" ]; then
   echo "check_ui_thread_sem_mixr: nao achei $ALVO" >&2
   exit 1
fi

# A regiao de UI vai do primeiro Renderer/CatchEvent ate o 'screen.Loop()'.
#   * Antes do inicio: declaracoes e o corpo de 'simThread' -- ela PODE (e deve)
#     tocar o MIXR, e o trabalho dela.
#   * Depois do Loop(): o encerramento, que roda depois de 'simThread.join()' e
#     de quiesceTimeCritical() -- ali nao ha mais outra thread da aplicacao, e
#     o 'station->updateData()' final e legitimo (drena o gravador uma ultima
#     vez). Sem este limite a guarda acusa esse caso e vira ruido.
INICIO="$(grep -n 'Renderer(\|CatchEvent(' "$ALVO" | head -1 | cut -d: -f1)"
FIM="$(grep -n 'screen\.Loop(' "$ALVO" | head -1 | cut -d: -f1)"
if [ -z "$INICIO" ] || [ -z "$FIM" ] || [ "$FIM" -le "$INICIO" ]; then
   echo "check_ui_thread_sem_mixr: nao achei a regiao de UI (Renderer/CatchEvent .. screen.Loop)" >&2
   echo "  -- o arquivo mudou de forma; reveja esta guarda em vez de desliga-la." >&2
   exit 1
fi

# 'clockStation->' fica de FORA: ClockStation e a classe deste repositorio, com
# API explicitamente thread-safe para este uso (atomicos + o handshake de
# requestTcStop/waitForTcQuiesced). O proibido e o grafo do FRAMEWORK.
PADRAO='station->|worldModel->|discoverComponentTree|makeTerrainSampler|->getPlayers\(|->getComponents\(|->getNetworks\('

ACHADOS="$(sed -n "${INICIO},${FIM}p" "$ALVO" \
   | grep -nE "$PADRAO" \
   | grep -v 'clockStation->' \
   | grep -vE '^[0-9]+:[[:space:]]*//' || true)"

if [ -n "$ACHADOS" ]; then
   echo "FALHA: a thread de DESENHO voltou a tocar o grafo vivo do MIXR." >&2
   echo "  (numeracao relativa ao inicio da regiao de UI, linha $INICIO de" >&2
   echo "   app/src/app/DashboardLoop.cpp; a regiao termina no screen.Loop() da $FIM)" >&2
   echo "$ACHADOS" | sed 's/^/    /' >&2
   echo >&2
   echo "  Leia o cabecalho deste script. O caminho certo e capturar o dado em" >&2
   echo "  captureState() (app/src/app/DashboardState.cpp), na 'simThread', e" >&2
   echo "  transporta-lo por valor no DashboardState." >&2
   exit 1
fi

echo "ok: a thread de desenho nao toca objeto vivo do MIXR"
