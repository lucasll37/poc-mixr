#pragma once

#include <string>
#include <vector>

namespace mixr {
namespace simulation { class Station; }
}

//------------------------------------------------------------------------------
// Descoberta da ARVORE DE COMPONENTES REAL da simulacao MIXR em execucao --
// para a aba "Componentes" (F6, ver app/ComponentTreePanel.hpp e
// app/DashboardLoop.cpp).
//
// mixr::base::Component::getComponents() ja devolve, em runtime, a lista de
// filhos declarada pelo slot 'components:' de qualquer objeto -- e como
// Station/WorldModel(Simulation)/Player/Gimbal/SensorMgr/OnboardComputer/
// UbfArbiter/etc SAO Component, descer recursivamente por ela basta pra
// reconstruir a arvore de verdade que o .epp descreveu (dynamicsModel,
// pilot, sensores, obc, agent, e dentro deles o resto -- ver o cabecalho de
// mixr::base::Component.hpp).
//
// MAS NEM TUDO passa por 'components:'. Investigado lendo o fonte do
// framework (contexts/src/mixr/src/simulation/Simulation.cpp e
// .../Station.cpp) antes de escrever qualquer heuristica: 'players'
// (Simulation), e 'simulation'/'dataRecorder'/'ioHandler'/'networks'
// (Station) sao guardados em membros PROPRIOS (safe_ptr/ponteiro cru),
// nunca passados para Component::processComponents() -- exatamente a mesma
// razao pela qual TacviewOutput::resolveInfo() nao alcanca a Station por
// findContainerByType() (ver o comentario correspondente no CLAUDE.md,
// secao xtacview). Sem tratar isso a parte, a arvore desta aba mostraria
// SO o que estiver dentro de 'components:' de cada objeto -- no cenario de
// producao isto e SO o PluginLoader da Station, zero players. Por isso
// discoverComponentTree() acrescenta estes quatro (mais 'players', do lado
// da Simulation/WorldModel) como filhos SINTETICOS, usando os getters
// publicos (getPlayers()/getSimulation()/getDataRecorder()/getIoHandler()/
// getNetworks()) em vez de reimplementar a travessia. Dali pra baixo (um
// Player, um DataRecorder, um OutputHandler) a recursao volta a ser
// generica via getComponents().
//------------------------------------------------------------------------------
namespace app {

// Fase/chamada onde o no PROVAVELMENTE participa -- ver o comentario grande
// de estimatePhase() em ComponentTreeQuery.cpp. NAO E MEDIDO: e um PALPITE
// por heuristica sobre o NOME DO SLOT e a CLASSE do objeto, porque o MIXR e
// dependencia binaria (nao instrumentavel sem recompilar o framework, fora
// de cogitacao -- ver o topo do CLAUDE.md). Isto e o UNICO dado nesta aba
// que nao e observado ao vivo -- todo o resto do dashboard (xboard,
// contadores de instancia, timing) sempre mostrou dado real, entao a UI
// (ver app/ComponentTreePanel.hpp) tem de deixar essa diferenca obvia, nao
// so este comentario.
enum class EstimatedPhase {
   Unknown,             // sem heuristica aplicavel (slot/classe nao reconhecidos)
   Structural,          // Station/WorldModel/Player -- orquestra as fases, nao "roda" numa so
   DynamicsPhase0,      // dynamicsModel -- fase 0 do frame T/C
   SensorPhase1And2,    // antena/gimbal/sensor/datalink -- fases 1 (transmit) e 2 (receive)
   DecisionPhase3,      // pilot/Autopilot/BtBehavior/FlightAgentTC/RLBridgeBehavior/UbfArbiter --
                        // fase 3 do frame T/C (agente que decide DENTRO do frame)
   DecisionBackground,  // SimAgent -- decide em updateData(), fora do frame T/C (ver
                        // ubf::Agent::updateData() no CLAUDE.md, secao xclock)
   Background,          // dataRecorder/ioHandler/networks -- updateData(), thread de fundo
};

std::string phaseLabel(EstimatedPhase phase);

// O bit de uma fase dentro de ComponentTreeNode::subtreePhaseMask.
unsigned int phaseBit(EstimatedPhase phase);

// true para toda fase que NAO seja 'Structural' -- usado so pra decidir se
// o card de detalhe mostra o aviso "(estimado)"; 'Structural' tambem e uma
// inferencia (nenhuma classe se autodeclara "eu sou estrutural"), mas e
// inferencia por TIPO C++ exato (Station/Simulation/Player), nao por nome
// de slot/substring -- risco de erro bem menor, por isso fica fora do aviso
// mais forte.
bool isHeuristicPhase(EstimatedPhase phase);

// Um par "rotulo: valor" de estado VIVO, lido do PROPRIO objeto MIXR por
// getter publico no momento da descoberta -- ao contrario de
// EstimatedPhase (que e palpite), tudo aqui e FATO observado. E texto ja
// formatado, e nao um numero, de proposito: cada classe tem uma unidade
// propria (graus, pes, nos, Hz, hertz, nm) e quem sabe qual e a certa e o
// ponto que leu o getter, nao o painel que desenha.
struct ComponentStateField
{
   std::string label;
   std::string value;
};

struct ComponentTreeNode
{
   // Caminho ESTAVEL do no dentro da arvore ("/simulation/players/falcon1/
   // dynamicsModel"), montado na descoberta. Existe porque a arvore e
   // redescoberta a cada redesenho (ver discoverComponentTree() abaixo) e o
   // INDICE de um no muda quando algo nasce/some no meio dela -- um missil
   // liberado, um fantasma DIS -- ou quando um galho e retraido. Selecao e
   // conjunto de retraidos sao guardados por esta chave, nunca por indice.
   std::string nodeKey;

   std::string slotName;    // Pair::slot() -- nome do filho no EDL; "" na raiz
   std::string className;   // nome de classe C++ mais derivado (RTTI demangled)

   // mixr::models::Player (ou uma subclasse) -- so entao 'playerId' e
   // AbstractPlayer::getID() de verdade; card de detalhe usa isto pra
   // decidir se busca xboard::Readout (ver app/ComponentTreePanel.cpp).
   bool isPlayer{};
   int playerId{-1};

   EstimatedPhase phase{EstimatedPhase::Unknown};

   // Bitmask de todas as EstimatedPhase presentes na subarvore deste no,
   // ele proprio incluido. Existe porque o desenho precisa acender o caminho
   // da recursao ate quem participa da fase corrente MESMO quando o galho
   // esta retraido -- ai os participantes nao estao no layout, e sem esta
   // mascara o caminho simplesmente nao apareceria (medido: com os falcons
   // fechados, a fase 3 nao acendia nada). Calculada de baixo para cima na
   // descoberta, em O(n).
   unsigned int subtreePhaseMask{};

   // Estado VIVO do objeto, lido por getter publico -- ver
   // captureLiveState() em ComponentTreeQuery.cpp para a lista por classe.
   // Vazio quando a classe nao e nenhuma das reconhecidas (o card de
   // detalhe diz isso explicitamente, em vez de fingir que nao ha estado).
   std::vector<ComponentStateField> state;

   std::vector<ComponentTreeNode> children;
};

// Descobre a arvore REAL a partir da Station (a raiz de tudo neste app --
// ver app/StationBuilder.hpp). nullptr devolve um no vazio (className
// "?"), nunca crasha. Barato o bastante (dezenas de nos num cenario de
// producao) pra chamar a cada redesenho, no molde de
// app::makeTerrainSampler()/discoverPlayers() -- nao ha cache aqui de
// proposito: um missil liberado ou um fantasma DIS que chega pela rede
// aparecem/somem sozinhos, sem precisar de invalidacao manual.
ComponentTreeNode discoverComponentTree(mixr::simulation::Station* station);

} // namespace app
