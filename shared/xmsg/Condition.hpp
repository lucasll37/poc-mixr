#ifndef __xmsg_Condition_H__
#define __xmsg_Condition_H__

#include "mixr/base/Object.hpp"

#include "xmsg/FieldCatalog.hpp"
#include "xmsg/Snapshot.hpp"

#include <string>

namespace mixr {
namespace base { class String; }

namespace xmsg {

//------------------------------------------------------------------------------
// Class: Condition
//
// Description: A base das condicoes de disparo de uma mensagem. E a fronteira
//              entre o EDL e as regras puras de rules/ -- nenhuma subclasse
//              implementa deteccao aqui: todas delegam.
//
// Slots:
//    field   <String>   ! nome do campo do catalogo (obrigatorio)
//
// ESTADO E POR PLAYER, E MORA NA CONDICAO. O objeto Condition e UM so, vindo
// do .edl, mas cada aeronave tem a sua histerese, o seu ultimo valor emitido e
// a sua janela. Por isso evaluate() recebe um 'slot' -- o indice estavel que o
// MsgReport atribuiu aquele player -- e cada subclasse dimensiona seu vetor de
// estado em prepare(). Depois disso nao ha mais alocacao.
//------------------------------------------------------------------------------
class Condition : public base::Object
{
   DECLARE_SUBCLASS(Condition, base::Object)

public:
   Condition();

   // Resolve o nome do campo e dimensiona o estado. Devolve false (com
   // LOG(ERROR)) se algo nao fecha -- e o .edl e recusado, nunca ignorado.
   virtual bool prepare(int maxPlayers);

   // true APENAS na borda em que a condicao passa a valer.
   virtual bool evaluate(double dt, const Snapshot& snap, int slot) = 0;

   virtual void resetSlot(int slot) = 0;

   Group group() const           { return group_; }
   int fieldIndex() const        { return index_; }
   const std::string& fieldName() const { return fieldName_; }

protected:
   // Le o campo deste snapshot. 'valid' sai false quando o grupo do campo nao
   // vale para este player -- e a regra que impede alarme falso no fantasma
   // que chega por DIS sem dynamicsModel.
   double read(const Snapshot& snap, bool& valid) const;

   Dim dim() const               { return dim_; }
   int slots() const             { return slots_; }

private:
   bool setSlotField(const base::String* const);

   std::string fieldName_;
   int index_{-1};
   Group group_{};
   Dim dim_{Dim::None};
   int slots_{};
};

} // namespace xmsg
} // namespace mixr

#endif
