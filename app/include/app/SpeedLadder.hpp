#pragma once

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A escada de velocidade e a decisao de rotulo/cor do cabecalho de
// app/DashboardLoop.cpp, separadas do wiring com ClockStation/FTXUI --
// mesma logica de app/BreakpointController.hpp.
//
// De proposito FORA daqui: o bloqueio por breakpoint em modo rapido (ver
// 'fastRunToBreakpoint' em runDashboard() -- estado de OUTRO subsistema,
// ja isolado em BreakpointController) e a chamada real a
// ClockStation::setTimeScale()/setPaused() (efeito sobre o MIXR). Este
// arquivo so decide "qual e o proximo indice/rotulo/cor", nunca aplica
// nada.
//------------------------------------------------------------------------------
namespace app {

// Como o "tom" da cor do rotulo de velocidade e escolhido -- mapeamento
// 1:1 pras cinco cores que o cabecalho ja usa (Green/Yellow/Cyan/Red/
// Magenta), so que sem incluir nenhum header do FTXUI. O chamador faz a
// traducao (um switch de 5 casos).
enum class SpeedTone { Green, Yellow, Cyan, Red, Magenta };

struct SpeedDisplay
{
   std::string label;
   SpeedTone tone{SpeedTone::Green};
};

// Mesma escada de shared/xclock/TimeControls.cpp, de proposito -- ver o
// "porque" de app/DashboardLoop.cpp nao reusar xclock::TimeControls
// (termios vs. FTXUI) no cabecalho daquele arquivo.
std::vector<double> defaultSpeedLadder();

// Duas casas decimais abaixo de 1.0, nenhuma acima (ex.: "0.25x", "4x").
std::string formatSpeedScale(double scale);

// O bloco de decisao do cabecalho: qual rotulo/cor mostrar dado o estado
// atual. 'actualTimeScale' (a velocidade MEDIDA, tempo simulado / tempo de
// parede -- nao a nominal da escada) aparece ao lado do comandado sempre
// que 'timeScale' != 1.0, nao so durante um breakpoint em modo rapido: o
// pacing pode nao alcancar o nominal comandado (carga de CPU, teto do
// laco), e mostrar so o comandado escondia essa divergencia. Durante
// 'fastBreakpointRun' o nominal deixa de significar algo (o laco ignora o
// pacing por completo), entao o rotulo vira so "MAX (~Nx real)".
SpeedDisplay speedDisplay(bool fastBreakpointRun, bool paused, double timeScale,
                          double actualTimeScale);

// Estado + transicoes da escada.
class SpeedLadder
{
public:
   explicit SpeedLadder(std::vector<double> ladder = defaultSpeedLadder(), int realTimeIndex = 3);

   // Posiciona o indice no valor da escada MAIS PROXIMO de 'scale' --
   // usado uma vez no startup do dashboard, pra sincronizar com
   // clockStation->getTimeScale() (que pode nao ser exatamente um valor
   // da escada, se o cenario tiver sido carregado com outro timeScale).
   void seedFromScale(double scale);

   // Devolvem false (nao-op) se ja estao no limite da escada.
   bool accelerate();
   bool decelerate();
   void toRealTime();

   int index() const { return index_; }
   double scale() const { return ladder_[static_cast<std::size_t>(index_)]; }
   double maxScale() const { return ladder_.back(); }
   int realTimeIndex() const { return realTimeIndex_; }
   int size() const { return static_cast<int>(ladder_.size()); }

private:
   std::vector<double> ladder_;
   int realTimeIndex_;
   int index_;
};

} // namespace app
