#include "xpyembed/PyEmbed.hpp"

#include "xlog/Log.hpp"

#include <dlfcn.h>

#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace mixr {
namespace xpyembed {

namespace {

//------------------------------------------------------------------------------
// A tabela de ponteiros para a API C do CPython.
//
// Nenhum header do Python e incluido aqui -- de proposito. Incluir Python.h
// exigiria os headers de desenvolvimento em tempo de COMPILACAO e nao mudaria
// o problema de runtime (ver o "porque" no cabecalho do .hpp). PyObject* vira
// void*: esta lib nunca desreferencia um, so os passa adiante.
//------------------------------------------------------------------------------

using PyObj = void*;

struct Api
{
   int   (*IsInitialized)(){};
   void  (*InitializeEx)(int){};
   PyObj (*EvalSaveThread)(){};
   int   (*GILStateEnsure)(){};
   void  (*GILStateRelease)(int){};

   PyObj (*DictNew)(){};
   int   (*DictSetItemString)(PyObj, const char*, PyObj){};
   PyObj (*DictGetItemString)(PyObj, const char*){};
   PyObj (*EvalGetBuiltins)(){};

   PyObj (*RunString)(const char*, int, PyObj, PyObj){};
   PyObj (*ObjectCallObject)(PyObj, PyObj){};

   PyObj (*TupleNew)(long){};
   int   (*TupleSetItem)(PyObj, long, PyObj){};
   PyObj (*ListNew)(long){};
   int   (*ListSetItem)(PyObj, long, PyObj){};
   PyObj (*SequenceGetItem)(PyObj, long){};
   long  (*SequenceSize)(PyObj){};
   PyObj (*FloatFromDouble)(double){};
   double (*FloatAsDouble)(PyObj){};

   void  (*DecRef)(PyObj){};
   PyObj (*ErrOccurred)(){};
   void  (*ErrPrint)(){};
   void  (*ErrClear)(){};
};

// Py_file_input -- o modo de compilacao para um MODULO inteiro (varias
// instrucoes), contra Py_eval_input (uma expressao). O valor e 257 desde
// sempre e vem de Python.h, que deliberadamente nao incluimos.
constexpr int kFileInput{257};

std::mutex g_mutex;          // protege a carga; NAO cobre a chamada (o GIL cobre)
Api g_api;
void* g_lib{};
bool g_tentouCarregar{};
bool g_disponivel{};

template <typename T>
bool resolver(void* handle, T& destino, const char* nome)
{
   destino = reinterpret_cast<T>(::dlsym(handle, nome));
   if (destino == nullptr) {
      LOG(ERROR) << "[xpyembed] simbolo ausente na libpython: " << nome;
      return false;
   }
   return true;
}

// Carrega (ou nao) a libpython. Ver o cabecalho do .hpp para o "porque" de
// cada ramo. Chamada sob g_mutex.
bool garantirInterpretador()
{
   if (g_tentouCarregar) return g_disponivel;
   g_tentouCarregar = true;

   // 1. Ja ha interpretador no processo? E o caso do src/rl, em que a
   //    simulacao roda DENTRO do Python. Nao carregamos nada -- carregar
   //    daria um SEGUNDO runtime, porque o python3 do Ubuntu tem a libpython
   //    estatica dentro do executavel.
   if (::dlsym(RTLD_DEFAULT, "Py_IsInitialized") != nullptr) {
      g_lib = RTLD_DEFAULT;
   } else {
      // 2. RTLD_GLOBAL e obrigatorio, nao preferencia: sem ele as extensoes C
      //    (numpy, ctypes) nao resolvem os simbolos do proprio CPython quando
      //    o interpretador foi trazido por um plugin RTLD_LOCAL.
      for (const char* nome : {"libpython3.12.so.1.0", "libpython3.12.so",
                               "libpython3.so"}) {
         g_lib = ::dlopen(nome, RTLD_NOW | RTLD_GLOBAL);
         if (g_lib != nullptr) break;
      }
      if (g_lib == nullptr) {
         LOG(WARNING) << "[xpyembed] libpython nao encontrada -- os nos em Python"
                      << " ficam inertes (o resto da simulacao roda normalmente)";
         return false;
      }
   }

   const bool ok{
      resolver(g_lib, g_api.IsInitialized,     "Py_IsInitialized") &&
      resolver(g_lib, g_api.InitializeEx,      "Py_InitializeEx") &&
      resolver(g_lib, g_api.EvalSaveThread,    "PyEval_SaveThread") &&
      resolver(g_lib, g_api.GILStateEnsure,    "PyGILState_Ensure") &&
      resolver(g_lib, g_api.GILStateRelease,   "PyGILState_Release") &&
      resolver(g_lib, g_api.DictNew,           "PyDict_New") &&
      resolver(g_lib, g_api.DictSetItemString, "PyDict_SetItemString") &&
      resolver(g_lib, g_api.DictGetItemString, "PyDict_GetItemString") &&
      resolver(g_lib, g_api.EvalGetBuiltins,   "PyEval_GetBuiltins") &&
      resolver(g_lib, g_api.RunString,         "PyRun_String") &&
      resolver(g_lib, g_api.ObjectCallObject,  "PyObject_CallObject") &&
      resolver(g_lib, g_api.TupleNew,          "PyTuple_New") &&
      resolver(g_lib, g_api.TupleSetItem,      "PyTuple_SetItem") &&
      resolver(g_lib, g_api.ListNew,           "PyList_New") &&
      resolver(g_lib, g_api.ListSetItem,       "PyList_SetItem") &&
      resolver(g_lib, g_api.SequenceGetItem,   "PySequence_GetItem") &&
      resolver(g_lib, g_api.SequenceSize,      "PySequence_Size") &&
      resolver(g_lib, g_api.FloatFromDouble,   "PyFloat_FromDouble") &&
      resolver(g_lib, g_api.FloatAsDouble,     "PyFloat_AsDouble") &&
      resolver(g_lib, g_api.DecRef,            "Py_DecRef") &&
      resolver(g_lib, g_api.ErrOccurred,       "PyErr_Occurred") &&
      resolver(g_lib, g_api.ErrPrint,          "PyErr_Print") &&
      resolver(g_lib, g_api.ErrClear,          "PyErr_Clear")};
   if (!ok) return false;

   if (g_api.IsInitialized() == 0) {
      g_api.InitializeEx(0);      // 0 = sem handlers de sinal: o Ctrl+C e do host
      // Py_InitializeEx SEGURA o GIL. Sem soltar aqui, a primeira
      // PyGILState_Ensure() de outra thread trava para sempre.
      g_api.EvalSaveThread();
   }

   g_disponivel = true;
   return true;
}

struct Script
{
   std::string fonte;
   std::string caminho;
   // Um dicionario de globais POR PLAYER -- e isto que mantem o resultado
   // independente da ordem de aquisicao do GIL. Ver o cabecalho do .hpp.
   std::map<int, PyObj> globaisPorPlayer;
};

// deque, NAO vector: decide() toma uma REFERENCIA para dentro deste
// container (Script& script{g_scripts[id]}, mais abaixo) e a SOLTA antes de
// terminar -- mas so o GIL, nao g_mutex, protege esse trecho (ver o
// comentario de decide()). Um vector realoca no push_back() e invalidaria
// essa referencia se outra thread chamasse loadScript() para um caminho NOVO
// enquanto a primeira ainda estivesse dentro de decide() -- um caso real:
// quatro aeronaves em threads diferentes do pool, cada uma tickando o SEU
// PyDecideAction pela primeira vez no mesmo frame, cada uma carregando um
// script DIFERENTE (bt/nodes/PyDecideAction.cpp so serializa a carga POR
// INSTANCIA de no, nao entre instancias). deque garante que
// insercao no fim NUNCA invalida referencias a elementos ja existentes
// (so iteradores) -- exatamente a garantia que falta aqui.
std::deque<Script> g_scripts{Script{}};      // indice 0 vazio: id 0 e invalido
std::map<std::string, ScriptId> g_porCaminho;

// Prepara (uma vez) o dicionario de globais deste player e executa o script
// nele. Exige o GIL segurado pelo chamador.
PyObj globaisDoPlayer(Script& script, const int playerId)
{
   const auto ja = script.globaisPorPlayer.find(playerId);
   if (ja != script.globaisPorPlayer.end()) return ja->second;

   PyObj globais{g_api.DictNew()};
   if (globais == nullptr) return nullptr;
   g_api.DictSetItemString(globais, "__builtins__", g_api.EvalGetBuiltins());

   PyObj resultado{g_api.RunString(script.fonte.c_str(), kFileInput, globais, globais)};
   if (resultado == nullptr) {
      LOG(ERROR) << "[xpyembed] erro ao executar '" << script.caminho << "'";
      if (g_api.ErrOccurred() != nullptr) { g_api.ErrPrint(); g_api.ErrClear(); }
      g_api.DecRef(globais);
      script.globaisPorPlayer[playerId] = nullptr;    // nao tenta de novo
      return nullptr;
   }
   g_api.DecRef(resultado);

   if (g_api.DictGetItemString(globais, "decide") == nullptr) {
      LOG(ERROR) << "[xpyembed] '" << script.caminho << "' nao define decide(obs)";
      g_api.DecRef(globais);
      script.globaisPorPlayer[playerId] = nullptr;
      return nullptr;
   }

   script.globaisPorPlayer[playerId] = globais;
   return globais;
}

} // namespace

bool isAvailable()
{
   std::lock_guard<std::mutex> lock(g_mutex);
   return garantirInterpretador();
}

ScriptId loadScript(const std::string& path)
{
   if (path.empty()) return 0;

   std::lock_guard<std::mutex> lock(g_mutex);
   const auto ja = g_porCaminho.find(path);
   if (ja != g_porCaminho.end()) return ja->second;

   if (!garantirInterpretador()) return 0;

   std::ifstream arquivo{path};
   if (!arquivo) {
      LOG(ERROR) << "[xpyembed] script nao encontrado: " << path;
      return 0;
   }
   std::ostringstream buffer;
   buffer << arquivo.rdbuf();

   Script script;
   script.fonte = buffer.str();
   script.caminho = path;
   g_scripts.push_back(std::move(script));

   const ScriptId id{static_cast<ScriptId>(g_scripts.size() - 1)};
   g_porCaminho[path] = id;
   return id;
}

bool decide(const ScriptId id, const int playerId,
            const double* const obs, const int nObs,
            double* const cmd, const int nCmd)
{
   if (obs == nullptr || cmd == nullptr || nObs <= 0 || nCmd <= 0) return false;

   {
      std::lock_guard<std::mutex> lock(g_mutex);
      if (!g_disponivel || id <= 0 || static_cast<std::size_t>(id) >= g_scripts.size()) {
         return false;
      }
   }

   // A partir daqui quem serializa e o GIL, nao o g_mutex -- segurar os dois
   // seria redundante e criaria uma segunda ordem de aquisicao (deadlock em
   // potencial).
   const int estado{g_api.GILStateEnsure()};
   bool ok{false};

   Script& script{g_scripts[static_cast<std::size_t>(id)]};
   PyObj globais{globaisDoPlayer(script, playerId)};
   if (globais != nullptr) {
      PyObj fn{g_api.DictGetItemString(globais, "decide")};   // emprestada
      PyObj lista{g_api.ListNew(nObs)};
      if (fn != nullptr && lista != nullptr) {
         for (int i = 0; i < nObs; ++i) {
            // PyList_SetItem ROUBA a referencia -- nao ha DecRef do float aqui.
            g_api.ListSetItem(lista, i, g_api.FloatFromDouble(obs[i]));
         }
         PyObj args{g_api.TupleNew(1)};
         g_api.TupleSetItem(args, 0, lista);       // rouba 'lista' tambem

         PyObj retorno{g_api.ObjectCallObject(fn, args)};
         g_api.DecRef(args);

         if (retorno == nullptr) {
            LOG(ERROR) << "[xpyembed] decide() lancou em '" << script.caminho << "'";
            if (g_api.ErrOccurred() != nullptr) { g_api.ErrPrint(); g_api.ErrClear(); }
         } else {
            const long tamanho{g_api.SequenceSize(retorno)};
            if (tamanho < nCmd) {
               LOG(ERROR) << "[xpyembed] decide() devolveu " << tamanho
                          << " valores, esperado " << nCmd;
               if (g_api.ErrOccurred() != nullptr) g_api.ErrClear();
            } else {
               ok = true;
               for (int i = 0; i < nCmd; ++i) {
                  PyObj item{g_api.SequenceGetItem(retorno, i)};
                  cmd[i] = (item != nullptr) ? g_api.FloatAsDouble(item) : 0.0;
                  if (item != nullptr) g_api.DecRef(item);
               }
               if (g_api.ErrOccurred() != nullptr) { g_api.ErrClear(); ok = false; }
            }
            g_api.DecRef(retorno);
         }
      }
   }

   g_api.GILStateRelease(estado);
   return ok;
}

} // namespace xpyembed
} // namespace mixr
