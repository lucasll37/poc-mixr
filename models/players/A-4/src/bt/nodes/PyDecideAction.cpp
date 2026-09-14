#include "bt/nodes/PyDecideAction.hpp"

#include "bt/DecisionContext.hpp"
#include "bt/ObservationSchema.hpp"

#include "xlog/Log.hpp"
#include "xpyembed/PyEmbed.hpp"
#include "xrlbridge/ObservationFields.hpp"

#include <array>
#include <atomic>
#include <vector>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

namespace {
// Um id por INSTANCIA do no -- ver o "porque" no cabecalho. Atomico porque as
// arvores das quatro aeronaves podem ser construidas em threads diferentes do
// pool de tempo critico.
std::atomic<int> g_proximaInstancia{1};
}

PyDecideAction::PyDecideAction(const std::string& name, const BT::NodeConfiguration& config,
                               const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context),
     instanciaId_(g_proximaInstancia.fetch_add(1))
{
}

BT::PortsList PyDecideAction::providedPorts()
{
   return {
      BT::InputPort<std::string>("script", "", "caminho do .py que define decide(obs)"),
      BT::InputPort<std::string>("label", "PY", "rotulo no dump e no quadro"),
      BT::InputPort<std::string>("schema", "classic28",
                                 "campos da observacao, e ordem: 'classic28' (default), "
                                 "'all', ou lista ad-hoc separada por espaco"),
   };
}

BT::NodeStatus PyDecideAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   if (!tentouCarregar_) {
      tentouCarregar_ = true;

      std::string schemaValor{"classic28"};
      if (const BT::Optional<std::string> in{getInput<std::string>("schema")}) schemaValor = in.value();
      try {
         bound_ = xrlbridge::bind<domain::WorldView>(resolveObservationSchema(schemaValor),
                                                      domain::worldViewFieldRegistry());
      } catch (const xrlbridge::SchemaError& ex) {
         LOG(ERROR) << "[PyDecide] " << ex.what();
         return BT::NodeStatus::FAILURE;
      }

      const BT::Optional<std::string> caminho{getInput<std::string>("script")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[PyDecide] porta 'script' ausente ou vazia no XML da arvore";
      } else if (!mixr::xpyembed::isAvailable()) {
         LOG(WARNING) << "[PyDecide] sem interpretador Python -- o no fica inerte";
      } else {
         scriptId_ = mixr::xpyembed::loadScript(caminho.value());
      }
   }
   if (scriptId_ == 0) return BT::NodeStatus::FAILURE;

   const domain::WorldView& snap{context_.behavior->snapshot()};
   std::vector<double> entrada(bound_.resolved.size());
   xrlbridge::pack(bound_, snap, entrada.data());

   std::array<double, XRLBRIDGE_ACTION_SIZE> saida{};
   if (!mixr::xpyembed::decide(scriptId_, instanciaId_,
                               entrada.data(), static_cast<int>(entrada.size()),
                               saida.data(), static_cast<int>(saida.size()))) {
      return BT::NodeStatus::FAILURE;
   }

   domain::FlightCommand cmd;
   cmd.headingDeg = saida[0];
   cmd.altitudeM = saida[1];
   cmd.speedKts = saida[2];

   std::string rotulo{"PY"};
   if (const BT::Optional<std::string> in{getInput<std::string>("label")}) rotulo = in.value();

   context_.behavior->decision().take(cmd, rotulo);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
