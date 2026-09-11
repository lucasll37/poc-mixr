#pragma once

namespace domain {

//------------------------------------------------------------------------------
// LaunchPolicy -- envelope de disparo, como regra pura (sem MIXR, sem
// BehaviorTree.CPP). Mesmo molde de domain::ThreatPolicy/geometry: decidir
// SE um contato esta dentro do envelope de lancamento e' geometria pura
// sobre alcance/marcacao relativa que a percepcao ja calculou (ver
// domain::WorldView::contactRangeM/contactRelBearingDeg) -- nao ha estado
// que sobreviva entre frames aqui (ao contrario de ThreatPolicy, que tem
// histerese: lancar nao precisa "continuar valendo" depois que o contato
// sai do envelope, so' checar de novo a cada tick).
//------------------------------------------------------------------------------
struct LaunchEnvelope
{
   double minRangeM{500.0};
   double maxRangeM{9000.0};
   double coneDeg{45.0};   // |marcacao relativa ao nariz| <= coneDeg
};

// true se o contato, no alcance e marcacao relativa dados, esta dentro do
// envelope de lancamento.
bool inLaunchEnvelope(const LaunchEnvelope& env, double rangeM, double relBearingDeg);

} // namespace domain
