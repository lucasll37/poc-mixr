#pragma once

#include <string>
#include <vector>

//------------------------------------------------------------------------------
// Amostragem AO VIVO dos contadores de instancia do MIXR (MetaObject::count/
// mc/tc), para a aba "Memoria" do dashboard -- a mesma pergunta que
// app/MetaObjectReport.cpp ja responde, so que HOJE ele so imprime uma vez,
// no shutdown do modo '-deterministic'. Aqui a mesma fonte e amostrada a
// cada frame de background (10 Hz) e mantida numa janela deslizante, para
// dar o "limiar de objetos instanciados para investigacao de vazamento" ao
// vivo, sem esperar o processo terminar.
//
// AGNOSTICO A MODELO por construcao: a fonte e
// mixr::xplugin::pluginMetaObjects() (shared/xplugin/PluginRegistry.hpp),
// que devolve os MetaObject* que O PROPRIO PLUGIN carregado declarou no seu
// descritor (campo 'metas' de PluginDescV1) -- automaticamente cobre
// flight/missile/stub e qualquer modelo futuro, sem um nome de classe sequer
// escrito aqui.
//
// Sem tipo de FTXUI nem de MIXR alem de MetaObject -- pode ser testado
// isolado (ver tests/dashboard/test_meta_object_snapshot.cpp).
//------------------------------------------------------------------------------
namespace mixr { namespace base { class MetaObject; } }

namespace app {

const int kHistoryWindow{30};   // ~3s a 10 Hz -- ver sampleMetaObjects()

struct ClassStat
{
   std::string factoryName;
   bool fromPlugin{};      // false = classe do HOST (termometro do parser/EDL)
   int count{};             // instancias vivas AGORA
   int mc{};                 // pico historico (o proprio MetaObject ja acumula)
   long tc{};                // total ja criado (idem)

   std::vector<int> countHistory;   // janela deslizante de 'count', mais recente por ultimo

   // count nunca caiu dentro da janela E esta estritamente maior que no
   // inicio dela -- crescimento sustentado, o sinal de vazamento que se
   // pode afirmar sem depender de limiar magico por classe.
   bool suspectedLeak{};
};

// 'previous' e o resultado da amostra anterior (para herdar o historico);
// vazio na primeira chamada. Le mixr::xplugin::pluginMetaObjects() mais duas
// classes fixas do HOST (mixr::base::Pair, mixr::base::String -- as mesmas
// que MetaObjectReport ja usa como termometro do parser/EDL).
std::vector<ClassStat> sampleMetaObjects(const std::vector<ClassStat>& previous);

// A logica pura de janela deslizante + veredito, separada de ONDE os
// contadores vem -- e o que tests/dashboard/test_meta_object_snapshot.cpp
// exercita com sequencias sinteticas, sem MIXR nenhum.
ClassStat updateClassStat(const ClassStat& previous, const std::string& factoryName,
                          bool fromPlugin, int count, int mc, long tc);

} // namespace app
