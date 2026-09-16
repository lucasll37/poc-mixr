#ifndef __events_PingMessage_H__
#define __events_PingMessage_H__

#include "mixr/base/Object.hpp"

#include <string>

namespace mixr {
namespace events {

//------------------------------------------------------------------------------
// Class: PingMessage
//
// Description: Carga util de um "ping" -- a prova de referencia deste
//              projeto de que um modelo pode EMITIR e TRATAR o proprio
//              evento (o caminho (b) de events/README.md, exatamente como
//              events::TacticalAlert, so que sem depender de nenhum
//              subsistema nativo como o Datalink). E' um mixr::base::Object
//              de verdade (ref-contado, com RTTI do framework) -- payload
//              NAO-nulo, com campos de verdade (nao os dois uint32 crus de
//              um REID_MARKER do recorder, que e' o motivo original de
//              events/ existir -- ver o cabecalho de libs/xlog/README.md
//              para o schema fechado do DataRecord.proto).
//
// Factory name: PingMessage
//
// Consumido por models/others/Beacon (src/plugin.cpp): toda instancia de
// ( Beacon ) emite este payload periodicamente para os DEMAIS players locais
// ativos (broadcast direto, Component::event() sobre getPlayers() -- ver
// events/README.md secao "As duas formas de despacho"), e toda instancia
// TAMBEM trata o evento -- e' o unico consumidor deste repositorio em que
// emissor e receptor sao a MESMA classe.
//
// Os campos sao deliberadamente CRUS: quem recebe decide o que fazer, o
// ping nao carrega nenhuma ordem -- mesmo raciocinio ja documentado no
// cabecalho de events::TacticalAlert.
//------------------------------------------------------------------------------
class PingMessage : public base::Object
{
   DECLARE_SUBCLASS(PingMessage, base::Object)

public:
   PingMessage();

   int getSenderId() const                    { return senderId; }
   const std::string& getSenderName() const   { return senderName; }
   unsigned int getSequence() const           { return sequence; }
   const std::string& getMessage() const      { return message; }

   void setSender(const int id, const std::string& name)  { senderId = id; senderName = name; }
   void setSequence(const unsigned int seq)                { sequence = seq; }
   void setMessage(const std::string& msg)                 { message = msg; }

private:
   int senderId{};
   std::string senderName;
   unsigned int sequence{};
   std::string message;
};

} // namespace events
} // namespace mixr

#endif
