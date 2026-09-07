#pragma once

namespace mixr {
namespace xmsg {
namespace rules {

//------------------------------------------------------------------------------
// A tolerancia com que todo acumulador de tempo simulado deste subsistema
// compara contra o seu limiar.
//
// POR QUE ELA PRECISA EXISTIR -- medido, nao suposto:
//
//    soma de 0.1  dez vezes       = 0.9999999999999999   (fica ABAIXO de 1.0)
//    soma de 0.02 cinquenta vezes = 1.0000000000000004   (fica ACIMA  de 1.0)
//
// Nem 0.1 nem 0.02 sao representaveis em binario, e os dois sao exatamente os
// dt deste repositorio: 10 Hz no laco de tempo real, 50 Hz em -deterministic.
// O erro da soma nao so existe como MUDA DE SINAL entre os dois modos.
//
// Sem tolerancia, um 'hold: ( Seconds 1.0 )' arma no passo 50 a 50 Hz e so no
// passo 11 a 10 Hz -- quem escreveu "1 segundo" recebe 1,0 s num modo e 1,1 s
// no outro, e nada no codigo diz isso. O sintoma seria atribuido ao modelo, ou
// ao proprio limiar, nunca a soma.
//
// 1e-9 s e ~um bilionesimo do menor dt em uso: grande o bastante para cobrir a
// deriva acumulada em qualquer corrida plausivel (o erro por passo e da ordem
// de 1e-17), e pequeno o bastante para nao mover nenhuma borda de verdade.
//
// A mesma classe de problema existe em domain::ThreatPolicy
// ('holdTimer_ -= dt; if (holdTimer_ <= 0.0)'), so que la o '<=' faz o erro
// cair para o lado seguro -- nao ha bug hoje, mas e o mesmo terreno.
//------------------------------------------------------------------------------
constexpr double TIME_TOL{1e-9};

// 'acumulado' ja alcancou 'limiar'? Robusto a deriva de soma nos dois sentidos.
inline bool reached(const double acumulado, const double limiar)
{
   return acumulado + TIME_TOL >= limiar;
}

} // namespace rules
} // namespace xmsg
} // namespace mixr
