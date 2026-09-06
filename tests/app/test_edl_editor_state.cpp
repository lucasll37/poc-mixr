#include "app/EdlEditorState.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>

// A parte SEM FTXUI da aba "EDL" (F7, ver app/DashboardLoop.cpp): io de
// arquivo e a interpretacao do exit code do oraculo 'edlcheck'. Nao testa
// 'edlcheckSiblingPath()' (depende de /proc/self/exe do PROPRIO binario de
// teste, nao do 'app' -- mesma razao de app/Respawn.cpp nao ter teste
// dedicado) nem sobe o 'edlcheck' de verdade -- 'runEdlCheck()' recebe o
// caminho do binario por parametro exatamente para poder ser testado contra
// programas triviais (/bin/true, /bin/false), sem depender do MIXR.

namespace {

// Mesmo padrao de tests/app/test_log_panel.cpp -- mkdtemp() em vez de um
// nome inventado, para nao correr entre gerar o nome e criar o diretorio.
class DiretorioTemporario {
public:
   DiretorioTemporario()
   {
      char molde[]{"/tmp/edl-editor-teste-XXXXXX"};
      const char* const dir{::mkdtemp(molde)};
      caminho_ = (dir != nullptr) ? dir : "";
   }
   ~DiretorioTemporario()
   {
      if (!caminho_.empty()) {
         std::system(("rm -rf " + caminho_).c_str());
      }
   }

   const std::string& caminho() const { return caminho_; }

private:
   std::string caminho_;
};

} // namespace

TEST(EdlEditorState, CaminhoDeTrabalhoEhSempreOMesmo)
{
   EXPECT_EQ(app::editedScenarioPath(), app::editedScenarioPath());
   EXPECT_FALSE(app::editedScenarioPath().empty());
   // Nunca aponta pra dentro de 'configs/' (onde moram o '.edl.in' e o
   // '.generated.edl' de producao) -- e o que garante "sem persistir no
   // arquivo real".
   EXPECT_EQ(app::editedScenarioPath().find("configs/"), std::string::npos);
}

TEST(EdlEditorState, ArquivoInexistenteLeComoVazio)
{
   EXPECT_EQ(app::readEdlFileOrEmpty("/caminho/que/nao/existe/em/lugar/nenhum.edl"), "");
}

TEST(EdlEditorState, EscreveELeDeVolta)
{
   const DiretorioTemporario dir;
   ASSERT_FALSE(dir.caminho().empty());
   const std::string path{dir.caminho() + "/edited.edl"};

   ASSERT_TRUE(app::writeEdlFile(path, "( Station )\n"));
   EXPECT_EQ(app::readEdlFileOrEmpty(path), "( Station )\n");

   // Uma segunda escrita SUBSTITUI, nao concatena.
   ASSERT_TRUE(app::writeEdlFile(path, "outra coisa"));
   EXPECT_EQ(app::readEdlFileOrEmpty(path), "outra coisa");
}

TEST(EdlEditorState, CriaODiretorioPaiSeFaltar)
{
   const DiretorioTemporario dir;
   ASSERT_FALSE(dir.caminho().empty());
   const std::string path{dir.caminho() + "/subpasta/nova/edited.edl"};

   ASSERT_TRUE(app::writeEdlFile(path, "conteudo"));
   EXPECT_EQ(app::readEdlFileOrEmpty(path), "conteudo");
}

TEST(EdlEditorState, RunEdlCheckOkQuandoOBinarioSaiComExitoZero)
{
   const app::EdlValidationResult r{app::runEdlCheck("/bin/true", "/qualquer/coisa.edl")};
   EXPECT_TRUE(r.ok);
}

TEST(EdlEditorState, RunEdlCheckFalhaQuandoOBinarioSaiComExitoNaoZero)
{
   const app::EdlValidationResult r{app::runEdlCheck("/bin/false", "/qualquer/coisa.edl")};
   EXPECT_FALSE(r.ok);
}

TEST(EdlEditorState, RunEdlCheckCapturaAMensagem)
{
   // '/bin/echo <edlFilePath>' -- confirma que stdout chega em 'message'
   // (sem \n final) e que 'edlFilePath' de fato vira o UNICO argumento do
   // binario, sem depender de um 'edlcheck' de verdade.
   const app::EdlValidationResult r{app::runEdlCheck("/bin/echo", "linha-de-teste-caminho.edl")};
   EXPECT_TRUE(r.ok);
   EXPECT_EQ(r.message, "linha-de-teste-caminho.edl");
}

TEST(EdlEditorState, RunEdlCheckBinarioInexistenteNaoTravaEDaFalha)
{
   const app::EdlValidationResult r{
      app::runEdlCheck("/binario/que/nao/existe/de/verdade", "/qualquer/coisa.edl")};
   EXPECT_FALSE(r.ok);
   EXPECT_FALSE(r.message.empty());
}
