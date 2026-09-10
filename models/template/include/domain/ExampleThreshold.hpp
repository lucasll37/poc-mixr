#pragma once

// namespace ANINHADO em mixr::models::xtemplate -- nao um "domain" solto no
// escopo global (que e o que models/players/A-4 faz, por ter chegado primeiro e ja
// estar documentado em dezenas de lugares daquele jeito). Isto e deliberado,
// nao capricho de estilo: um cenario pode carregar MAIS DE UM plugin no
// mesmo processo (este modelo ao lado de outro, cada um com sua propria
// pasta "domain/"), e dois tipos com o MESMO nome qualificado
// ("domain::Foo") em dois .so's distintos tem o MESMO simbolo mangled -- a
// comparacao de type_info deste toolchain degrada para strcmp entre objetos
// RTLD_LOCAL (ver o comentario equivalente, mais detalhado, em
// libs/xplugin/PluginRegistry.cpp). Aninhar sob o namespace proprio deste
// modelo elimina a colisao de graca -- troque "xtemplate" pelo nome do SEU
// modelo ao copiar este diretorio (ver docs/PRIMEIROS-PASSOS.md).
namespace mixr {
namespace models {
namespace xtemplate {
namespace domain {

//------------------------------------------------------------------------------
// ExampleThreshold -- um Schmitt trigger (histerese de dois limiares).
//
// Regra pura: sem MIXR, sem BehaviorTree.CPP, sem estado global -- testavel
// isolada (ver tests/domain/test_ExampleThreshold.cpp) sem levantar Station
// nenhuma. E o tipo de regra que praticamente todo modelo de decisao
// precisa mais cedo ou mais tarde: "engajar quando o valor sobe, mas so
// desengajar quando ele cai BEM abaixo" evita alternar a cada amostra
// quando o sinal de entrada oscila perto de UM limiar so. Ver
// domain::ThreatPolicy/PatrolPlan em models/players/A-4 para versoes de
// producao da mesma ideia, e libs/xmsg/rules/Schmitt.hpp para outra.
//
// onValue engaja (a partir de onValue, inclusive); offValue desengaja
// (abaixo de offValue, exclusive). Espera-se offValue <= onValue -- com os
// dois iguais, degenera num limiar simples, sem histerese nenhuma (ainda
// valido, so sem o beneficio).
//------------------------------------------------------------------------------
struct ExampleThreshold
{
   double onValue{1.0};
   double offValue{0.0};

   // engaged: o estado ATUAL, antes desta amostra. value: a leitura nova.
   // Devolve o estado seguinte -- quem chama e responsavel por guardar o
   // retorno e passa-lo de volta na proxima chamada (ver
   // ubf::ExampleBehavior::genAction()).
   bool next(double value, bool engaged) const;
};

} // namespace domain
} // namespace xtemplate
} // namespace models
} // namespace mixr
