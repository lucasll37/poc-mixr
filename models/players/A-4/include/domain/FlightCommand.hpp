#pragma once

namespace domain {

// Comando de voo "de alto nivel" produzido pelas regras de negocio puras
// (patrulha, RTB, evasao) e consumido pelos nos da arvore de comportamento,
// que o traduzem para o dynamics model. Deliberadamente NAO conhece nenhum
// tipo do MIXR nem do BehaviorTree.CPP -- e so um DTO.
//
// Unidades explicitas no nome do campo: a armadilha classica e misturar
// pes/metros/nos (ver "Gotchas de unidades e de modelo" no CLAUDE.md).
struct FlightCommand
{
   double headingDeg{};    // rumo verdadeiro comandado (graus)
   double altitudeM{};     // altitude comandada (metros)
   double speedKts{};      // velocidade comandada (nos)

   // --- Manobra acrobatica: comanda o AILERON direto, em vez do rumo. ---
   //
   // Com rollOverride true, 'headingDeg' e IGNORADO pela atuacao: quem atua
   // desliga o heading hold do Autopilot (que e exatamente o que abre o
   // caminho do stick -- ver Autopilot::headingController(), que so repassa
   // setControlStickRollInput() ao dynamics model no ramo 'else' de
   // isHeadingHoldOn()) e manda 'rollStick' no lugar. 'altitudeM'/'speedKts'
   // continuam valendo: so UM eixo e liberado.
   //
   // Estes dois campos NAO entram no contrato de RL. libs/xrlbridge/
   // ObservationFields.hpp enumera os campos por NOME (XRLBRIDGE_ACTION_FIELDS,
   // XRLBRIDGE_ACTION_SIZE 3) -- campo novo aqui nao muda o tensor nem a
   // contagem, e nenhum .onnx ja treinado e invalidado.
   bool rollOverride{};    // true: pilota pelo aileron, nao pelo rumo
   double rollStick{};     // -1..1; esquerda(-) / direita(+). So vale com rollOverride
};

} // namespace domain
