//
// dump_tree_model -- gera o <TreeNodesModel> que o Groot precisa para
// reconhecer os nos customizados deste modelo (ver CLAUDE.md, "Groot --
// editor e monitor ao vivo"), usando a funcao NATIVA do BT.CPP para isso:
// BT::writeTreeNodesModelXML(), a partir de uma factory populada pelos
// MESMOS registerNodes()/registerSdkNodes() que o modelo de verdade chama.
//
// Isto substitui manter o bloco a mao nas arvores .xml deste projeto: sempre
// que um no novo entrar em bt_factory.cpp/bt_factory_sdk.cpp, rodar este
// binario de novo reflete o registro real, sem depender de alguem lembrar de
// atualizar XML a mao. Nao ha nada aqui amarrado ao modelo especifico onde
// este arquivo mora -- bt_factory.hpp/bt_factory_sdk.hpp sao os deste
// PROJETO (copie tools/ inteiro para outro models/players/<nome>/ e ele
// reflete o registro DAQUELE projeto).
//
// DOIS modos:
//
//   dump-tree-model                 -- imprime so' o fragmento
//                                       <TreeNodesModel>...</TreeNodesModel>.
//                                       BT::writeTreeNodesModelXML() devolve
//                                       isso ja envolto num <root> proprio
//                                       (pensado pra virar um .xml sozinho),
//                                       mas as arvores de producao deste
//                                       projeto ja tem o <root> delas; por
//                                       isso extrai so o miolo, pronto pra
//                                       colar dentro de um <root> ja
//                                       existente. E' o modo que
//                                       tools/update_bt_models.py usa (alvo
//                                       'make update-bt') para atualizar
//                                       TODA arvore de configs/.
//
//   dump-tree-model --skeleton [ID] -- imprime um .xml COMPLETO e pronto pra
//                                       SALVAR E ABRIR NO GROOT: uma arvore
//                                       vazia (um <Fallback> so, de partida
//                                       -- builtin do BT.CPP, nao precisa de
//                                       modelo nenhum) mais o mesmo
//                                       <TreeNodesModel> acima, os DOIS
//                                       dentro do MESMO <root>. E' a resposta
//                                       para "como eu crio uma arvore nova
//                                       com os nos que eu ja implementei":
//                                       nao tinha nada gerando esse
//                                       esqueleto antes disto -- so o
//                                       <TreeNodesModel> era gerado, e o
//                                       "<BehaviorTree><Fallback/></...>"
//                                       era so um exemplo digitado a mao. E'
//                                       o modo que 'make create-bt' usa para
//                                       escrever configs/bt.xml.
//
#include "bt/bt_factory.hpp"
#include "bt/bt_factory_sdk.hpp"

#include "behaviortree_cpp_v3/xml_parsing.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

std::string extractTreeNodesModel(const BT::BehaviorTreeFactory& factory)
{
   const std::string full = BT::writeTreeNodesModelXML(factory);
   const std::string openTag = "<TreeNodesModel>";
   const std::string closeTag = "</TreeNodesModel>";
   const auto begin = full.find(openTag);
   const auto end = full.find(closeTag);
   if (begin == std::string::npos || end == std::string::npos) {
      throw std::runtime_error(
         "<TreeNodesModel> nao encontrado na saida de writeTreeNodesModelXML() "
         "-- a factory esta vazia?");
   }
   return full.substr(begin, end + closeTag.size() - begin);
}

void printUsage(const char* prog)
{
   std::cerr << "uso: " << prog << " [--skeleton [ID-da-arvore]]\n"
             << "  sem argumento    : imprime so' <TreeNodesModel>...</TreeNodesModel>\n"
             << "  --skeleton [ID]  : imprime um .xml COMPLETO, pronto pra salvar e abrir\n"
             << "                     no Groot (arvore vazia + o modelo). ID default:\n"
             << "                     'NovaArvore'.\n";
}

} // namespace

int main(int argc, char** argv)
{
   bool skeleton = false;
   std::string treeId = "NovaArvore";

   if (argc >= 2) {
      if (std::strcmp(argv[1], "--skeleton") == 0) {
         skeleton = true;
         if (argc >= 3) treeId = argv[2];
      } else if (std::strcmp(argv[1], "-h") == 0 || std::strcmp(argv[1], "--help") == 0) {
         printUsage(argv[0]);
         return 0;
      } else {
         printUsage(argv[0]);
         return 2;
      }
   }

   BT::BehaviorTreeFactory factory;

   // behavior=nullptr e seguro aqui: registerBuilder<T>() so guarda um
   // construtor (lambda) na factory, nunca instancia um no. Nenhum no e de
   // fato criado -- so' o manifesto (ID + portas) e' lido por
   // writeTreeNodesModelXML(), via factory.manifests().
   mixr::models::xA_4::bt_nodes::NodeContext context;
   mixr::models::xA_4::bt_nodes::registerNodes(factory, context);
   mixr::models::xA_4::bt_nodes::registerSdkNodes(factory, context);

   std::string model;
   try {
      model = extractTreeNodesModel(factory);
   } catch (const std::exception& ex) {
      std::cerr << "dump-tree-model: " << ex.what() << "\n";
      return 1;
   }

   if (!skeleton) {
      std::cout << model << "\n";
      return 0;
   }

   // --skeleton precisa recusar um ID que ja e o de um no registrado
   // (Condition/Action) neste modelo: o BT.CPP resolve uma tag XML nua para
   // no registrado antes de subarvore (createNodeFromXML(), confirmado em
   // xml_parsing.cpp -- factory.builders().count(ID) e checado antes de
   // tree_roots.count(ID)). Uma subarvore nova com ID colidindo,
   // referenciada pela mesma tag nua em outra arvore, resolveria
   // silenciosamente para o no nativo, nunca para a subarvore. Recusar na
   // geracao do esqueleto e mais barato que descobrir depois de editar no
   // Groot.
   if (factory.builders().count(treeId) > 0) {
      std::cerr << "dump-tree-model: '" << treeId << "' ja e' o ID de um NO registrado "
                << "(Condition/Action) neste modelo -- uma subarvore com este MESMO ID "
                << "resolveria para o no, nunca para a subarvore, sempre que referenciada "
                << "pela tag nua (o idioma que este projeto ja usa em producao, ex. "
                << "<FuelLow.../> em flight_tree.xml). Escolha outro ID.\n";
      return 1;
   }

   std::cout << "<root main_tree_to_execute=\"" << treeId << "\">\n"
             << "  <BehaviorTree ID=\"" << treeId << "\">\n"
             << "    <Fallback name=\"root\"/>\n"
             << "  </BehaviorTree>\n\n"
             << "  " << model << "\n"
             << "</root>\n";
   return 0;
}
