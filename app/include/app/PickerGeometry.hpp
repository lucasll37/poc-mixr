#pragma once

namespace app {
namespace pickergeometry {

//------------------------------------------------------------------------------
// A geometria PURA por tras da tela generica de selecao
// (app/ScenarioPickerScreen.cpp): quantas linhas o menu ocupa e a altura
// total da caixa, a partir do numero de itens. Nenhum tipo de FTXUI aqui.
//
// Promovido de um namespace anonimo dentro do .cpp -- mesmo motivo de
// app/MapGeometry.hpp: a regra ja era pura, so nunca tinha ficado visivel
// fora do arquivo. Sem isto, testar a decisao de clamp exigiria simular um
// screen.Loop() interativo de verdade (o resto de runPickerScreen() e
// FTXUI puro, bloqueante -- nao vale a pena tornar testavel).
//------------------------------------------------------------------------------

// A ALTURA DA LISTA nao pode ser um numero fixo cravado pra 3 entradas --
// quem chama isto e' '-folder <pasta>' (app/ScenarioFolder.hpp), com um
// numero de subpastas que so se sabe em runtime -- 2, 3 ou 30. Piso e teto
// evitam tanto uma caixa esticada por 1-2 itens quanto uma lista sem
// limite: acima do teto, ganha o mesmo 'vscroll_indicator | frame' que
// Players/Memoria/Log ja usam.
constexpr int kMinMenuLines{3};
constexpr int kMaxMenuLines{12};

// Largura fixa da caixa e altura reservada pra descricao (2 linhas cobrem
// a mais comprida vista ate hoje, ~80 caracteres, com folga na largura
// abaixo). As duas sao 'EQUAL', nao 'GREATER_THAN', de proposito: com
// 'GREATER_THAN' a caixa croscia/encolhia (e recentralizava) conforme a
// descricao de CADA item selecionado -- a tela "pulava" so de navegar.
constexpr int kWidth{76};
constexpr int kDescLines{2};

// Tudo no corpo da tela que NAO e o menu: titulo(1) + separador(1) +
// separador(1) + descricao(kDescLines) + separador(1) + rodape(1) +
// borda(2). Some 'menuLines' pra chegar na altura total.
constexpr int kChromeLines{1 + 1 + 1 + kDescLines + 1 + 1 + 2};

struct Geometry
{
   int menuLines{};
   int totalHeight{};
};

// 'itemCount' e' o numero de opcoes que a tela vai mostrar -- calculado
// UMA VEZ, na construcao da tela (a lista de itens nao muda durante a
// sessao do picker), entao fixar a altura a partir dele nao reintroduz o
// problema de "tela pulando" que motivou EQUAL em vez de GREATER_THAN.
Geometry computeGeometry(int itemCount);

} // namespace pickergeometry
} // namespace app
