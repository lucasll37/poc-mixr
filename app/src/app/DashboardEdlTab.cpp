#include "app/DashboardWiring.hpp"

#include "app/EdlEditorState.hpp"
#include "app/EdlHighlightRender.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

//------------------------------------------------------------------------------
// A aba "EDL" (F7): editor de .edl EM MEMORIA (ver app/EdlEditorState.hpp
// para o "sem persistir no arquivo real"). E' a UNICA aba que precisa de
// foco de TECLADO de verdade (um ftxui::Input multilinha) -- as outras seis
// nunca precisaram porque cada tecla e' tratada a mao no CatchEvent mais
// externo (ver app/DashboardLoop.cpp, o comentario grande sobre
// 'edlInput->TakeFocus()').
//
// 'w.doEdlValidate'/'w.doEdlRun'/'w.doEdlRevert' sao chamadas tanto pelos
// botoes desta aba quanto pelo CatchEvent global (F8/F9/F10 com
// 'activeTab==6') -- por isso viram campos. 'w.edlInput' tambem precisa
// sair daqui: 'appRoot' (DashboardLoop.cpp) chama 'w.edlInput->TakeFocus()'
// a cada redesenho enquanto esta aba esta ativa.
//------------------------------------------------------------------------------
namespace app {

namespace {
using namespace ftxui;
} // namespace

Component buildEdlTab(DashboardWiring& w)
{
   // O texto ORIGINAL e' o que este processo carregou (lido uma vez, aqui);
   // 'w.editedEdlText' e' o buffer mutavel que o ftxui::Input escreve direto
   // -- nunca escrito de volta em 'w.generatedEdlPath'.
   w.originalEdlText = readEdlFileOrEmpty(w.generatedEdlPath);
   w.editedEdlText = w.originalEdlText;

   // ---- acoes da aba "EDL" -- as tres escrevem SEMPRE em
   // editedScenarioPath(), nunca em 'w.generatedEdlPath' (o arquivo que este
   // processo carregou) nem no '.edl.in' de origem -- "sem persistir no
   // arquivo real" e' a premissa da aba inteira. ----
   w.doEdlRevert = [&w] {
      w.editedEdlText = w.originalEdlText;
      w.edlHasStatus = false;
      w.edlStatusMessage.clear();
   };
   w.doEdlValidate = [&w] {
      writeEdlFile(editedScenarioPath(), w.editedEdlText);
      const EdlValidationResult r{runEdlCheck(edlcheckSiblingPath(), editedScenarioPath())};
      w.edlHasStatus = true;
      w.edlStatusOk = r.ok;
      w.edlStatusMessage = r.message;
   };
   // "Rodar" reaproveita a MESMA validacao de 'doEdlValidate' antes de sair
   // do laco -- nunca reexecuta com um '.edl' que o oraculo ja rejeitou.
   w.doEdlRun = [&w] {
      writeEdlFile(editedScenarioPath(), w.editedEdlText);
      const EdlValidationResult r{runEdlCheck(edlcheckSiblingPath(), editedScenarioPath())};
      w.edlHasStatus = true;
      w.edlStatusOk = r.ok;
      w.edlStatusMessage = r.message;
      if (!r.ok) return;
      w.action = DashboardExit::RunEdited;
      if (w.screen != nullptr) w.screen->Exit();
   };

   // 'cursor_position' precisa de armazenamento PROPRIO ('w.edlCursorPos')
   // porque o 'transform' abaixo -- fora da classe Input -- precisa ler a
   // posicao atual pra saber ONDE marcar o glifo do cursor na previa
   // colorida (ver renderHighlightedEdlText()).
   InputOption edlInputOpt;
   edlInputOpt.multiline = true;
   edlInputOpt.placeholder = "(cenario vazio -- " + w.generatedEdlPath + " nao pode ser lido)";
   edlInputOpt.cursor_position = &w.edlCursorPos;
   // Destaque de sintaxe SEMPRE que ha' texto -- 'w.edlInput->TakeFocus()'
   // (chamado em 'appRoot', DashboardLoop.cpp) roda a CADA redesenho
   // enquanto esta aba esta em cena, entao 'state.focused' aqui e'
   // efetivamente PERMANENTE.
   edlInputOpt.transform = [&w](InputState state) -> Element {
      if (state.is_placeholder) return InputOption::Default().transform(state);
      return renderHighlightedEdlText(w.editedEdlText, w.edlCursorPos, state.focused, state.hovered);
   };
   w.edlInput = Input(&w.editedEdlText, edlInputOpt);

   const Component btnEdlValidate{makeButton("[F8] Validar", w.doEdlValidate)};
   const Component btnEdlRun{makeButton("[F9] Rodar versao editada", w.doEdlRun)};
   const Component btnEdlRevert{makeButton("[F10] Reverter", w.doEdlRevert)};
   const Component edlButtons{Container::Horizontal({btnEdlValidate, btnEdlRun, btnEdlRevert})};

   const Component edlBody{Container::Vertical({w.edlInput, edlButtons})};
   return Renderer(edlBody, [&w, edlButtons]() -> Element {
      const bool dirty{w.editedEdlText != w.originalEdlText};

      // Indicador de foco DESENHADO PELO APP -- alem do cursor nativo do
      // terminal, porque nem todo terminal/multiplexador honra a troca de
      // estilo de cursor.
      const bool edlFocused{w.edlInput->Focused()};

      Element statusLine{text("(ainda nao validado nesta sessao)") | dim};
      if (w.edlHasStatus) {
         statusLine = paragraphAlignLeft((w.edlStatusOk ? "OK -- " : "INVALIDO -- ") + w.edlStatusMessage)
                     | color(w.edlStatusOk ? Color::Green : Color::Red) | bold;
      }

      Element editorBox{w.edlInput->Render() | vscroll_indicator | frame | flex | border};
      if (edlFocused) editorBox = editorBox | color(Color::Blue);

      return vbox({
         hbox({
            text(" carregado de " + w.generatedEdlPath + " ") | dim,
            filler(),
            text(edlFocused ? " EDITANDO -- cursor ativo " : " clique no texto para editar ")
               | (edlFocused ? (bgcolor(Color::Blue) | color(Color::White) | bold) : dim),
            text(dirty ? " editado, em memoria " : " sem alteracoes ")
               | (dirty ? (bgcolor(Color::Yellow) | color(Color::Black) | bold) : dim),
         }),
         separator(),
         editorBox,
         separator(),
         statusLine,
         text("as alteracoes ficam SO em memoria: nunca sao escritas no cenario original. "
              "[F8]/[Validar] roda o oraculo 'edlcheck'; [F9]/[Rodar versao editada] "
              "reexecuta o app com o texto atual (via -f, o mesmo caminho de uma fixture de "
              "teste), sem tocar em nenhum '.edl'/'.edl.in' de origem; "
              "[F10]/[Reverter] descarta a edicao. Roda do mouse rola o texto. "
              "O texto aparece colorido por sintaxe (mesma paleta do editor grafico web, "
              "src/ui/edl-builder.html); clique no meio do texto foca/rola normalmente, "
              "so' pode nao acertar o caractere exato -- use as setas para posicionar o "
              "cursor com precisao.") | dim,
         edlButtons->Render(),
      });
   });
}

} // namespace app
