#include "app/BreakpointController.hpp"

#include <iomanip>
#include <sstream>

namespace app {

void BreakpointController::arm(const int entityId, std::string entityName, std::string nodeTag,
                               const bool fastMode, const double currentTimeScale)
{
   armed_ = true;
   entityId_ = entityId;
   entityName_ = std::move(entityName);
   nodeTag_ = std::move(nodeTag);
   fastMode_ = fastMode;
   restoreTimeScale_ = currentTimeScale;
   hit_ = false;
   hitSimSec_ = 0.0;
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

   return result;
}

BreakpointStatus BreakpointController::status(const bool hasTreeSelection,
                                              const bool selectionIsLeaf,
                                              const std::string& selectedLeafTag) const
{
   if (armed_) {
      std::ostringstream oss;
      oss << "BP armado: " << entityName_ << " -> \"" << nodeTag_ << "\" ("
          << (fastMode_ ? "MAX" : "atual") << ")";
      return BreakpointStatus{BreakpointStatusBranch::Armed, oss.str()};
   }
   if (hit_) {
      std::ostringstream oss;
      oss << "BP atingido: " << entityName_ << " -> \"" << nodeTag_ << "\" (sim="
          << std::fixed << std::setprecision(1) << hitSimSec_ << "s) -- PAUSADO";
      return BreakpointStatus{BreakpointStatusBranch::Hit, oss.str()};
   }
   if (hasTreeSelection) {
      if (!selectionIsLeaf) {
         return BreakpointStatus{BreakpointStatusBranch::NonLeafSelected,
            "Selecione uma folha (nao um no de controle)"};
      }
      return BreakpointStatus{BreakpointStatusBranch::LeafSelected,
         "Folha: \"" + selectedLeafTag + "\""};
   }
   return BreakpointStatus{BreakpointStatusBranch::NoTreeSelection,
      "Clique numa folha da arvore para marcar um BP"};
}

} // namespace app
