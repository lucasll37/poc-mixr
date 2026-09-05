#pragma once

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// Visualizacao GRAFICA da arvore de comportamento (BehaviorTree.CPP, a
// tecnologia de BT declarada do projeto -- ver contexts/BTCPP-CONTEXT.md)
// dentro do card de detalhe de uma entidade (abas Frota e Mapa).
//
// A UNICA coisa que o dashboard sabe em tempo real sobre a decisao de uma
// entidade e o ROTULO da folha vencedora (xboard::Readout::label, ja
// generico -- ver app/DashboardState.cpp). A ESTRUTURA da arvore (quem e
// filho de quem) nao existe em nenhum lugar acessivel em runtime -- por
// isso este arquivo LE o mesmo arquivo XML que o cenario ja resolveu para
// 'treeFile:' (achado no '.generated.edl', uma unica vez, no startup -- ver
// loadTreeForScenario()) e faz um parse minimo, so o suficiente pro formato
// do BT.CPP (tags aninhadas, '<Tag/>' pra folha, '<Tag>...</Tag>' pra
// controle).
//
// Continua agnostico a MODELO (funciona com qualquer '.xml' de arvore que
// o cenario aponte, nao um caminho fixo) mas NAO a tecnologia de BT --
// assume BehaviorTree.CPP, que e a escolha de projeto documentada, nao uma
// suposicao sobre qual modelo esta carregado.
//------------------------------------------------------------------------------
namespace app {

struct BtNode
{
   std::string tag;              // nome da tag XML ("Fallback", "Patrol", ...)
   std::vector<BtNode> children;  // vazio = folha (acao/condicao)
};

bool isValid(const BtNode& node);

// Parse minimo de UM arquivo de arvore BT.CPP -- devolve o NO RAIZ de
// verdade (o primeiro filho de '<BehaviorTree>', sem o envelope
// '<root>'/'<BehaviorTree>' que so serve o parser do framework). BtNode{}
// (invalido) se o arquivo nao existir ou nao tiver essa forma.
BtNode parseBehaviorTreeXml(const std::string& xmlPath);

// Varre 'generatedEdlPath' (o '.edl' ja expandido do cenario) procurando a
// PRIMEIRA ocorrencia de 'treeFile: "..."' e devolve a arvore ja parseada.
// BtNode{} se o cenario nao declarar nenhuma (um modelo futuro sem BT, ou
// um player sem BtBehavior). Cenarios de producao tem uma BtBehavior por
// aviao, quase sempre com o MESMO arquivo -- a primeira ocorrencia ja basta
// pra mostrar a estrutura (limite conhecido: um player com arvore
// DIFERENTE dos outros mostraria a arvore errada -- documentado no
// CLAUDE.md, nao acontece em nenhum cenario deste repositorio hoje).
BtNode loadTreeForScenario(const std::string& generatedEdlPath);

// MELHOR ESFORCO: compara o nome da tag normalizado (maiusculo, so
// alfanumerico) contra o rotulo por CONTEM-EM-QUALQUER-DIRECAO. Cobre os
// casos regulares (tag "Patrol" <-> rotulo "PATROL", tag "SupportAlert" <->
// "SUPPORT") mas nao pega rotulo que so existe no C++ sem relacao textual
// com a tag (ex.: a tag "ReturnToBase" decide em runtime entre os rotulos
// "RTB"/"HOME", nenhum dos dois contido no nome da tag) -- limite aceito,
// documentado no CLAUDE.md: a alternativa seria uma tabela de mapeamento
// escrita a mao aqui, especifica do modelo 'flight' -- o oposto do resto
// deste dashboard. Publica (nao so uso interno) porque tanto o destaque da
// folha ativa QUANTO a checagem de breakpoint (ver DashboardLoop.cpp)
// precisam do MESMO criterio.
bool matchesLabel(const std::string& tag, const std::string& label);

//------------------------------------------------------------------------------
// Uma linha ja "achatada" da arvore -- prefixo de indentacao (linhas
// Unicode, estilo 'tree') + a tag, pronta pra exibir, mais se e folha (so
// folha faz sentido como alvo de breakpoint -- ver o "porque" no
// comentario de DashboardLoop.cpp sobre 'runPendingAction'/'Breakpoint').
//------------------------------------------------------------------------------
struct BtTreeLine
{
   std::string tag;      // so o nome da tag, pra casar com matchesLabel()
   std::string display;  // prefixo + tag, ja pronto pro Menu mostrar
   bool leaf{};
};

// Mesma travessia de sempre, so que devolvendo uma LISTA PLANA em vez de um
// Element -- e o que permite a arvore virar um ftxui::Menu de verdade (uma
// linha por entrada), pedido explicito: "a caixa onde aparece a bt deve
// ser clicavel para selecionar a folha de interesse".
std::vector<BtTreeLine> flattenBehaviorTree(const BtNode& root);

} // namespace app
