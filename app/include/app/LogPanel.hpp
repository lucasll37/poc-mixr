#pragma once

#include "xlog/Log.hpp"

#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A aba "Log": as ultimas linhas emitidas por shared/xlog (LOG(NIVEL) <<
// ...), uma por linha, com carimbo de hora, nivel colorido e a mensagem.
//
// A FONTE e o buffer em memoria de mixr::xlog (ver o bloco "BUFFER EM
// MEMORIA" em shared/xlog/Log.hpp), nao um tail do arquivo. Como 'xlog' e
// uma shared_library() -- ha UMA copia no processo --, o que o MODELO
// registra de dentro do .so aberto por dlopen cai no mesmo buffer que o do
// host e aparece aqui sem nenhuma ponte extra.
//
// E o MODELO que produz este conteudo, nao o app: quem escreve e
// models/player/A4/src/ubf/FlightAction.cpp (transicao de comportamento,
// alerta tatico, lancamento de missil, batimento de decisoes) e
// ubf/BtBehavior.cpp (falha ao carregar a arvore). O app nao chama
// LOG(...) em lugar nenhum -- ele so EXIBE. O unico ponto em que ele mexe
// no xlog e xlog::setConsoleEnabled(false) no inicio de runDashboard(),
// porque o FTXUI e dono do terminal (ver o comentario la).
//
// Este arquivo e PURO: recebe as entradas ja copiadas e devolve Element.
// Quem copia do buffer e quem cuida da rolagem/filtro e app/
// DashboardLoop.cpp, que e quem tem a vida do laco de eventos -- mesma
// divisao de FleetPanel/MemoryPanel/BackgroundPanel.
//------------------------------------------------------------------------------
namespace app {

// Largura fixa de cada coluna -- mesmo raciocinio de app/FleetPanel.hpp
// (alinhamento tabular de verdade): cada campo em 'size(WIDTH, EQUAL, N)',
// nao texto colado com padding. A mensagem NAO tem largura fixa: e a
// ultima coluna e leva o que sobrar.
const int kColLogSeq{7};
const int kColLogTime{14};
const int kColLogLevel{9};

// Filtro por nivel MINIMO -- 'DEBUG' mostra tudo, 'ERROR' so os erros. A
// ordem de Level (DEBUG < INFO < WARNING < ERROR) e a do enum, entao a
// comparacao e direta.
bool passesLevelFilter(mixr::xlog::Level level, mixr::xlog::Level minLevel);

// Proximo nivel do ciclo do filtro (ERROR volta pra DEBUG) -- usado pela
// tecla/botao '[f]'.
mixr::xlog::Level nextLevelFilter(mixr::xlog::Level minLevel);

// Texto plano de uma linha -- usado como rotulo de fallback do ftxui::Menu
// em DashboardLoop.cpp (a versao colorida vem de renderLogRow(), no
// transform; mesmo padrao de app/FleetPanel.hpp).
std::string logRowText(const mixr::xlog::Entry& e);

ftxui::Element renderLogRow(const mixr::xlog::Entry& e, bool focused);

// Cabecalho fixo (fora do Menu, que rola) nomeando cada coluna -- mesmo
// motivo de renderEntityListHeader() na aba Players: sem ele, "seq" e
// "hora" sao duas colunas de numeros sem nome nenhum.
ftxui::Element renderLogListHeader();

} // namespace app
