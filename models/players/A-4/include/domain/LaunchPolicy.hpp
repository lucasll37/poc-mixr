#pragma once

namespace domain {

//------------------------------------------------------------------------------
// LaunchPolicy -- envelope de lancamento do missil, PURO e testavel sem
// simulacao (mesmo padrao de domain::ThreatPolicy).
//
// Uma unica questao: o contato esta a uma distancia e a um angulo em que faz
// sentido lancar? Nao decide POR SI SO -- quem decide e o no da arvore
// (bt/nodes/LaunchEnvelopeCondition.cpp), que tambem confere se ha arma
// disponivel (domain::WorldView::weaponReady). Separar os dois evita
// misturar "o alvo esta no envelope" com "temos arma" -- sao duas perguntas
// independentes.
//------------------------------------------------------------------------------
struct LaunchEnvelope
{
   double minRangeM{1500.0};
   double maxRangeM{9000.0};
   double coneDeg{45.0};       // meio-angulo do cone de lancamento, em torno do nariz
};

// 'relBearingDeg' e -180..180, relativo ao nariz da aeronave (mesma convencao
// de domain::ThreatContact::relBearingDeg).
bool inLaunchEnvelope(const LaunchEnvelope& env, double rangeM, double relBearingDeg);

} // namespace domain
