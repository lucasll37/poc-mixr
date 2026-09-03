#pragma once

#include <functional>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// A maquina de estados do breakpoint de arvore de comportamento --
// "marcar um estado da bt de um dado elemento e rodar a simulacao ate que
// aquele no seja atingido, devolvendo a simulacao pausada" -- separada do
// wiring de app/DashboardLoop.cpp (mutex/ClockStation/FTXUI). NENHUM tipo
// de FTXUI/MIXR aqui: quem toma o lock ('bpMutex' em runDashboard()),
// chama ClockStation::setPaused()/setTimeScale() e desenha o Element
// continua sendo responsabilidade do chamador -- so a DECISAO ("armou?
// atingiu? expirou? qual mensagem mostrar?") mora aqui, testavel sem
// levantar Station nem tela nenhuma (ver tests/app/test_breakpoint_
// controller.cpp).
//------------------------------------------------------------------------------
namespace app {

// O minimo que a checagem por amostra precisa de cada entidade visivel --
// equivalente aos dois campos de app::EntityState que o laco de
// verificacao realmente le (id, behaviorLabel).
struct BreakpointEntity
{
   int id{};
   std::string behaviorLabel;
};

// Sem timeout: uma vez armado, o breakpoint so sai do ar por HIT ou por
// cancelamento manual ('x'/botao) -- "continuar acelerado ate o fim" e
// "a busca deve ir ate chegar nesse no ou a simulacao encerrar" foram
// pedidos explicitos que descartam qualquer desarme automatico por tempo.
enum class BreakpointOutcome { None, Hit };

// O que mudou nesta chamada a tick() -- o chamador decide o QUE FAZER
// (pausar o ClockStation, restaurar a escala nominal); o controller so
// decide SE mudou e o que aconteceu.
struct BreakpointTickResult
{
   BreakpointOutcome outcome{BreakpointOutcome::None};
   bool shouldPause{};          // Hit: chamador deve ClockStation::setPaused(true)
   bool shouldRestoreScale{};   // Hit em modo rapido: restaurar a escala nominal
   double simSecAtOutcome{};
};

// Mesma assinatura de app::matchesLabel -- injetada por parametro para
// este header nunca precisar incluir BehaviorTreeView.hpp.
using BreakpointLabelMatcher = std::function<bool(const std::string& tag, const std::string& label)>;

enum class BreakpointStatusBranch {
   Armed,           // aguardando o no ser atingido
   Hit,             // atingiu -- simulacao pausada
   NoTreeSelection, // nenhuma linha da arvore selecionada ainda
   NonLeafSelected, // selecionou um no de controle (Fallback/Sequence), nao serve de alvo
   LeafSelected,    // selecionou uma folha valida -- pode armar
};

struct BreakpointStatus
{
   BreakpointStatusBranch branch{BreakpointStatusBranch::NoTreeSelection};
   std::string text;   // mensagem formatada, pronta pra exibir (sem cor/Element)
};

// Sem mutex proprio -- o chamador continua dono da secao critica (o mesmo
// 'bpMutex' que ja protegia a struct Breakpoint anterior): quem le/escreve
// o estado publicado para a UI precisa da MESMA garantia de visibilidade
// entre a thread de simulacao e a de UI, e isso e um problema de
// CONCORRENCIA, nao de REGRA -- nao faz sentido escondido aqui dentro.
class BreakpointController
{
public:
   BreakpointController() = default;

   // Arma sobre (entityId, nodeTag). 'currentTimeScale' e sempre guardado
   // como a escala a restaurar (so e CONSULTADO se fastMode==true depois).
   void arm(int entityId, std::string entityName, std::string nodeTag,
            bool fastMode, double currentTimeScale);

   // Cancelamento manual (tecla 'x'/botao). Devolve true se o chamador
   // deve restaurar a escala nominal do ClockStation (isto e, se estava
   // em modo rapido).
   bool cancel();

   bool isArmed() const { return armed_; }
   // Usado pelo destaque "[BP]" na linha da arvore -- true se HA um
   // breakpoint armado sobre exatamente esta tag de no.
   bool isArmedOn(const std::string& tag) const { return armed_ && nodeTag_ == tag; }
   double restoreTimeScale() const { return restoreTimeScale_; }

   // Uma amostra da thread de simulacao. Nao toma lock nenhum: o chamador
   // chama isto DENTRO da mesma secao critica que ja tinha antes.
   BreakpointTickResult tick(const std::vector<BreakpointEntity>& entities,
                             const BreakpointLabelMatcher& matches, double simSecNow);

   // A decisao de QUAL branch e QUAL texto -- o chamador decide os
   // botoes/cores por 'branch'.
   BreakpointStatus status(bool hasTreeSelection, bool selectionIsLeaf,
                           const std::string& selectedLeafTag) const;

private:
   bool armed_{};
   int entityId_{-1};
   std::string entityName_;
   std::string nodeTag_;
   bool fastMode_{};
   double restoreTimeScale_{1.0};

   bool hit_{};
   double hitSimSec_{};
};

} // namespace app
