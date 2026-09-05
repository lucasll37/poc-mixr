#include "xplugin/PluginRegistry.hpp"

#include "xplugin/PluginAbi.hpp"

#include "mixr/base/MetaObject.hpp"
#include "mixr/base/Object.hpp"

// Para o canario de layout: o host compara o proprio sizeof(models::Player)
// com o que o plugin gravou no descritor.
#include "mixr/models/player/Player.hpp"

#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

//------------------------------------------------------------------------------
// POLITICA DE FALHA: toda falha de plugin e FATAL (std::exit).
//
// Segue a convencao que o repo ja tem -- ensureTerrainData, generateScenario,
// buildStation, worldModelOf e collectFleet todos matam o processo; so
// clockStationOf/ioHandlerOf/tacviewOutputOf avisam e seguem, porque naqueles
// casos o cenario pode legitimamente nao querer aquilo.
//
// Um ( PluginLoader ) no .edl e uma declaracao explicita de intencao: nao
// existe leitura razoavel de "o cenario pediu o plugin, ele nao carregou,
// siga sem". E avisar-e-seguir produziria exatamente a patologia de
// contexts/BTCPP-CONTEXT.md:2358 -- o aviso rola para fora da tela e a falha
// real reaparece depois, disfarcada de erro de cenario.
//
// FLAGS DO dlopen: RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE.
//
//   RTLD_NOW      resolve tudo agora. Com b_lundef=true o caminho feliz seria
//                 equivalente ao LAZY, mas: (a) um plugin construido FORA do
//                 nosso meson passa a falhar aqui, com o nome do simbolo, em
//                 vez de estourar dentro de uma thread de tempo critico;
//                 (b) o momento da falha deixa de depender do caminho de
//                 execucao, o que envenenaria check_determinism.sh; e (c) um
//                 plugin compilado COM ASan carregado num host SEM ASan vira
//                 "undefined symbol: __asan_report_load8" limpo, na partida.
//
//   RTLD_LOCAL    contra o prior art do BehaviorTree.CPP, que usa GLOBAL
//                 (BTCPP-CONTEXT.md:8641). O comentario que a BT.CPP herdou do
//                 POCO -- "RTTI nao funciona para tipos definidos na shared
//                 library" -- esta obsoleto para o nosso caso: no Linux/GCC
//                 __GXX_MERGED_TYPEINFO_NAMES == 0, entao
//                 type_info::operator== cai em strcmp, e o payload real da .so
//                 entregue nao tem o '*' inicial que forcaria comparacao por
//                 endereco. Alem disso o escopo de busca de um objeto
//                 RTLD_LOCAL JA INCLUI o escopo global (executavel +
//                 libmixr_*.so), entao o plugin enxerga o MIXR normalmente.
//
//                 E RTLD_GLOBAL traria risco real: Player::metaObject e
//                 Player::slottable, embora 'private:' em C++, sao simbolos
//                 GLOBAL OBJECT no .dynsym. Dois plugins independentes que
//                 declarassem classes de mesmo nome mangled se interporiam em
//                 silencio -- contadores de instancia fundidos, tabelas de
//                 slot cruzadas, clone() construindo a classe errada.
//
//   RTLD_NODELETE redundante com "nunca chamar dlclose", e e por isso que
//                 vale: torna a politica propriedade do CARREGAMENTO, e nao
//                 da disciplina de quem escrever codigo aqui depois.
//
// NUNCA dlclose. O argumento e concreto, nao cautela generica:
//   1. STANDARD_CONSTRUCTOR() faz 'slotTable = &slottable' -- TODA instancia
//      viva guarda um ponteiro para o .data do plugin;
//   2. SlotTable guarda 'baseTable', encadeando para dentro do plugin;
//   3. o ponteiro de vtable de toda instancia aponta para o .data.rel.ro dele;
//   4. STANDARD_DESTRUCTOR() faz 'metaObject.count--' -- ESCREVE na imagem do
//      plugin, no momento da destruicao;
//   5. app/MetaObjectReport le esses MetaObject no fim da execucao.
// O main.cpp so desmonta a arvore em station->unref(), ultima linha antes do
// return. Qualquer dlclose antes disso e use-after-unmap; depois disso nao
// sobra motivo. Mesma conclusao a que o BehaviorTree.CPP chegou
// (BTCPP-CONTEXT.md:7257).
//
// CONSEQUENCIA que precisa estar dita: "sem recompilar tudo" -- sim.
// "sem reiniciar o processo" -- NAO. Recarregar um plugin e reiniciar.
//------------------------------------------------------------------------------

namespace mixr {
namespace xplugin {

namespace {

struct Loaded
{
   std::string resolvedPath;
   std::string pluginName;
   const PluginDescV1* desc{};
};

std::vector<Loaded>& loaded()
{
   static std::vector<Loaded> v;
   return v;
}

std::unordered_map<std::string, factory_fn>& registry()
{
   static std::unordered_map<std::string, factory_fn> m;
   return m;
}

std::vector<const base::MetaObject*>& metas()
{
   static std::vector<const base::MetaObject*> v;
   return v;
}

base::factory_func builtinFactory_{};
bool sealed_{};

[[noreturn]] void die()
{
   std::cerr << "[plugin] carga abortada." << std::endl;
   std::exit(EXIT_FAILURE);
}

std::string cwd()
{
   char buf[4096]{};
   return (::getcwd(buf, sizeof(buf)) != nullptr) ? std::string{buf} : std::string{"(desconhecido)"};
}

// Devolve o caminho canonico, ou string vazia se o arquivo nao existe.
std::string canonical(const std::string& path)
{
   char* const p{::realpath(path.c_str(), nullptr)};
   if (p == nullptr) return {};
   std::string out{p};
   std::free(p);
   return out;
}

// Resolve 'file' contra 'searchPaths', na ordem. Um 'file' que ja comeca com
// '/', './' ou '../' e tentado como veio, primeiro.
std::string resolve(const std::string& file,
                    const std::vector<std::string>& searchPaths,
                    std::vector<std::string>& tried)
{
   const bool explicito{!file.empty() &&
      (file[0] == '/' || file.compare(0, 2, "./") == 0 || file.compare(0, 3, "../") == 0)};

   if (explicito) {
      tried.push_back(file);
      const std::string c{canonical(file)};
      if (!c.empty()) return c;
   }

   for (const std::string& dir : searchPaths) {
      std::string cand{dir};
      if (!cand.empty() && cand.back() != '/') cand += '/';
      cand += file;
      tried.push_back(cand);
      const std::string c{canonical(cand)};
      if (!c.empty()) return c;
   }

   if (!explicito) {
      // Ultimo recurso: o caminho como veio, relativo ao cwd.
      tried.push_back(file);
      const std::string c{canonical(file)};
      if (!c.empty()) return c;
   }
   return {};
}

// dlsym com o idioma correto: dlerror() e thread-local E E LIMPO PELA LEITURA,
// e um simbolo pode legitimamente valer nullptr -- so o dlerror() decide.
void* symbol(void* const handle, const char* const name, std::string& err)
{
   ::dlerror();
   void* const sym{::dlsym(handle, name)};
   const char* const e{::dlerror()};
   err = (e != nullptr) ? std::string{e} : std::string{};
   return sym;
}

} // namespace

void setBuiltinFactory(const base::factory_func f)
{
   builtinFactory_ = f;
}

void seal()
{
   sealed_ = true;
}

const std::vector<const base::MetaObject*>& pluginMetaObjects()
{
   return metas();
}

base::Object* loadedFactory(const std::string& name)
{
   const auto it = registry().find(name);
   return (it != registry().end()) ? it->second(name.c_str()) : nullptr;
}

void loadModule(const std::string& file,
                const std::vector<std::string>& searchPaths,
                const std::vector<std::string>& provides)
{
   if (sealed_) {
      std::cerr << "[plugin] erro de programacao: loadModule('" << file << "') depois do parse."
                << std::endl;
      std::cerr << "[plugin]   o registro so pode ser escrito durante o parse do .edl,"
                << " na thread principal." << std::endl;
      die();
   }

   // --- 1) resolucao de caminho ------------------------------------------
   std::vector<std::string> tried;
   const std::string path{resolve(file, searchPaths, tried)};
   if (path.empty()) {
      std::cerr << "[plugin] nao encontrei o modulo '" << file << "'." << std::endl;
      std::cerr << "[plugin]   procurei em:" << std::endl;
      for (const std::string& t : tried) std::cerr << "[plugin]     " << t << std::endl;
      std::cerr << "[plugin]   diretorio de trabalho: " << cwd() << std::endl;
      std::cerr << "[plugin]   os caminhos de 'searchPaths:' sao relativos a RAIZ do"
                << " repositorio, que e de onde as pocs rodam." << std::endl;
      die();
   }

   // --- 2) idempotencia ---------------------------------------------------
   // isValid() pode ser chamado mais de uma vez para o mesmo objeto
   // (Pair::isValid() repassa), entao carregar de novo nao pode duplicar.
   for (const Loaded& l : loaded()) {
      if (l.resolvedPath == path) return;
   }

   // --- 3) dlopen ---------------------------------------------------------
   void* const handle{::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE)};
   if (handle == nullptr) {
      const char* const e{::dlerror()};
      std::cerr << "[plugin] falha ao abrir '" << path << "':" << std::endl;
      std::cerr << "[plugin]   " << (e != nullptr ? e : "(dlerror vazio)") << std::endl;
      std::cerr << "[plugin]   conferir com:  ldd " << path << " | grep 'not found'" << std::endl;
      std::cerr << "[plugin]   lembrando que o executavel NAO exporta simbolos e que todo"
                << " plugin leva -Wl,--no-undefined:" << std::endl;
      std::cerr << "[plugin]   um plugin nao pode chamar codigo da aplicacao, so as classes"
                << " base do MIXR." << std::endl;
      die();
   }

   // --- 4) ponto de entrada ----------------------------------------------
   std::string err;
   void* const sym{symbol(handle, "mixr_plugin_v1", err)};
   if (!err.empty() || sym == nullptr) {
      std::cerr << "[plugin] '" << path << "' nao exporta 'mixr_plugin_v1'." << std::endl;
      if (!err.empty()) std::cerr << "[plugin]   dlerror: " << err << std::endl;
      std::cerr << "[plugin]   use a macro MIXR_PLUGIN_DEFINE de"
                << " shared/xplugin/PluginAbi.hpp -- ela emite o extern \"C\" com"
                << " visibilidade default." << std::endl;
      std::cerr << "[plugin]   a armadilha classica e a funcao acabar 'static' e sumir do"
                << " dlsym (BTCPP-CONTEXT.md:7248)." << std::endl;
      std::cerr << "[plugin]   conferir com:  nm -D --defined-only " << path
                << " | grep mixr_plugin_" << std::endl;
      die();
   }

   using entry_fn = const PluginDescV1* (*)();
   // void* -> ponteiro de funcao e formalmente UB em C++ mas exigido pelo
   // POSIX; so avisa sob -pedantic, e o build usa warning_level=1.
   const auto entry = reinterpret_cast<entry_fn>(sym);
   const PluginDescV1* const desc{entry()};
   if (desc == nullptr) {
      std::cerr << "[plugin] '" << path << "': mixr_plugin_v1() devolveu nulo." << std::endl;
      die();
   }

   const std::string nome{desc->plugin_name != nullptr ? desc->plugin_name : "(sem nome)"};

   // --- 5) guardas de ABI -- RECUSA ---------------------------------------
   const auto minimo = static_cast<std::uint32_t>(
      reinterpret_cast<const char*>(&desc->factory) - reinterpret_cast<const char*>(desc)
      + sizeof(factory_fn));
   if (desc->struct_size < minimo) {
      std::cerr << "[plugin] '" << path << "': descritor truncado (" << desc->struct_size
                << " bytes; o minimo utilizavel e " << minimo << ")." << std::endl;
      die();
   }
   if (desc->struct_size > sizeof(PluginDescV1)) {
      std::cerr << "[plugin] '" << path << "': descritor de " << desc->struct_size
                << " bytes, mas este binario so entende " << sizeof(PluginDescV1) << "."
                << std::endl;
      std::cerr << "[plugin]   o plugin foi compilado contra um SDK MAIS NOVO que o host."
                << std::endl;
      die();
   }
   if (desc->abi != PLUGIN_ABI) {
      std::cerr << "[plugin] '" << path << "': ABI do contrato " << desc->abi
                << ", este binario espera " << PLUGIN_ABI << "." << std::endl;
      std::cerr << "[plugin]   recompile o plugin contra este repositorio." << std::endl;
      die();
   }
   {
      const auto hostAbi = static_cast<std::uint32_t>(MIXR_PLUGIN_CXX11_ABI);
      if (desc->cxx11_abi == CXX11_ABI_DESCONHECIDA || hostAbi == CXX11_ABI_DESCONHECIDA) {
         std::cerr << "[plugin] AVISO: '" << nome << "' nao reporta _GLIBCXX_USE_CXX11_ABI;"
                   << " nao da para conferir o layout de std::string." << std::endl;
      } else if (desc->cxx11_abi != hostAbi) {
         std::cerr << "[plugin] '" << path << "': _GLIBCXX_USE_CXX11_ABI " << desc->cxx11_abi
                   << " != " << hostAbi << " deste binario." << std::endl;
         std::cerr << "[plugin]   std::string tem layout diferente dos dois lados, e os"
                   << " headers do MIXR expoem isso inline" << std::endl;
         std::cerr << "[plugin]   (MetaObject::getClassName() le um std::string MEMBRO)."
                   << " O sintoma seria corrupcao silenciosa." << std::endl;
         die();
      }
   }
   if (desc->player_size == 0) {
      std::cerr << "[plugin] AVISO: '" << nome << "' nao afere sizeof(models::Player)"
                << " (MIXR_PLUGIN_DEFINE_NOCANARY);" << std::endl;
      std::cerr << "[plugin]   a checagem de layout mais util esta desligada para ele."
                << std::endl;
   } else if (desc->player_size != static_cast<std::uint32_t>(sizeof(models::Player))) {
      std::cerr << "[plugin] '" << path << "': sizeof(models::Player) " << desc->player_size
                << " != " << sizeof(models::Player) << " deste binario." << std::endl;
      std::cerr << "[plugin]   os dois lados viram headers do MIXR diferentes."
                << " Recompile o plugin contra o mesmo pacote." << std::endl;
      die();
   }

   // --- 6) guardas informativas -- AVISO ----------------------------------
   if (desc->mixr_version != static_cast<std::uint32_t>(MIXR_VERSION)) {
      std::cerr << "[plugin] AVISO: '" << nome << "' viu MIXR_VERSION " << desc->mixr_version
                << ", este binario " << MIXR_VERSION << "." << std::endl;
   }
   {
      const std::string pkgPlugin{desc->mixr_pkg_version != nullptr ? desc->mixr_pkg_version : "?"};
      const std::string pkgHost{MIXR_PLUGIN_PKG_VERSION};
      if (pkgPlugin != pkgHost) {
         std::cerr << "[plugin] AVISO: '" << nome << "' compilado contra mixr " << pkgPlugin
                   << ", este binario contra " << pkgHost << "." << std::endl;
         std::cerr << "[plugin]   os SONAME do MIXR nao sao versionados (libmixr_base.so,"
                   << " sem .so.1.0.5), entao o loader" << std::endl;
         std::cerr << "[plugin]   nao detectaria isso sozinho." << std::endl;
      }
   }

   // --- 7) o que o plugin diz que responde --------------------------------
   if (desc->factory_names == nullptr || desc->factory_names[0] == nullptr) {
      std::cerr << "[plugin] '" << path << "': nao declara nenhum nome de fabrica." << std::endl;
      die();
   }
   if (desc->factory == nullptr) {
      std::cerr << "[plugin] '" << path << "': ponteiro de fabrica nulo." << std::endl;
      die();
   }

   std::vector<std::string> nomes;
   for (const char* const* p = desc->factory_names; *p != nullptr; ++p) nomes.emplace_back(*p);

   // --- 8) 'provides:' do .edl como ASSERCAO ------------------------------
   if (!provides.empty()) {
      std::vector<std::string> a{provides};
      std::vector<std::string> b{nomes};
      std::sort(a.begin(), a.end());
      std::sort(b.begin(), b.end());
      if (a != b) {
         std::cerr << "[plugin] '" << path << "': o 'provides:' do cenario nao bate com o que"
                   << " a .so entrega." << std::endl;
         std::cerr << "[plugin]   cenario declarou:";
         for (const std::string& s : a) std::cerr << " " << s;
         std::cerr << std::endl;
         std::cerr << "[plugin]   a .so entrega:   ";
         for (const std::string& s : b) std::cerr << " " << s;
         std::cerr << std::endl;
         std::cerr << "[plugin]   costuma ser uma .so velha esquecida num dos searchPaths."
                   << std::endl;
         die();
      }
   }

   // --- 9) colisao ---------------------------------------------------------
   // A ordem na cadeia (plugin por ULTIMO) faz o plugin so ACRESCENTAR nomes.
   // Mas a ordem sozinha deixaria o plugin silenciosamente inerte, entao a
   // defesa real e esta sonda, aqui, com os dois donos nomeados.
   for (const std::string& n : nomes) {
      const auto it = registry().find(n);
      if (it != registry().end()) {
         std::string dono{"(desconhecido)"};
         for (const Loaded& l : loaded()) {
            for (const char* const* p = l.desc->factory_names; *p != nullptr; ++p) {
               if (n == *p) dono = l.pluginName + " (" + l.resolvedPath + ")";
            }
         }
         std::cerr << "[plugin] '" << path << "': o nome '" << n << "' ja foi registrado por "
                   << dono << "." << std::endl;
         std::cerr << "[plugin]   quem ganharia dependeria da ordem de carga -- renomeie a"
                   << " classe de um dos dois." << std::endl;
         die();
      }
      if (builtinFactory_ != nullptr) {
         base::Object* const probe{builtinFactory_(n)};
         if (probe != nullptr) {
            probe->unref();
            std::cerr << "[plugin] '" << path << "': o nome de fabrica '" << n
                      << "' JA e construido pelo framework ou pelas factories locais."
                      << std::endl;
            std::cerr << "[plugin]   o registro de plugins e consultado por ULTIMO, entao esse"
                      << " nome nunca chegaria ao plugin." << std::endl;
            std::cerr << "[plugin]   renomeie a classe do plugin." << std::endl;
            die();
         }
      }
   }

   // --- 10) a fabrica realmente constroi o que declarou? -------------------
   for (const std::string& n : nomes) {
      base::Object* const probe{desc->factory(n.c_str())};
      if (probe == nullptr) {
         std::cerr << "[plugin] '" << path << "': declara o nome '" << n
                   << "' mas a fabrica devolve nulo para ele." << std::endl;
         std::cerr << "[plugin]   descritor e factory fora de sincronia." << std::endl;
         die();
      }
      probe->unref();
   }

   // --- 11) registra -------------------------------------------------------
   for (const std::string& n : nomes) registry()[n] = desc->factory;
   if (desc->metas != nullptr) {
      for (const base::MetaObject* const* m = desc->metas; *m != nullptr; ++m) {
         metas().push_back(*m);
      }
   }
   loaded().push_back(Loaded{path, nome, desc});

   std::cout << "[plugin] carregado '" << nome << "' (ABI " << desc->abi
             << ", mixr " << (desc->mixr_pkg_version != nullptr ? desc->mixr_pkg_version : "?")
             << ", " << (desc->build_id != nullptr ? desc->build_id : "?") << ")" << std::endl;
   std::cout << "[plugin]   de " << path << std::endl;
   std::cout << "[plugin]   responde por:";
   for (const std::string& n : nomes) std::cout << " " << n;
   std::cout << std::endl;
}

void reportUnknownFactoryName(const std::string& name)
{
   std::cerr << "[plugin] nome de fabrica desconhecido no cenario: '" << name << "'."
             << std::endl;
   std::cerr << "[plugin]   nenhuma factory da aplicacao nem plugin carregado responde por ele."
             << std::endl;

   if (loaded().empty()) {
      std::cerr << "[plugin]   nenhum plugin foi carregado ate aqui." << std::endl;
   } else {
      std::cerr << "[plugin]   plugins carregados ate aqui: " << loaded().size() << std::endl;
      for (const Loaded& l : loaded()) {
         std::cerr << "[plugin]     " << l.pluginName << " (" << l.resolvedPath << ")"
                   << std::endl;
         std::cerr << "[plugin]       responde por:";
         for (const char* const* p = l.desc->factory_names; *p != nullptr; ++p) {
            std::cerr << " " << *p;
         }
         std::cerr << std::endl;
      }
   }

   std::cerr << "[plugin]   se a classe vem de um plugin, confira DUAS coisas:" << std::endl;
   std::cerr << "[plugin]     1) a grafia do nome;" << std::endl;
   std::cerr << "[plugin]     2) a POSICAO do bloco ( PluginLoader ) no .edl. O edl_parser"
             << std::endl;
   std::cerr << "[plugin]        constroi cada forma no fecha-parenteses DELA, em ordem de"
             << std::endl;
   std::cerr << "[plugin]        texto, entao o bloco tem de aparecer ANTES do primeiro uso."
             << std::endl;
   std::cerr << "[plugin]        Convencao deste repo: primeira entrada de 'components:' da"
             << std::endl;
   std::cerr << "[plugin]        Station, que ja vem antes de 'simulation:'." << std::endl;
   std::cerr << "[plugin]   (este diagnostico existe porque o edl_parser nao o da: a mensagem"
             << std::endl;
   std::cerr << "[plugin]    'undefined factory name' dele e codigo morto, e devolver nulo ao"
             << std::endl;
   std::cerr << "[plugin]    parser terminaria em SIGSEGV no edl_parser.y:179.)" << std::endl;
   std::exit(EXIT_FAILURE);
}

} // namespace xplugin
} // namespace mixr
