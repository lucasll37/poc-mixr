#include "app/ScenarioTemplate.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

namespace app {

namespace {

const char* const PLACEHOLDER{"@NUM_TC_THREADS@"};

// Onde os fragmentos EDL compartilhados (ver app/configs/fragments/) moram
// -- caminho relativo, como todo o resto do projeto: os binarios rodam a
// partir da RAIZ do repositorio (convencao documentada no CLAUDE.md).
const char* const kFragmentsDir{"./app/configs/fragments"};

// Uma thread T/C por nucleo, menos a que roda o laco de background; no
// maximo 4 por padrao. O '-threads N' do usuario ainda e limitado pelo
// numero de nucleos -- pedir mais threads do que ha CPUs so acrescenta
// troca de contexto.
int resolveTcThreadCount(const int threadsOverride, unsigned int* const hwThreadsOut)
{
   const unsigned int hwThreads{std::thread::hardware_concurrency()};
   *hwThreadsOut = hwThreads;

   const int maxByCpu{static_cast<int>(hwThreads > 1 ? hwThreads - 1 : 1)};
   const int wanted{(threadsOverride > 0) ? threadsOverride : std::min(maxByCpu, 8)};
   return std::max(1, std::min(wanted, maxByCpu));
}

std::string readFileOrDie(const std::string& path)
{
   // Achado rodando (stress-sweep desta sessao), dois sintomas da MESMA
   // causa: 'in.rdbuf()' le ate o EOF sem checar QUE TIPO de arquivo e' --
   // um caractere-dispositivo de leitura infinita ('-f /dev/zero') nunca
   // da EOF e o buffer cresce ate o processo morrer com std::bad_alloc
   // (SIGABRT, core dump -- nao um EXIT_FAILURE limpo); um FIFO sem
   // escritor ('-f <pipe-nomeado>') trava o proprio std::ifstream(path) NO
   // OPEN, antes mesmo de chegar no rdbuf() -- sem timeout interno, so kill
   // externo resolve. is_regular_file() rejeita os dois ANTES de abrir,
   // com a mesma mensagem clara ja usada para "arquivo nao existe".
   std::error_code ec;
   if (!std::filesystem::is_regular_file(path, ec)) {
      std::cerr << "[main] nao consegui ler " << path
                << " (nao e um arquivo regular)" << std::endl;
      std::exit(EXIT_FAILURE);
   }
   std::ifstream in(path);
   if (!in.good()) {
      std::cerr << "[main] nao consegui ler " << path << std::endl;
      std::exit(EXIT_FAILURE);
   }
   std::ostringstream buf;
   buf << in.rdbuf();
   return buf.str();
}

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
   std::size_t pos{};
   while ((pos = text.find(from, pos)) != std::string::npos) {
      text.replace(pos, from.size(), to);
      pos += to.size();
   }
}

// Acha '@include:NOME@' e troca pelo conteudo de 'kFragmentsDir/NOME' --
// roda ANTES da substituicao de token (PLACEHOLDER/extraTokens), assim um
// fragmento pode conter tokens que so fazem sentido resolvidos depois do
// include (ex.: '@SCENARIO_ID@' dentro de tacview_recorder.edl.frag). O
// parser EDL nunca ve nem '@include:...@' nem '@TOKEN@' -- a mesma garantia
// que ja vale para '@NUM_TC_THREADS@' desde sempre: a resolucao acontece
// aqui, no .edl.in -> .edl, nao no '.edl' final que o edl_parser le.
//
// Deliberadamente RASO -- um nivel so, sem include recursivo -- para nao
// reintroduzir a complexidade que a auxsencia do preprocessador C evitou
// (ver a armadilha do acento em comentario, documentada no CLAUDE.md).
std::string expandIncludes(std::string text, const std::string& fragmentsDir)
{
   const std::string marker{"@include:"};
   std::size_t pos{};
   while ((pos = text.find(marker, pos)) != std::string::npos) {
      const std::size_t nameStart{pos + marker.size()};
      // O '@' de fechamento tem que estar na MESMA linha -- EDL e' linha a
      // linha, e sem este limite um '@include:' com o '@' de fechamento
      // ESQUECIDO "vaza" ate o proximo '@' de QUALQUER token seguinte no
      // arquivo. Achado rodando (stress-sweep desta sessao): isso funde
      // dois marcadores, produz um nome de fragmento com quebra de linha
      // embutida (mensagem de erro ilegivel) e engole o segundo include
      // inteiro sem processa-lo -- o antigo fallback "deixa como esta" so
      // disparava quando NAO SOBRAVA nenhum outro '@' no resto do arquivo,
      // o que e raro em EDL real.
      const std::size_t lineEnd{text.find('\n', nameStart)};
      const std::size_t nameEnd{text.find('@', nameStart)};
      if (nameEnd == std::string::npos || (lineEnd != std::string::npos && nameEnd > lineEnd)) {
         const auto linha{1 + std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(pos), '\n')};
         std::cerr << "[main] '@include:' sem '@' de fechamento na mesma linha (linha "
                   << linha << ")." << std::endl;
         std::exit(EXIT_FAILURE);
      }
      const std::string fragName{text.substr(nameStart, nameEnd - nameStart)};
      const std::string fragText{readFileOrDie(fragmentsDir + "/" + fragName)};
      text.replace(pos, (nameEnd + 1) - pos, fragText);
      pos += fragText.size();
   }
   return text;
}

// Nao ha default para '@TOKEN@' (ver o comentario de generateScenario() no
// .hpp) -- um token sem par em 'extraTokens' sobra LITERAL no texto final e
// vira "syntax error" do edl_parser, sem dizer qual token faltou. Isto
// varre o resultado ANTES de escrever o '.generated.edl' e avisa, com o
// nome do token, enquanto ainda da pra apontar a causa real. So conta como
// "token perdido" um '@...@' no MESMO formato que todo token deste
// mecanismo ja usa (MAIUSCULAS/digitos/underscore, curto) -- um '@' solto
// no meio de texto comum (comentario, email) nao e' isto, e' so pontuacao.
// Deliberadamente um AVISO, nao um die(): o mecanismo em si e tolerante
// (substitui so quem tem par), e forcar fatal aqui mudaria esse contrato.
void warnUnresolvedTokens(const std::string& text)
{
   std::size_t pos{};
   while ((pos = text.find('@', pos)) != std::string::npos) {
      const std::size_t end{text.find('@', pos + 1)};
      if (end == std::string::npos) break;
      const std::string inner{text.substr(pos + 1, end - pos - 1)};
      const bool pareceToken{!inner.empty() && inner.size() < 40 &&
         std::all_of(inner.begin(), inner.end(), [](const char c) {
            return (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9');
         })};
      if (pareceToken) {
         std::cerr << "[main] AVISO: '@" << inner << "@' sobrou sem substituicao no"
                   << " .generated.edl -- token ausente de 'extraTokens'? (o edl_parser"
                   << " provavelmente vai recusar isto com \"syntax error\", sem dizer"
                   << " o motivo real)" << std::endl;
      }
      pos = end + 1;
   }
}

} // namespace

int generateScenario(const std::string& templatePath, const std::string& outPath,
                     const int threadsOverride, const std::map<std::string, std::string>& extraTokens)
{
   unsigned int hwThreads{};
   const int numTcThreads{resolveTcThreadCount(threadsOverride, &hwThreads)};

   std::string text{readFileOrDie(templatePath)};
   text = expandIncludes(std::move(text), kFragmentsDir);
   replaceAll(text, PLACEHOLDER, std::to_string(numTcThreads));
   for (const auto& [name, value] : extraTokens) replaceAll(text, "@" + name + "@", value);
   warnUnresolvedTokens(text);

   std::ofstream out(outPath);
   out << text;

   std::cout << "[main] numTcThreads=" << numTcThreads
             << " (hardware_concurrency=" << hwThreads << ")" << std::endl;
   return numTcThreads;
}

} // namespace app
