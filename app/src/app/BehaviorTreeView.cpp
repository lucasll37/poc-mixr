#include "app/BehaviorTreeView.hpp"

#include <cctype>
#include <fstream>
#include <sstream>

namespace app {

namespace {

//------------------------------------------------------------------------------
// Parser minimo de XML -- so o suficiente pro formato do BT.CPP (tags
// aninhadas, comentarios, atributos que a gente ignora). NAO e um parser de
// XML geral (nao lida com CDATA, entidades, aspas com '>' dentro de
// atributo etc.) -- os arquivos de arvore deste repositorio nunca precisam
// disso, e nao vale a pena puxar uma biblioteca de XML so pra isto.
//------------------------------------------------------------------------------
struct XmlScanner
{
   const std::string& s;
   std::size_t i{};

   explicit XmlScanner(const std::string& text) : s(text) {}

   bool eof() const { return i >= s.size(); }

   void skipCommentsAndWs()
   {
      for (;;) {
         while (!eof() && std::isspace(static_cast<unsigned char>(s[i]))) i++;
         if (i + 4 <= s.size() && s.compare(i, 4, "<!--") == 0) {
            const auto end{s.find("-->", i)};
            i = (end == std::string::npos) ? s.size() : end + 3;
            continue;
         }
         if (i + 2 <= s.size() && s.compare(i, 2, "<?") == 0) {
            const auto end{s.find("?>", i)};
            i = (end == std::string::npos) ? s.size() : end + 2;
            continue;
         }
         break;
      }
   }

   struct TagInfo { std::string name; bool closing{}; bool selfClosing{}; };

   // Precondicao: s[i] == '<'. Consome ate depois do '>' correspondente.
   TagInfo readTag()
   {
      TagInfo info;
      i++;   // '<'
      if (!eof() && s[i] == '/') { info.closing = true; i++; }
      const std::size_t nameStart{i};
      while (!eof() && !std::isspace(static_cast<unsigned char>(s[i])) && s[i] != '>' && s[i] != '/') i++;
      info.name = s.substr(nameStart, i - nameStart);
      while (!eof() && s[i] != '>') {
         if (s[i] == '/' && i + 1 < s.size() && s[i + 1] == '>') info.selfClosing = true;
         i++;
      }
      if (!eof()) i++;   // '>'
      return info;
   }
};

BtNode parseElement(XmlScanner& sc)
{
   sc.skipCommentsAndWs();
   if (sc.eof() || sc.s[sc.i] != '<') return {};

   const auto tag{sc.readTag()};
   if (tag.closing) return {};

   BtNode node;
   node.tag = tag.name;
   if (tag.selfClosing) return node;

   for (;;) {
      sc.skipCommentsAndWs();
      if (sc.eof()) break;
      if (sc.s[sc.i] != '<') { sc.i++; continue; }
      if (sc.i + 1 < sc.s.size() && sc.s[sc.i + 1] == '/') { sc.readTag(); break; }
      node.children.push_back(parseElement(sc));
   }
   return node;
}

std::string readFile(const std::string& path)
{
   std::ifstream f(path);
   if (!f) return "";
   std::ostringstream oss;
   oss << f.rdbuf();
   return oss.str();
}

std::string normalize(const std::string& s)
{
   std::string out;
   for (const char c : s) {
      if (std::isalnum(static_cast<unsigned char>(c))) {
         out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      }
   }
   return out;
}

// U+251C/U+2514/U+2502/U+2500 (box-drawing), 3 bytes cada em UTF-8.
const char* const kTee{"\xE2\x94\x9C\xE2\x94\x80\xE2\x94\x80 "};      // "├── "
const char* const kElbow{"\xE2\x94\x94\xE2\x94\x80\xE2\x94\x80 "};    // "└── "
const char* const kBar{"\xE2\x94\x82   "};                            // "│   "
const char* const kBlank{"    "};

void appendLine(std::vector<BtTreeLine>& lines, const BtNode& node, const std::string& prefix,
                const bool isLast)
{
   BtTreeLine line;
   line.tag = node.tag;
   line.display = prefix + (isLast ? kElbow : kTee) + node.tag;
   line.leaf = node.children.empty();
   lines.push_back(std::move(line));

   const std::string childPrefix{prefix + (isLast ? kBlank : kBar)};
   for (std::size_t i = 0; i < node.children.size(); i++) {
      appendLine(lines, node.children[i], childPrefix, i + 1 == node.children.size());
   }
}

}

bool isValid(const BtNode& node) { return !node.tag.empty(); }

bool matchesLabel(const std::string& tag, const std::string& label)
{
   if (label.empty() || label == "--") return false;
   const std::string nt{normalize(tag)};
   const std::string nl{normalize(label)};
   if (nt.empty() || nl.empty()) return false;
   return nt.find(nl) != std::string::npos || nl.find(nt) != std::string::npos;
}

BtNode parseBehaviorTreeXml(const std::string& xmlPath)
{
   const std::string content{readFile(xmlPath)};
   if (content.empty()) return {};

   XmlScanner sc{content};
   while (!sc.eof()) {
      sc.skipCommentsAndWs();
      if (sc.eof() || sc.s[sc.i] != '<') { if (!sc.eof()) sc.i++; continue; }
      if (sc.i + 1 < sc.s.size() && sc.s[sc.i + 1] == '/') { sc.readTag(); continue; }

      const auto tag{sc.readTag()};
      if (tag.name == "BehaviorTree") {
         if (tag.selfClosing) return {};
         return parseElement(sc);
      }
      // Outra tag container (ex.: '<root ...>') que so envolve a que
      // interessa -- nao precisa de tratamento especial: o laco continua
      // varrendo em frente e encontra '<BehaviorTree>' do mesmo jeito.
   }
   return {};
}

BtNode loadTreeForScenario(const std::string& generatedEppPath)
{
   const std::string content{readFile(generatedEppPath)};
   if (content.empty()) return {};

   const auto pos{content.find("treeFile:")};
   if (pos == std::string::npos) return {};
   const auto q1{content.find('"', pos)};
   if (q1 == std::string::npos) return {};
   const auto q2{content.find('"', q1 + 1)};
   if (q2 == std::string::npos) return {};

   return parseBehaviorTreeXml(content.substr(q1 + 1, q2 - q1 - 1));
}

std::vector<BtTreeLine> flattenBehaviorTree(const BtNode& root)
{
   std::vector<BtTreeLine> lines;
   if (!isValid(root)) return lines;

   BtTreeLine rootLine;
   rootLine.tag = root.tag;
   rootLine.display = root.tag;
   rootLine.leaf = root.children.empty();
   lines.push_back(std::move(rootLine));

   for (std::size_t i = 0; i < root.children.size(); i++) {
      appendLine(lines, root.children[i], "", i + 1 == root.children.size());
   }
   return lines;
}

} // namespace app
