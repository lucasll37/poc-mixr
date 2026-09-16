#include "bt/nodes/OnnxPolicyAction.hpp"

#include "bt/DecisionContext.hpp"
#include "bt/ObservationSchema.hpp"

#include "xinfer/Infer.hpp"
#include "xlog/Log.hpp"
#include "xrlbridge/RLBridge.hpp"

#include <array>
#include <vector>

namespace mixr {
namespace models {
namespace xA_4 {
namespace bt_nodes {

OnnxPolicyAction::OnnxPolicyAction(const std::string& name, const BT::NodeConfiguration& config,
                                   const NodeContext& context)
   : BT::SyncActionNode(name, config), context_(context)
{
}

BT::PortsList OnnxPolicyAction::providedPorts()
{
   return {
      BT::InputPort<std::string>("model", "", "caminho do .onnx da politica"),
      BT::InputPort<bool>("normalized", true,
                          "true: saida em [-1,1], desnormalizada aqui (export do SB3)"),
      BT::InputPort<std::string>("label", "ONNX", "rotulo no dump e no quadro"),
      BT::InputPort<std::string>("schema", "classic28",
                                 "campos da observacao, e ordem: 'classic28' (default), "
                                 "'all', ou lista ad-hoc separada por espaco"),
   };
}

BT::NodeStatus OnnxPolicyAction::tick()
{
   if (context_.behavior == nullptr) return BT::NodeStatus::FAILURE;

   if (!tentouAbrir_) {
      tentouAbrir_ = true;

      std::string schemaValor{"classic28"};
      if (const BT::Optional<std::string> in{getInput<std::string>("schema")}) schemaValor = in.value();
      try {
         bound_ = xrlbridge::bind<domain::WorldView>(resolveObservationSchema(schemaValor),
                                                      domain::worldViewFieldRegistry());
      } catch (const xrlbridge::SchemaError& ex) {
         LOG(ERROR) << "[OnnxPolicy] " << ex.what();
         return BT::NodeStatus::FAILURE;
      }

      const BT::Optional<std::string> caminho{getInput<std::string>("model")};
      if (!caminho || caminho.value().empty()) {
         LOG(ERROR) << "[OnnxPolicy] porta 'model' ausente ou vazia no XML da arvore";
      } else {
         modelId_ = mixr::xinfer::open(caminho.value());
         if (modelId_ != 0) {
            int nIn{}, nOut{};
            const int nEsperado{static_cast<int>(bound_.resolved.size())};
            // A forma e contrato, nao sugestao. Um .onnx com outra CONTAGEM
            // foi treinado contra outro schema, e comandar com ele seria
            // pior que nao comandar.
            if (mixr::xinfer::shape(modelId_, nIn, nOut) &&
                (nIn != nEsperado || nOut != XRLBRIDGE_ACTION_SIZE)) {
               LOG(ERROR) << "[OnnxPolicy] '" << caminho.value() << "' tem forma "
                          << nIn << "->" << nOut << ", mas o schema '" << bound_.schema.name
                          << "' espera " << nEsperado << "->" << XRLBRIDGE_ACTION_SIZE;
               modelId_ = 0;
            } else if (modelId_ != 0) {
               // A contagem bater nao basta: dois .onnx do MESMO tamanho
               // podem esperar campos DIFERENTES, ou na ordem errada -- risco
               // que so passou a existir com o schema variavel (antes, so
               // havia UM tamanho possivel). Checado por IDENTIDADE quando o
               // .onnx traz a metadata (ver xinfer::fields()); sem ela --
               // todo .onnx exportado antes desta funcionalidade -- so a
               // checagem de contagem acima vale, como sempre.
               std::vector<std::string> declarados;
               if (mixr::xinfer::fields(modelId_, declarados)) {
                  std::vector<std::string> esperados;
                  esperados.reserve(bound_.resolved.size());
                  for (const auto* const decl : bound_.resolved) esperados.push_back(decl->name);
                  if (declarados != esperados) {
                     LOG(ERROR) << "[OnnxPolicy] '" << caminho.value()
                                << "' foi exportado para outros campos (ou outra ordem) que o "
                                << "schema '" << bound_.schema.name
                                << "' resolveu -- reexporte ou corrija a porta 'schema'";
                     modelId_ = 0;
                  }
               }
            }
         }
      }
   }
   if (modelId_ == 0) return BT::NodeStatus::FAILURE;

   const domain::WorldView& snap{context_.behavior->snapshot()};
   std::vector<float> entrada(bound_.resolved.size());
   xrlbridge::pack(bound_, snap, entrada.data());

   std::array<float, XRLBRIDGE_ACTION_SIZE> saida{};
   const int escritos{mixr::xinfer::run(modelId_, entrada.data(),
                                        static_cast<int>(entrada.size()),
                                        saida.data(), static_cast<int>(saida.size()))};
   if (escritos != XRLBRIDGE_ACTION_SIZE) return BT::NodeStatus::FAILURE;

   bool normalizada{true};
   if (const BT::Optional<bool> in{getInput<bool>("normalized")}) normalizada = in.value();

   domain::FlightCommand cmd;
   if (normalizada) {
      // Uma unica implementacao da desnormalizacao, em libs/xrlbridge, com
      // os mesmos limites que o lado Python usa para montar o action_space.
      const mixr::xrlbridge::Command c{mixr::xrlbridge::unscaleCommand(saida.data())};
      cmd.headingDeg = c.headingDeg;
      cmd.altitudeM = c.altitudeM;
      cmd.speedKts = c.speedKts;
   } else {
      cmd.headingDeg = static_cast<double>(saida[0]);
      cmd.altitudeM = static_cast<double>(saida[1]);
      cmd.speedKts = static_cast<double>(saida[2]);
   }

   std::string rotulo{"ONNX"};
   if (const BT::Optional<std::string> in{getInput<std::string>("label")}) rotulo = in.value();

   context_.behavior->decision().take(cmd, rotulo);
   return BT::NodeStatus::SUCCESS;
}

} // namespace bt_nodes
} // namespace xA_4
} // namespace models
} // namespace mixr
