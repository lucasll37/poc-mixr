#pragma once

#include "behaviortree_cpp_v3/action_node.h"

#include "formations.hpp"

namespace mixr {
namespace models { class Autopilot; class Player; }
namespace terrain { class Terrain; }
}

namespace bt_nodes {

// Aplica os offsets (trail/right/altitude) da formacao corrente nos slots
// nativos do Autopilot do wingman (leadFollowingDistanceTrail/Right/
// DeltaAltitude) -- quem de fato converge pro slot e o followTheLeadMode
// nativo, este no so decide/atualiza QUAL offset usar. So chama os
// setters quando o offset muda (evita sobrescrever o mesmo valor todo
// tick). Porta "forceTrail" (default false): quando true, usa sempre
// TRAIL independente da formacao corrente do FormationState -- usado no
// ramo de RTB da arvore (ver wingman_tree.xml).
//
// Tambem aplica uma correcao simples de terreno: se a posicao atual do
// wingman ja estiver abaixo da elevacao do terreno (consultada via
// mixr::terrain::Terrain nativo) + margem de seguranca, soma a diferenca
// no delta de altitude pedido -- nao e um seguidor de terreno rigoroso,
// so uma rede de seguranca proporcional (ver CLAUDE.md).
class FollowFormationAction final : public BT::SyncActionNode
{
public:
   FollowFormationAction(const std::string& name, const BT::NodeConfiguration& config);

   static BT::PortsList providedPorts()
   {
      return { BT::InputPort<bool>("forceTrail", false, "forca formacao TRAIL (ramo de RTB)") };
   }

protected:
   BT::NodeStatus tick() override;

private:
   formations::FormationState* formationState{};
   int slotIndex{};
   mixr::models::Autopilot* autopilot{};
   mixr::models::Player* wingman{};
   const mixr::terrain::Terrain* terrain{};

   bool hasAppliedOffset{};
   formations::Offset lastOffset{};
};

} // namespace bt_nodes
