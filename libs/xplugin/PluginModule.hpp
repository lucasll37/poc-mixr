#ifndef __xplugin_PluginModule_H__
#define __xplugin_PluginModule_H__

#include "mixr/base/Object.hpp"

#include <string>
#include <vector>

namespace mixr {
namespace base { class PairStream; class String; }
namespace xplugin {

//------------------------------------------------------------------------------
// Uma .so a carregar, declarada no .edl.
//
// Factory name: PluginModule
//
// Slots:
//    file      <String>      ! arquivo .so, resolvido contra os 'searchPaths:'
//                            !   do ( PluginLoader ) que o contem
//    provides  <PairStream>  ! ASSERCAO: os nomes de fabrica que esta .so deve
//                            !   entregar. Opcional, mas recomendado -- se a
//                            !   .so entregar outra coisa, o processo morre
//                            !   dizendo o que ela entrega. E o que pega uma
//                            !   .so velha esquecida num dos searchPaths.
//
// Esta classe so CARREGA DADO -- nao abre nada. Quem carrega e o
// PluginLoader::isValid(), depois que o parser terminou de setar os slots dos
// dois. Ver o cabecalho de PluginLoader.hpp para o porque dessa separacao.
//------------------------------------------------------------------------------
class PluginModule : public base::Object
{
   DECLARE_SUBCLASS(PluginModule, base::Object)

public:
   PluginModule();

   const std::string& file() const                   { return file_; }
   const std::vector<std::string>& provides() const  { return provides_; }

private:
   std::string file_;
   std::vector<std::string> provides_;

   bool setSlotFile(const base::String* const);
   bool setSlotProvides(const base::PairStream* const);
};

} // namespace xplugin
} // namespace mixr

#endif
