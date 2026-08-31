#include "app/MetaObjectReport.hpp"

#include "ubf/AltitudeSafetyBehavior.hpp"
#include "ubf/BtBehavior.hpp"
#include "ubf/FlightAction.hpp"
#include "ubf/FlightState.hpp"
#include "xnative/AlertDatalink.hpp"
#include "xnative/TacticalAlert.hpp"

#include "xmsg/MsgFeed.hpp"
#include "xmsg/MsgReport.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Pair.hpp"
#include "mixr/base/String.hpp"

#include <iostream>

namespace app {

namespace {

template <typename T>
void reportClass()
{
   const mixr::base::MetaObject* const meta{T::getMetaObject()};
   if (meta == nullptr) return;

   std::cout << "meta=" << meta->getFactoryName()
             << " count=" << meta->count
             << " mc=" << meta->mc
             << " tc=" << meta->tc
             << std::endl;
}

} // namespace

void printMetaObjectReport()
{
   // As classes que o modelo aloca EM REGIME -- e onde um vazamento
   // apareceria. FlightAction e a mais exposta: uma por ciclo de decisao,
   // por aviao, entregue ao UBF, que precisa dar unref().
   reportClass<mixr::xnative::FlightAction>();
   reportClass<mixr::xnative::TacticalAlert>();

   // Estas nascem uma vez, no parse do EDL: 'count' tem de ficar parado.
   reportClass<mixr::xnative::FlightState>();
   reportClass<mixr::xnative::BtBehavior>();
   reportClass<mixr::xnative::AltitudeSafetyBehavior>();
   reportClass<mixr::xnative::AlertDatalink>();

   // Mensageria: nascem no parse do EDL e nao devem se multiplicar. O 'count'
   // aqui e o detector de clone acidental (BT::Tree e move-only, mas um
   // PairStream de slot copiado sem cuidado duplicaria os objetos).
   //
   // O que este relatorio NAO enxerga sao os blocos de estado por player do
   // MsgFeed -- eles ficam fora do ref-counting. Quem cobre isso e o campo
   // 'states' da mensagem interna msgHealth, comparado por
   // tests/memory/run_leak_test.py entre duas duracoes.
   reportClass<mixr::xmsg::MsgFeed>();
   reportClass<mixr::xmsg::MsgReport>();

   // Termometro geral do parser/EDL.
   reportClass<mixr::base::Pair>();
   reportClass<mixr::base::String>();
}

} // namespace app
