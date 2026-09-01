#ifndef __xboard_Board_H__
#define __xboard_Board_H__

#include <string>

namespace mixr {
namespace xboard {

//------------------------------------------------------------------------------
// O QUADRO DE LEITURA -- a unica coisa que o host e o modelo compartilham.
//
// Uma unica questao: o que o dump e a linha de status precisam saber do modelo,
// por id de player. O player e o models::Aircraft NATIVO, que nao tem campo
// proprio para nada disto -- um quadro global por id resolve sem subclassear o
// Player so por causa de uma string.
//
//------------------------------------------------------------------------------
// POR QUE ESTA E A PRIMEIRA shared_library() DE shared/
//
// As outras cinco (xtacview, xclock, xjoystick, xlog, xmsg) sao
// static_library(). Esta NAO pode ser, e o motivo e estrutural:
//
//    quem ESCREVE aqui e o modelo, que mora num .so carregado com dlopen;
//    quem LE e o host, que e o executavel.
//
// Com uma lib estatica, cada lado ganharia a SUA PROPRIA copia dos mapas: o
// modelo escreveria num, o host leria do outro, e o dump sairia com 'bt=--' e
// 'dec=0' para sempre -- sem erro de link, sem aviso, sem sintoma alem do
// numero errado. E a armadilha registrada em
// contexts/BTCPP-CONTEXT.md:7262-7270 e no cabecalho de
// shared/xplugin/PluginAbi.hpp.
//
// E tambem a saida que shared/xplugin/README.md ja aponta como a honesta
// quando o plugin precisa de codigo compartilhado: "promover a peca necessaria
// a shared_library com SONAME", em vez de relaxar a regra de que um plugin so
// depende de mixr_dep + xplugin_abi_dep.
//------------------------------------------------------------------------------
// CONCORRENCIA: escrito nas threads de tempo critico (a atuacao do UBF roda
// la), lido no laco de background. Por isso todo acesso passa por um mutex.
//------------------------------------------------------------------------------

struct Readout
{
   std::string label{"--"};   // rotulo do comportamento que venceu o voto
   long decisions{};          // decisoes efetivamente ATUADAS
   int  threadTag{-1};        // thread do pool T/C que decidiu (-1 = nao se aplica)

   bool alertValid{};         // ha alerta tatico valido recebido?
   std::string alertSender;
   std::string alertContact;
   long sent{};               // alertas transmitidos por este player
   long received{};           // alertas recebidos
};

//--- escrita: SO o modelo chama -----------------------------------------------

void setBehaviorLabel(int playerId, const std::string& label);

// Conta DECISAO, nao candidatura: e chamada no ponto da atuacao, depois de o
// UbfArbiter ja ter escolhido o vencedor.
//
// Existe para o dump poder AFIRMAR -- e nao so imprimir -- que a decisao esta
// amarrada ao frame: em passo fixo, o avanco deste numero entre dois dumps tem
// de ser igual ao avanco do numero de frames, com 1, 2 ou 4 threads T/C.
void bumpDecisionCount(int playerId);

void setThreadTag(int playerId, int tag);
void setAlert(int playerId, bool valid, const std::string& sender, const std::string& contact);
void setDatalinkCounters(int playerId, long sent, long received);

//--- leitura: SO o host chama -------------------------------------------------

Readout get(int playerId);

} // namespace xboard
} // namespace mixr

#endif
