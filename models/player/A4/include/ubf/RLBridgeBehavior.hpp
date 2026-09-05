#ifndef __xnative_ubf_RLBridgeBehavior_H__
#define __xnative_ubf_RLBridgeBehavior_H__

#include "mixr/base/ubf/AbstractBehavior.hpp"

namespace mixr {
namespace xnative {

//------------------------------------------------------------------------------
// Class: RLBridgeBehavior
//
// Description: A DECISAO vem de FORA do processo MIXR -- de um agente de RL
//              em Python, via bindings pybind11 (ver src/rl/bindings/). Em vez de
//              ticar uma arvore (BtBehavior), devolve o ultimo comando que a
//              ponte Python entregou, lido de shared/xrlbridge.
//
// Factory name: RLBridgeBehavior
//
// Slots: (nenhum) -- a configuracao vem de fora, por shared/xrlbridge, nao de
// EDL.
//
// POR QUE shared/xrlbridge E NAO UM PONTEIRO ESTATICO NESTA CLASSE: esta
// classe mora num .so aberto com dlopen (o plugin do modelo); a ponte
// pybind11 mora no EXECUTAVEL (src/rl/bindings), que NAO pode incluir headers
// deste modelo nem linkar contra o .so dele em tempo de compilacao -- e o
// invariante que tests/guard/check_host_opaco.sh trava ("o host nao pode
// conhecer o fonte do modelo"). shared/xrlbridge e uma shared_library() de
// verdade, do mesmo jeito e pelo mesmo motivo que shared/xboard::Board:
// escrita/leitura cruzam essa fronteira, e uma lib estatica daria a cada
// lado a sua propria copia do estado. Ver o cabecalho de
// shared/xrlbridge/RLBridge.hpp para o protocolo completo.
//
// V1 -- UM UNICO agente RL por processo (sem chave por player id -- ver o
// mesmo "porque" no cabecalho de RLBridge.hpp).
//------------------------------------------------------------------------------
class RLBridgeBehavior : public base::ubf::AbstractBehavior
{
   DECLARE_SUBCLASS(RLBridgeBehavior, base::ubf::AbstractBehavior)

public:
   RLBridgeBehavior();

   base::ubf::AbstractAction* genAction(const base::ubf::AbstractState* const state,
                                        const double dt) override;
};

} // namespace xnative
} // namespace mixr

#endif
