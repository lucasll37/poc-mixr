// plugininfo -- introspeccao de RUNTIME de um plugin .so, para o catalogo
// do editor grafico de .edl (src/ui/scripts/generate_edl_catalog.py) saber
// quais CLASSES um plugin de TERCEIRO (sem fonte C++ neste repositorio, so
// o binario em plugins/) publica -- o gerador estatico (regex sobre
// .cpp/.hpp) nao tem como enxergar isso, por definicao: nao ha texto pra
// escanear.
//
// Reaproveita xplugin::loadModule() -- os MESMOS guardas de ABI que
// protegem uma carga de cenario de verdade (versao do contrato, tamanho de
// std::string, sizeof(models::Player), ...). NUNCA le PluginDescV1/
// MetaObject/SlotTable de um .so que falhou esses guardas: ler struct
// binaria de um .so com ABI incompativel e desalinhamento de memoria, nao
// so nome errado -- por isso este programa nao reimplementa dlopen/dlsym
// por conta propria, mesmo sendo bem menos codigo.
//
// O QUE DA PRA DESCOBRIR, e o PORQUE do limite (ver o comentario de
// generate_edl_catalog.py que consome esta saida): NOME de classe/fabrica
// e a CADEIA de heranca (via MetaObject::baseMetaObject) sempre que o
// plugin popular 'metas' no MIXR_PLUGIN_DEFINE(...) -- nomes de SLOT
// tambem, via MetaObject::slottable (SlotTable::n()/name()). NUNCA tipo ou
// unidade de slot: essa informacao so existe como CODIGO (o
// dynamic_cast<ObjType*> dentro de setSlotByIndex(), gerado pela macro
// ON_SLOT), nunca como dado -- nao ha API de runtime que devolva isso, so
// o fonte .cpp (que um terceiro, por definicao, nao entrega).
//
// Uma classe cujo nome de fabrica NAO tem MetaObject correspondente
// ('metas' parcial, ou nullptr inteiro -- o ABI permite os dois, ver
// PluginAbi.hpp:96) simplesmente nao aparece na saida -- silencioso, nao
// um erro: e' uma limitacao do PLUGIN (nao populou metas), nao deste
// programa.
//
// Saida: JSON em stdout, uma execucao bem sucedida por linha de comando.
// TODA falha de carga (ABI incompativel, .so que nao e' plugin MIXR, etc.)
// e' FATAL (xplugin::loadModule() ja' morre sozinho, ver o cabecalho de
// PluginRegistry.cpp) -- generate_edl_catalog.py trata exit code != 0 como
// "pula este .so", nunca como erro do pipeline inteiro.
//
// Uso: plugininfo <caminho/para/plugin.so>

#include "xplugin/PluginRegistry.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/SlotTable.hpp"

#include <cxxabi.h>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

// MetaObject::getClassName() devolve o nome cru de 'type_info' (o proprio
// header documenta isso -- "class name from 'type_info'"), que no
// GCC/Linux e' o nome MANGLED (ex.: "N4mixr6models5xstub13AlertDatalinkE"),
// nao "AlertDatalink" -- confirmado rodando ANTES deste demangle existir.
// abi::__cxa_demangle() e' a mesma API que 'c++filt'/'nm -C' usam por
// baixo. Sem ela, o catalogo mostraria lixo ilegivel no lugar do nome da
// classe. Mantem so' o ULTIMO segmento apos '::' -- mesma convencao de
// build_inheritance() em tools/mixr_source_scan.py (guarda so' o ultimo
// segmento do namespace, por isso "mixr::models::xstub::AlertDatalink"
// vira so' "AlertDatalink").
std::string demangleClassName(const char* mangled)
{
   int status{};
   char* const demangled{abi::__cxa_demangle(mangled, nullptr, nullptr, &status)};
   std::string result{(status == 0 && demangled != nullptr) ? std::string{demangled} : std::string{mangled}};
   if (demangled != nullptr) std::free(demangled);
   const auto pos = result.rfind("::");
   if (pos != std::string::npos) result = result.substr(pos + 2);
   return result;
}

// Do mais derivado pro mais base -- MESMA ordem de resolve_chain() em
// generate_edl_catalog.py (chain[0] == a propria classe).
std::vector<std::string> chainOf(const mixr::base::MetaObject* meta)
{
   std::vector<std::string> chain;
   for (const mixr::base::MetaObject* m = meta; m != nullptr; m = m->baseMetaObject) {
      chain.push_back(demangleClassName(m->getClassName()));
   }
   return chain;
}

// SlotTable::n()/name() ja' vem ACHATADO pela heranca (o proprio header
// documenta isso) -- nao precisa repetir a subida por baseMetaObject aqui.
std::vector<std::string> slotsOf(const mixr::base::MetaObject* meta)
{
   std::vector<std::string> names;
   if (meta->slottable == nullptr) return names;
   const int n = meta->slottable->n();
   for (int i = 1; i <= n; ++i) {
      const char* const name = meta->slottable->name(i);
      if (name != nullptr) names.emplace_back(name);
   }
   return names;
}

std::string jsonEscape(const std::string& s)
{
   std::string out;
   out.reserve(s.size());
   for (const char c : s) {
      if (c == '"' || c == '\\') out += '\\';
      out += c;
   }
   return out;
}

void printJsonArray(const std::vector<std::string>& items)
{
   std::cout << "[";
   for (std::size_t j = 0; j < items.size(); ++j) {
      if (j > 0) std::cout << ", ";
      std::cout << "\"" << jsonEscape(items[j]) << "\"";
   }
   std::cout << "]";
}

} // namespace

int main(int argc, char* argv[])
{
   if (argc != 2) {
      std::cerr << "uso: plugininfo <caminho/para/plugin.so>" << std::endl;
      return 2;
   }
   const std::string path{argv[1]};

   // provides={}: pula a assercao da etapa 8 de loadModule() -- este
   // programa quer DESCOBRIR o que o .so declara, nao confirmar contra um
   // 'provides:' de .edl que nao existe aqui. searchPaths={}: 'path' e'
   // sempre passado explicito (a chamadora, generate_edl_catalog.py,
   // sempre resolve o caminho absoluto antes de invocar este binario).
   // setBuiltinFactory() nunca e' chamado -- builtinFactory_ fica nullptr
   // e PluginRegistry::loadModule() ja' trata esse caso (pula so' a sonda
   // de colisao com a factory nativa, que nao faz sentido fora de uma
   // Station de verdade).
   //
   // loadModule() ESCREVE um banner de sucesso ("[plugin] carregado ...")
   // em std::cout, nao std::cerr (PluginRegistry.cpp) -- do lado de uma
   // Station de verdade isso e' informativo, mas aqui contaminaria a saida
   // JSON. Redireciona std::cout pra um buffer descartavel so' durante a
   // chamada; falha (loadModule morre com die()/std::exit) escreve em
   // std::cerr e nunca chega aqui, entao nao precisa de try/restauracao.
   {
      std::ostringstream discard;
      std::streambuf* const oldCout{std::cout.rdbuf(discard.rdbuf())};
      mixr::xplugin::loadModule(path, {}, {});
      std::cout.rdbuf(oldCout);
   }

   const std::vector<const mixr::base::MetaObject*>& metas = mixr::xplugin::pluginMetaObjects();

   std::cout << "{\n  \"classes\": [\n";
   for (std::size_t i = 0; i < metas.size(); ++i) {
      const mixr::base::MetaObject* const meta = metas[i];
      std::cout << "    {\n";
      std::cout << "      \"factory\": \"" << jsonEscape(meta->getFactoryName()) << "\",\n";
      std::cout << "      \"class\": \"" << jsonEscape(demangleClassName(meta->getClassName())) << "\",\n";
      std::cout << "      \"chain\": ";
      printJsonArray(chainOf(meta));
      std::cout << ",\n      \"slots\": ";
      printJsonArray(slotsOf(meta));
      std::cout << "\n    }" << (i + 1 < metas.size() ? "," : "") << "\n";
   }
   std::cout << "  ]\n}" << std::endl;
   return 0;
}
