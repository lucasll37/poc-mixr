// mixr::xmsg::RecordWriter -- monta NDJSON num buffer fixo, sem alocar.
// Sem MIXR, sem Station -- so a classe e um arquivo em disco.
//
// Achado por auditoria (workflow de investigacao desta sessao, dimensao
// 'libs-individual'): addLabel() concatenava 'value' CRU dentro das aspas
// JSON, sem escapar. 'value' pode vir do nome de um player recebido por DIS
// (EntityMarking::marking -- 11 bytes crus controlados por quem envia o PDU
// na rede local), entao um nome malicioso injetava um campo JSON inteiro na
// linha gravada. Este arquivo prova o escape, e que ele nao muda o caso
// feliz (nome comum, sem caractere especial).
#include "xmsg/RecordWriter.hpp"

#include <gtest/gtest.h>

#include <string>

using mixr::xmsg::RecordWriter;

namespace {

// Monta uma linha de UM campo e devolve o JSON completo, ja terminado.
std::string linhaComUmLabel(const char* const key, const char* const value)
{
   RecordWriter w;
   w.begin(1.0, "teste");
   w.addLabel(key, value);
   w.end();
   return std::string(w.data(), w.size());
}

TEST(RecordWriter, NomeComumPassaIntacto)
{
   const std::string json{linhaComUmLabel("player", "falcon1")};
   EXPECT_NE(json.find(R"("player":"falcon1")"), std::string::npos) << json;
}

TEST(RecordWriter, AspaNoValorNaoQuebraOCampoNemInjetaChaveNova)
{
   // O payload do achado original: fecha a aspa e injeta "q":1 como se fosse
   // outro campo do objeto.
   const std::string json{linhaComUmLabel("player", R"(x","q":1)")};

   EXPECT_EQ(json.find("\"q\":1"), std::string::npos)
      << "o campo injetado nao pode aparecer como chave de primeira classe: " << json;

   // O documento inteiro continua JSON valido com EXATAMENTE os campos que
   // RecordWriter escreveu (t, msg, player) -- nao mais, nao menos. Um
   // parser JSON de verdade seria o teste mais forte; aqui, contar aspas
   // nao-escapadas fora de par basta para confirmar que a string ficou bem
   // formada (numero par de aspas delimitadoras, todo '"' interno com '\'
   // na frente).
   int abertas{};
   for (std::size_t i{}; i < json.size(); ++i) {
      if (json[i] == '"' && (i == 0 || json[i - 1] != '\\')) ++abertas;
   }
   EXPECT_EQ(abertas % 2, 0) << "aspas desbalanceadas: " << json;
}

TEST(RecordWriter, BarraInvertidaEEscapadaLiteralmente)
{
   const std::string json{linhaComUmLabel("player", R"(C:\rota)")};
   EXPECT_NE(json.find(R"(C:\\rota)"), std::string::npos) << json;
}

TEST(RecordWriter, NovaLinhaNaoQuebraOArquivoNdjson)
{
   // Um '\n' cru no valor criaria uma SEGUNDA linha no .jsonl -- cada
   // registro tem de ficar numa unica linha fisica.
   const std::string json{linhaComUmLabel("player", "a\nb")};
   EXPECT_EQ(json.find('\n'), std::string::npos) << json;
   EXPECT_NE(json.find(R"(a\nb)"), std::string::npos) << json;
}

TEST(RecordWriter, CaractereDeControleViraEscapeUnicode)
{
   const char valor[]{"a\x01" "b"};   // 0x01 -- controle, sem escape curto dedicado
   const std::string json{linhaComUmLabel("player", valor)};
   EXPECT_NE(json.find(R"(a\u0001b)"), std::string::npos) << json;
}

} // namespace
