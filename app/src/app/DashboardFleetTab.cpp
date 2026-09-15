#include "app/DashboardWiring.hpp"

#include "app/BehaviorTreeView.hpp"
#include "app/FleetPanel.hpp"

#include "xclock/ClockStation.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <utility>

//------------------------------------------------------------------------------
// A aba "Players" (F1): lista rolavel + detalhe da entidade selecionada --
// ver o cabecalho de app/DashboardLoop.cpp para o desenho geral (SETE ABAS,
// duas threads, CatchEvent unico).
//
// ESTE ARQUIVO TAMBEM E' DONO da infraestrutura de "card de detalhe" +
// "breakpoint de arvore de BT" que a aba Mapa REUSA (as duas abas
// compartilham 'selectedEntityIndex' desde sempre -- clicar numa entidade no
// Mapa e clicar nela na lista de Players sao a MESMA operacao). Por isso
// buildFleetTab() tem de ser chamado ANTES de buildMapTab() em
// runDashboard() -- ele e' quem preenche 'w.treeMenuMap'/'w.buildDetailPanel'/
// 'w.doArmBreakpoint'/etc, que buildMapTab() so' LE.
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;

// Uma linha da arvore de BT (ja achatada por app::flattenBehaviorTree) --
// 'isActiveLeaf' e a folha vencedora AGORA (o mesmo destaque que existia
// antes, quando a arvore era so um Element estatico); 'isSelected' e a
// escolha do USUARIO na lista (pra virar alvo de breakpoint); 'isBreakpoint'
// marca o no que ESTA armado.
Element renderBtLine(const BtTreeLine& line, const bool isActiveLeaf, const bool isSelected,
                     const bool isBreakpoint)
{
   Element e{text(line.display)};
   if (!line.leaf) e = e | dim;
   if (isActiveLeaf) e = e | bgcolor(Color::Blue) | color(Color::White) | bold;
   if (isBreakpoint) e = hbox({e, text(" [BP]") | color(Color::Red) | bold});
   if (isSelected) e = e | inverted;
   return e;
}

} // namespace

Component buildFleetTab(DashboardWiring& w)
{
   // ---- breakpoint de arvore de BT -- "marcar um estado da bt de um dado
   // elemento e rodar a simulacao ate que aquele no seja atingido,
   // devolvendo a simulacao pausada" ----
   //
   // 'fast' escolhe a velocidade: false = "a velocidade que eu decidir" (so
   // arma o observador -- 'simThread' ja checa a condicao toda amostra,
   // rodando ou pausada, em qualquer escala de tempo; so garante que NAO
   // esta pausada, senao nunca chegaria a lugar nenhum); true = "maxima
   // possivel" (liga 'fastRunToBreakpoint', que faz 'simThread' pular o
   // pacing de parede).
   //
   // So arma sobre uma FOLHA (control node -- Fallback/Sequence -- nao tem
   // 'matchesLabel()' fazendo sentido nenhum contra um rotulo de xboard); se
   // a linha selecionada nao e folha, nao faz nada -- 'buildBreakpointStatus()'
   // explica o motivo lendo o mesmo 'selectedBtLineIndex', sem precisar de um
   // campo de erro a parte.
   w.doArmBreakpoint = [&w](const bool fast) {
      if (w.displayedEntities.empty()) return;
      if (w.selectedBtLineIndex < 0
          || w.selectedBtLineIndex >= static_cast<int>(w.treeLines.size())) return;
      const BtTreeLine& line{w.treeLines[static_cast<std::size_t>(w.selectedBtLineIndex)]};
      if (!line.leaf) return;

      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(w.selectedEntityIndex, 0, static_cast<int>(w.displayedEntities.size()) - 1))};
      const EntityState& target{w.displayedEntities[idx]};

      {
         const std::lock_guard<std::mutex> lock(w.bpMutex);
         w.bp.arm(target.id, target.name, line.tag, fast,
                 w.clockStation != nullptr ? w.clockStation->getTimeScale() : 1.0);
         if (fast && w.clockStation != nullptr) {
            // Crava o pool de tempo critico no topo da escada -- ver o
            // comentario grande sobre 'restoreTimeScale()' em
            // app/BreakpointController.hpp sobre por que pular o msleep()
            // do dashboard nao bastava sozinho.
            w.clockStation->setTimeScale(w.ladder.maxScale());
         }
      }
      w.fastRunToBreakpoint = fast;
      if (w.clockStation != nullptr) w.clockStation->setPaused(false);
   };
   w.doCancelBreakpoint = [&w] {
      const std::lock_guard<std::mutex> lock(w.bpMutex);
      const bool shouldRestoreScale{w.bp.cancel()};
      if (shouldRestoreScale && w.clockStation != nullptr)
         w.clockStation->setTimeScale(w.bp.restoreTimeScale());
      w.fastRunToBreakpoint = false;
   };
   // Rotulo dos dois botoes ja e a LEGENDA do efeito: "trava" e a palavra que
   // tambem aparece no cabecalho ([BP]) e no status do card da arvore (ver
   // BreakpointController::status()) -- vocabulario consistente nos tres
   // lugares.
   w.btnRunToBreakpoint =
      makeButton("[g] Rodar (trava veloc. atual)", [&w] { w.doArmBreakpoint(false); });
   w.btnRunToBreakpointMax =
      makeButton("[G] Rodar (trava em veloc. MAXIMA)", [&w] { w.doArmBreakpoint(true); });
   w.btnCancelBreakpoint = makeButton("[x] Cancelar breakpoint", w.doCancelBreakpoint);

   // A "caixa onde aparece a bt" -- pedido explicito: clicavel, pra
   // selecionar a folha de interesse (breakpoint). PRECISA de DUAS instancias
   // (uma pra aba Players, outra pra aba Mapa): um Container::Tab so entrega
   // evento pro filho ATIVO, entao um Menu dentro de Players nunca receberia
   // clique nenhum enquanto a aba Mapa estivesse em cena. As DUAS apontam pro
   // MESMO 'w.selectedBtLineIndex'/'w.treeLineLabels' (o MenuOption e
   // copiado, nao movido -- os ponteiros dentro dele continuam os mesmos),
   // entao selecionar numa aba reflete na outra.
   MenuOption btTreeMenuOpt;
   btTreeMenuOpt.entries = &w.treeLineLabels;
   btTreeMenuOpt.selected = &w.selectedBtLineIndex;
   btTreeMenuOpt.entries_option.transform = [&w](const EntryState& es) -> Element {
      if (es.index < 0 || es.index >= static_cast<int>(w.treeLines.size())) return text(es.label);
      const BtTreeLine& line{w.treeLines[static_cast<std::size_t>(es.index)]};

      std::string activeLabel{"--"};
      if (!w.displayedEntities.empty()) {
         const std::size_t idx{static_cast<std::size_t>(
            std::clamp(w.selectedEntityIndex, 0, static_cast<int>(w.displayedEntities.size()) - 1))};
         activeLabel = w.displayedEntities[idx].behaviorLabel;
      }
      const bool isActiveLeaf{line.leaf && matchesLabel(line.tag, activeLabel)};

      bool isBreakpoint{};
      {
         const std::lock_guard<std::mutex> lock(w.bpMutex);
         isBreakpoint = w.bp.isArmedOn(line.tag);
      }

      return renderBtLine(line, isActiveLeaf, es.active, isBreakpoint);
   };
   w.treeMenuFleet = Menu(btTreeMenuOpt);
   w.treeMenuMap = Menu(btTreeMenuOpt);

   // Status do breakpoint + as acoes que dependem dele -- pedido explicito:
   // "devem ficar no quadro da propria arvore". Le 'w.bp' sob 'w.bpMutex' e
   // 'w.selectedBtLineIndex'/'w.treeLines' direto (mutacao so pelo clique do
   // proprio usuario, sem concorrencia de thread).
   w.buildBreakpointStatus = [&w]() -> Element {
      const bool hasSelection{w.selectedBtLineIndex >= 0
                              && w.selectedBtLineIndex < static_cast<int>(w.treeLines.size())};
      const bool selectionIsLeaf{hasSelection
         && w.treeLines[static_cast<std::size_t>(w.selectedBtLineIndex)].leaf};
      const std::string selectedLeafTag{hasSelection
         ? w.treeLines[static_cast<std::size_t>(w.selectedBtLineIndex)].tag : std::string{}};

      BreakpointStatus snap;
      {
         const std::lock_guard<std::mutex> lock(w.bpMutex);
         snap = w.bp.status(hasSelection, selectionIsLeaf, selectedLeafTag);
      }

      // 'paragraphAlignLeft' quebra linha sozinho na largura disponivel;
      // botao(es) vao numa linha PROPRIA, embaixo do texto, nunca no mesmo
      // hbox (um hbox nao quebra linha, so estoura pra fora do card).
      switch (snap.branch) {
         case BreakpointStatusBranch::Armed:
            return vbox({paragraphAlignLeft(snap.text) | color(Color::Yellow) | bold,
                         w.btnCancelBreakpoint->Render()});
         case BreakpointStatusBranch::Hit:
            return paragraphAlignLeft(snap.text) | color(Color::Green) | bold;
         case BreakpointStatusBranch::LeafSelected:
            return vbox({paragraphAlignLeft(snap.text),
                         hbox({w.btnRunToBreakpoint->Render(), text(" "),
                              w.btnRunToBreakpointMax->Render()})});
         case BreakpointStatusBranch::NonLeafSelected:
         case BreakpointStatusBranch::NoTreeSelection:
         default:
            return paragraphAlignLeft(snap.text) | dim;
      }
   };

   // O card de detalhe INTEIRO (campos + arvore de BT + os controles de
   // breakpoint, se houver arvore) -- usado pelas DUAS abas (Players e Mapa),
   // sempre com o MESMO tamanho ('w.detailPanelWidth'/'kDetailPanelHeight').
   // A arvore (e os controles de breakpoint junto dela) so aparece quando a
   // entidade TEM comportamento publicado (behaviorLabel != "--") E o
   // cenario declarou 'treeFile:' que deu pra carregar. 'treeMenu' e qual
   // das duas instancias (Players/Mapa) usar.
   w.buildDetailPanel = [&w](const EntityState& e, const Component& treeMenu) -> Element {
      Elements parts{renderEntityDetail(e)};
      if (e.behaviorLabel != "--" && !w.treeLines.empty()) {
         const Element treeBox{vbox({
            text(" Arvore de Comportamento ") | bold | bgcolor(Color::Blue) | color(Color::White),
            separator(),
            treeMenu->Render() | vscroll_indicator | frame | size(HEIGHT, LESS_THAN, 14),
            separator(),
            w.buildBreakpointStatus(),
         }) | border};
         parts.push_back(separator());
         parts.push_back(treeBox);
      }
      return vbox(std::move(parts)) | size(WIDTH, EQUAL, w.detailPanelWidth)
            | size(HEIGHT, EQUAL, kDetailPanelHeight);
   };

   // ---- aba "Players": lista rolavel + detalhe da entidade selecionada ----
   MenuOption entityMenuOpt;
   entityMenuOpt.entries = &w.entityLabels;
   entityMenuOpt.selected = &w.selectedEntityIndex;
   entityMenuOpt.entries_option.transform = [&w](const EntryState& es) -> Element {
      if (es.index >= 0 && es.index < static_cast<int>(w.displayedEntities.size()))
         return renderEntityRow(w.displayedEntities[static_cast<std::size_t>(es.index)], es.active);
      return text(es.label);
   };
   const Component entityMenu{Menu(entityMenuOpt)};

   // 'w.treeMenuFleet' entra como FILHO deste Renderer (nao Renderer solto)
   // -- e o que faz o clique nele chegar de verdade (o broadcast de mouse
   // desce por TODOS os filhos de um Container, mas so alcanca quem esta de
   // fato na arvore de componentes).
   const Component entityDetail{Renderer(w.treeMenuFleet, [&w]() -> Element {
      if (w.displayedEntities.empty()) {
         return text("(sem entidades no cenario)") | dim | center
               | size(WIDTH, EQUAL, w.detailPanelWidth) | size(HEIGHT, EQUAL, kDetailPanelHeight);
      }
      const std::size_t idx{static_cast<std::size_t>(
         std::clamp(w.selectedEntityIndex, 0, static_cast<int>(w.displayedEntities.size()) - 1))};
      return w.buildDetailPanel(w.displayedEntities[idx], w.treeMenuFleet);
   })};

   const Component fleetBody{Container::Horizontal({entityMenu, entityDetail})};
   return Renderer(fleetBody, [&w, entityMenu, entityDetail]() -> Element {
      return hbox({
                // O LIMITE tem de caber a soma de TODAS as colunas
                // (badge+nome+tipo+bt+thread+altitude+vel+combust, ver
                // app/FleetPanel.hpp) -- com um numero menor a soma passava
                // do limite e o FTXUI apertava as ultimas colunas em
                // silencio.
                vbox({
                   renderEntityListHeader(),
                   separator(),
                   entityMenu->Render() | vscroll_indicator | frame | flex,
                }) | size(WIDTH, LESS_THAN, kColBadge + kColName + kColType + kColBehavior
                                            + kColThread + kColAlt + kColSpd + kColFuel + 4) | flex,
                separator(),
                entityDetail->Render(),
             })
             | flex;
   });
}

} // namespace app
