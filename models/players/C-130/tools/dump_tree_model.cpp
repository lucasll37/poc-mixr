//
// dump-tree-model -- gera o <TreeNodesModel> que o Groot precisa para
// reconhecer os nos customizados DESTE modelo, usando a funcao NATIVA do
// BT.CPP para isso: BT::writeTreeNodesModelXML(), sobre uma factory populada
// pelo MESMO registerNodes() que o modelo de verdade chama.
//
// Por que isso e' preciso: o Groot e' um app Qt a parte, que nunca viu o .so
// deste modelo -- sem esse bloco dentro do .xml, ele recusa a arvore inteira
// com "This model has not been registered: <ID>". Ver CLAUDE.md raiz, secao
// "Groot -- editor e monitor ao vivo", armadilha no 2.
//
// Nada aqui e' amarrado ao modelo em que este arquivo mora: bt_factory.hpp e'
// o DESTE projeto, entao a copia que 'make new-model' fez do template
// reflete o registro DAQUELE modelo, sem editar uma linha.
//
// DOIS modos:
//
//   dump-tree-model                 -- imprime so' o fragmento
//                                      <TreeNodesModel>...</TreeNodesModel>,
//                                      pronto pra colar dentro do <root> de
//                                      uma arvore que ja existe. E' o modo
//                                      que tools/update_bt_models.py usa
//                                      (alvo 'make update-bt').
//
//   dump-tree-model --skeleton [ID] -- imprime um .xml COMPLETO, pronto pra
//                                      salvar e abrir no Groot: uma arvore
//                                      vazia (um <Fallback> so, builtin do
//                                      BT.CPP) mais o mesmo bloco, os dois
//                                      dentro do MESMO <root>. E' o modo que
//                                      'make create-bt' usa.
//
#include "bt/bt_factory.hpp"

#include "behaviortree_cpp_v3/xml_parsing.h"

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {

std::string extractTreeNodesModel(const BT::BehaviorTreeFactory& factory)
{
   const std::string full{BT::writeTreeNodesModelXML(factory)};
   const std::string openTag{"<TreeNodesModel>"};
   const std::string closeTag{"</TreeNodesModel>"};
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
   bool skeleton{false};
   std::string treeId{"NovaArvore"};

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

   // behavior=nullptr e' seguro: registerBuilder<T>() so guarda um construtor
   // (lambda) na factory, nunca instancia um no. Nenhum no e' de fato criado
   // -- so' o manifesto (ID + portas) e' lido por writeTreeNodesModelXML(),
   // via factory.manifests(). E' tambem por isso que todo tick() deste modelo
   // comeca conferindo 'context_.behavior == nullptr'.
   mixr::models::xC_130::bt::NodeContext context;
   mixr::models::xC_130::bt::registerNodes(factory, context);

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

   // O ID de uma SUBARVORE nova nao pode colidir com o de um NO registrado:
   // o BT.CPP resolve uma tag XML nua para NO primeiro e para subarvore so
   // como fallback (xml_parsing.cpp, createNodeFromXML: builders().count(ID)
   // e' checado ANTES de tree_roots.count(ID)) -- a colisao seria silenciosa.
   // Recusar aqui, na hora de GERAR, e' mais barato que descobrir depois de
   // editar a arvore no Groot.
   if (factory.builders().count(treeId) > 0) {
      std::cerr << "dump-tree-model: '" << treeId << "' ja e' o ID de um NO registrado "
                << "neste modelo -- uma subarvore com este MESMO ID resolveria para o no, "
                << "nunca para a subarvore, sempre que referenciada pela tag nua. "
                << "Escolha outro ID.\n";
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
