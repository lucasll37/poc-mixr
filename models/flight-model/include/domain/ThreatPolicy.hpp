#pragma once

#include "domain/FlightCommand.hpp"
#include "domain/TerrainFloor.hpp"

namespace domain {

// Contato "cru" entregue pela percepcao -- de novo sem nenhum tipo do MIXR:
// quem traduz Track -> ThreatContact e o BtBehavior, na fronteira.
struct ThreatContact
{
   double rangeM{};
   double relBearingDeg{};   // -180..180, relativo ao nariz da aeronave
   double deltaAltM{};       // positivo = contato acima
};

struct EvasionLimits
{
   double breakTurnDeg{110.0};      // quanto virar para longe do contato
   double climbM{250.0};            // quanto subir/descer para desconflitar
   double dashSpeedKts{160.0};      // velocidade durante a evasao
   double holdSeconds{30.0};        // quanto a manobra continua valendo sem contato
   double terrainClearanceM{500.0}; // folga minima sobre o terreno no desconflito
};

//------------------------------------------------------------------------------
// ThreatPolicy -- a manobra de evasao, como MANOBRA e nao como reflexo.
//
// Regra: virar para o lado oposto ao contato e desconflitar em altitude no
// sentido contrario ao dele. Pura, deterministica e testavel sem simulacao.
//
// DUAS CORREÇÕES QUE VIERAM DE OBSERVAR O VOO NO TACVIEW (as aeronaves
// voavam "batendo asa", oscilando +-25 graus com periodo de ~24 s):
//
//  1) O ALVO E FIXADO NA ENTRADA DA MANOBRA, nao recalculado a cada tick.
//     A versao anterior devolvia 'meu_rumo + 110 graus' a cada decisao: um
//     setpoint que fugia na mesma velocidade em que a aeronave girava, entao
//     a curva nunca terminava. O mesmo valia para a altitude
//     ('minha_altitude - 400 m' a cada tick, uma descida sem fim). Agora
//     update() calcula o comando UMA vez, quando a manobra comeca, e o
//     mantem: o piloto automatico tem para onde convergir.
//
//  2) O RUMO DE FUGA E RELATIVO AO CONTATO, nao ao proprio nariz. O alvo sai
//     da MARCACAO ABSOLUTA do contato deslocada pela quebra -- que e o que
//     "fugir dele" quer dizer. Antes a marcacao do contato so escolhia o
//     SENTIDO da curva; para onde ela terminava dependia de onde a aeronave
//     ja estava apontando.
//
// E UMA HISTERESE, que e o que de fato mata a oscilacao:
//
//     engaged() continua true por 'holdSeconds' DEPOIS de o contato sumir.
//     Sem isso, a quebra tirava o intruso do setor do radar (+-30 graus,
//     contra uma quebra de 110), a pista sumia no mesmo instante, o ramo de
//     apoio da arvore assumia e trazia a aeronave de volta -- que reaquisita
//     e quebra de novo. Os dois ramos comandam sentidos opostos sobre o
//     mesmo objeto; sem tempo minimo de permanencia, alternam para sempre.
//
// O DESCONFLITO VERTICAL TEM PISO DE TERRENO (ver domain/TerrainFloor.hpp).
// "Desconflitar para baixo" so faz sentido enquanto houver espaco embaixo: a
// versao anterior descia 'climbM' a partir da altitude atual contra um piso
// absoluto de 200 m, que sobre a Serra do Mar fica 1500 m DENTRO da
// montanha. O piso agora e 'terreno + terrainClearanceM', com o piso
// absoluto como minimo -- e, como o alvo e fixado uma vez, o piso tambem e
// avaliado uma vez, na entrada da manobra.
//
// update() e chamado UMA vez por ciclo de decisao, antes de a arvore ticar.
//------------------------------------------------------------------------------
class ThreatPolicy
{
public:
   ThreatPolicy() = default;
   explicit ThreatPolicy(const EvasionLimits& limits) : limits_(limits) {}

   void setLimits(const EvasionLimits& limits) { limits_ = limits; }

   void reset();

   // Alimenta a politica com a percepcao do frame e envelhece a histerese.
   // 'ground' e a elevacao sob a aeronave NO MOMENTO DA ENTRADA na manobra --
   // e so nesse instante que ela e usada, porque e nesse instante que o alvo
   // e fixado (ver a nota acima).
   void update(double dt, bool hasContact, const ThreatContact& contact,
               double ownHeadingDeg, double ownAltM, const GroundReference& ground);

   // True enquanto a manobra vale -- com contato ou no arrasto da histerese.
   // E o que o ramo de evasao da arvore consulta.
   bool engaged() const              { return engaged_; }

   // True so quando ha contato de verdade. Separa "estou evadindo" de "estou
   // vendo o intruso": o alerta para os outros so faz sentido no segundo caso.
   bool contactLive() const          { return contactLive_; }

   // O comando fixado na entrada da manobra.
   const FlightCommand& command() const   { return cmd_; }

   double holdRemaining() const      { return holdTimer_; }

private:
   FlightCommand breakCommand(const ThreatContact& contact, double ownHeadingDeg,
                              double ownAltM, const GroundReference& ground) const;

   EvasionLimits limits_{};

   FlightCommand cmd_{};
   bool engaged_{};
   bool contactLive_{};
   double holdTimer_{};
};

} // namespace domain
