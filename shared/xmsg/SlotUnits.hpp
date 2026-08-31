#pragma once

#include "xmsg/FieldCatalog.hpp"

namespace mixr {
namespace base { class Number; }

namespace xmsg {

//------------------------------------------------------------------------------
// SlotValue -- um limiar vindo do EDL, que pode chegar de duas formas.
//
// base::Distance, base::Angle e base::Time TODAS herdam de base::Number. Logo
// um slot declarado como base::Number aceita, no mesmo lugar:
//
//     by: 100              numero cru, na unidade documentada no nome do campo
//     by: ( Meters 100 )   objeto de unidade
//
// A validacao de dimensao NAO pode acontecer na hora de ler o slot: o 'field:'
// pode vir depois no .epp, e ai ainda nao se sabe qual dimensao esperar. Por
// isso o valor e capturado aqui com a sua natureza, e so e resolvido em
// prepare(), quando o campo ja esta conhecido. Ordem no .epp deixa de importar.
//------------------------------------------------------------------------------
struct SlotValue
{
   bool set{};
   Dim kind{Dim::None};   // None = numero cru; senao, a dimensao do objeto
   double value{};        // cru, ou ja convertido para metros / graus / segundos

   // Captura a natureza do objeto e ja normaliza a unidade canonica.
   bool capture(const base::Number* n);

   // Resolve contra a dimensao do campo. 'out' so e escrito em caso de sucesso.
   //   numero cru            -> aceito sempre
   //   unidade da dimensao X -> aceito se o campo for da dimensao X
   //   unidade de outra dim  -> RECUSADO (e o erro que se quer pegar)
   bool resolve(Dim fieldDim, double& out) const;
};

// Para slots que sao sempre tempo ('hold:', 'every:', 'window:'): aceita
// numero cru em segundos ou ( Seconds ... ) / ( MilliSeconds ... ).
bool captureSeconds(const base::Number* n, double& out);

} // namespace xmsg
} // namespace mixr
