#ifndef __xmsg_MsgReport_H__
#define __xmsg_MsgReport_H__

#include "mixr/base/Object.hpp"

#include "xmsg/Snapshot.hpp"
#include "xmsg/rules/EmitGate.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace base { class Identifier; class Number; class PairStream; class String; }

namespace xmsg {

class Condition;
class RecordWriter;

//------------------------------------------------------------------------------
// Class: MsgReport
//
// Description: UMA mensagem configuravel -- quem, o que, e quando.
//
// Factory name: MsgReport
//
// Slots:
//    name      <String|Identifier> ! nome da mensagem (obrigatorio)
//    players   <PairStream>        ! nomes; vazio = todos
//    labels    <PairStream>        ! rotulos de texto: player side mode track
//    fields    <PairStream>        ! campos do catalogo (obrigatorio)
//    when      <PairStream>        ! condicoes; ausente = mensagem periodica
//    match     <Identifier>        ! "any" (default) ou "all" sobre 'when'
//    every     <Time>              ! intervalo minimo entre emissoes
//
// SEM 'when:' a mensagem e PERIODICA (sai todo ciclo, limitada por 'every:').
// Com 'when:', sai na BORDA -- no instante em que o casamento passa a valer.
//
// O ESTADO E POR PLAYER. Este objeto e um so, vindo do .edl, mas cada aeronave
// tem a sua histerese e o seu piso de emissao. Por isso tudo aqui recebe um
// 'slot' -- o indice estavel que o MsgFeed atribuiu aquele player.
//------------------------------------------------------------------------------
class MsgReport : public base::Object
{
   DECLARE_SUBCLASS(MsgReport, base::Object)

public:
   MsgReport();

   bool prepare(int maxPlayers);

   const std::string& name() const     { return name_; }
   unsigned groupMask() const          { return groupMask_; }
   bool wantsPlayer(const char* playerName) const;

   // true = emitir para este player neste ciclo.
   bool evaluate(double dt, const Snapshot& snap, int slot);

   void render(RecordWriter& w, double t, const Snapshot& snap) const;

   void resetSlot(int slot);

   long deferred() const;

private:
   bool setSlotName(const base::String* const);
   bool setSlotNameId(const base::Identifier* const);
   bool setSlotPlayers(const base::PairStream* const);
   bool setSlotLabels(const base::PairStream* const);
   bool setSlotFields(const base::PairStream* const);
   bool setSlotWhen(base::PairStream* const);
   bool setSlotMatch(const base::Identifier* const);
   bool setSlotEvery(const base::Number* const);

   std::string name_;
   std::vector<std::string> players_;      // vazio = todos
   std::vector<std::string> labels_;
   std::vector<std::string> fieldNames_;

   std::vector<int> fields_;               // resolvidos em prepare()
   unsigned groupMask_{};
   bool matchAll_{};
   double every_{};

   base::PairStream* when_{};              // mantem as condicoes vivas
   std::vector<Condition*> conds_;         // vista plana, montada em prepare()

   std::vector<rules::EmitGate> gates_;
};

} // namespace xmsg
} // namespace mixr

#endif
