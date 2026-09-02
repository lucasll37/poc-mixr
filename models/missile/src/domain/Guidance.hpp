#pragma once

namespace domain {

//------------------------------------------------------------------------------
// Guidance -- a lei de guiagem do misseil, PURA (sem MIXR, sem JSBSim).
//
// E "perseguicao pura" (pure pursuit): aponta o nariz para o alvo, banco
// proporcional ao erro de rumo, profundor proporcional ao erro de elevacao.
// NAO e navegacao proporcional de verdade -- essa exigiria estimar a taxa de
// variacao da linha de visada (LOS rate) de forma filtrada, contra ruido de
// medicao; para um alvo unico, nao manobrando, dentro do alcance curto desta
// demo, perseguicao pura converge e e trivial de explicar. Documentado aqui
// como simplificacao deliberada, nao descoberta later.
//
// Entra tudo em NED, metros e graus (mesma convencao do resto do repositorio
// -- ver domain/WorldView.hpp do flight). Sai comando NORMALIZADO
// (-1..1), pronto para Player::setControlStickRollInput()/PitchInput(), que
// e a mesma faixa que o joystick fisico ja usa (shared/xjoystick).
//------------------------------------------------------------------------------
struct GuidanceCommand
{
   double rollNorm{};    // -1 (banca esquerda) .. 1 (banca direita)
   double pitchNorm{};   // -1 (cabra) .. 1 (pica), ver sinal em Player::setControlStickPitchInput
};

struct GuidanceGains
{
   double headingGainDeg{45.0};      // erro de rumo que satura o comando de banco
   double pitchGainDeg{30.0};        // erro de elevacao que satura o comando de profundor

   // Termo DERIVATIVO (taxa), no mesmo comando -- ver o "porque" abaixo.
   // Graus/segundo de taxa propria que cancela um comando de erro unitario.
   double rollRateGainDps{60.0};
   double pitchRateGainDps{60.0};
};

//------------------------------------------------------------------------------
// relNorthM/relEastM/relDownM: vetor do missil ATE o alvo, NED, metros.
// ownHeadingDeg/ownPitchDeg: atitude atual do missil, graus.
// ownRollRateDps/ownPitchRateDps: taxa PROPRIA de rolagem/arfagem, graus/s
// (corpo) -- ver Player::getAngularVelocities().
//
// ARMADILHA CONFIRMADA RODANDO: um controlador so-proporcional (erro de
// angulo -> comando), mesmo com o comando limitado em TAXA DE VARIACAO (ver
// GuidedMissile::guide()), diverge nesta aeronave -- medido indo de
// oscilacoes de poucos graus a mais de 100 graus de arfagem/banco em menos
// de 0.3 s, com a velocidade escalando para milhares de nos (o integrador
// de passo fixo do JSBSim, 0.02 s, nao acompanha um modo de arfagem tao
// pouco amortecido quanto o que sobra depois de reduzir a inercia do
// aim1.xml). O termo de TAXA aqui (proporcional-derivativo, nao so
// proporcional) e o amortecimento que falta: ele cancela a rotacao antes
// que ela ultrapasse o alvo, em vez de deixar so a saturacao do comando
// (que limita AMPLITUDE, nao FREQUENCIA) conter a oscilacao.
//------------------------------------------------------------------------------
GuidanceCommand pursuit(double relNorthM, double relEastM, double relDownM,
                        double ownHeadingDeg, double ownPitchDeg,
                        double ownRollRateDps, double ownPitchRateDps,
                        const GuidanceGains& gains = GuidanceGains{});

} // namespace domain
