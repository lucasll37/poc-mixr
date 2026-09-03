#include "app/BreakpointController.hpp"

#include <iomanip>
#include <sstream>

namespace app {

BreakpointController::BreakpointController(const double timeoutSimSec)
   : timeoutSimSec_(timeoutSimSec)
{
}

void BreakpointController::arm(const int entityId, std::string entityName, std::string nodeTag,
                               const bool fastMode, const double simSecNow,
                               const double currentTimeScale)
{
   armed_ = true;
   entityId_ = entityId;
   entityName_ = std::move(entityName);
   nodeTag_ = std::move(nodeTag);
   fastMode_ = fastMode;
   armedAtSimSec_ = simSecNow;
   restoreTimeScale_ = currentTimeScale;
   hit_ = false;
   hitSimSec_ = 0.0;
   timedOut_ = false;
}

bool BreakpointController::cancel()
{
   const bool shouldRestoreScale{armed_ && fastMode_};
   armed_ = false;
   return shouldRestoreScale;
}

BreakpointTickResult BreakpointController::tick(const std::vector<BreakpointEntity>& entities,
                                                const BreakpointLabelMatcher& matches,
                                                const double simSecNow)
{
   BreakpointTickResult result;
   if (!armed_) return result;

   for (const auto& e : entities) {
      if (e.id == entityId_ && matches(nodeTag_, e.behaviorLabel)) {
         armed_ = false;
         hit_ = true;
         hitSimSec_ = simSecNow;
         result.outcome = BreakpointOutcome::Hit;
         result.shouldPause = true;
         result.shouldRestoreScale = fastMode_;
         result.simSecAtOutcome = simSecNow;
         return result;
      }
   }

   if (armed_ && (simSecNow - armedAtSimSec_) > timeoutSimSec_) {
      armed_ = false;
      timedOut_ = true;
      result.outcome = BreakpointOutcome::TimedOut;
      result.shouldRestoreScale = fastMode_;
      result.simSecAtOutcome = simSecNow;
   }

   return result;
}

BreakpointStatus BreakpointController::status(const bool hasTreeSelection,
                                              const bool selectionIsLeaf,
                                              const std::string& selectedLeafTag) const
{
   if (armed_) {
      std::ostringstream oss;
      oss << "Aguardando: " << entityName_ << " -> \"" << nodeTag_ << "\""
          << (fastMode_ ? " (velocidade maxima)" : " (velocidade atual)");
      return BreakpointStatus{BreakpointStatusBranch::Armed, oss.str()};
   }
   if (hit_) {
      std::ostringstream oss;
      oss << "Breakpoint atingido: " << entityName_ << " chegou em \"" << nodeTag_
          << "\" (sim=" << std::fixed << std::setprecision(1) << hitSimSec_
          << "s) -- simulacao PAUSADA";
      return BreakpointStatus{BreakpointStatusBranch::Hit, oss.str()};
   }
   if (timedOut_) {
      std::ostringstream oss;
      oss << "Breakpoint NAO atingido em " << std::fixed << std::setprecision(0)
          << timeoutSimSec_ << "s de simulacao -- cancelado";
      return BreakpointStatus{BreakpointStatusBranch::TimedOut, oss.str()};
   }
   if (hasTreeSelection) {
      if (!selectionIsLeaf) {
         return BreakpointStatus{BreakpointStatusBranch::NonLeafSelected,
            "Selecione uma FOLHA da arvore (nao um no de controle) para marcar um breakpoint"};
      }
      return BreakpointStatus{BreakpointStatusBranch::LeafSelected,
         "Folha selecionada: \"" + selectedLeafTag + "\""};
   }
   return BreakpointStatus{BreakpointStatusBranch::NoTreeSelection,
      "Clique numa folha da arvore (Players ou Mapa) para marcar um breakpoint"};
}

} // namespace app
