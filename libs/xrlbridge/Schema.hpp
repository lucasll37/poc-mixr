#ifndef __xrlbridge_Schema_H__
#define __xrlbridge_Schema_H__

#include "xrlbridge/FieldRegistry.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace mixr {
namespace xrlbridge {

//------------------------------------------------------------------------------
// Schema -- um NOME e uma lista ORDENADA de nomes de campo. Dado puro (nao
// sabe ler nada de nenhum State) -- e' o que permite declarar "quais campos,
// e em que ordem" numa porta de no de arvore (uma string no XML/.edl) sem
// nenhum tipo C++ do lado de quem escreve o cenario.
//
// Tres formas de uso, todas resolvidas por quem CHAMA bind() (ver os tres
// nos em models/players/air/A-4/src/bt/nodes/):
//   - um preset com nome ("classic28" -- ver xrlbridge::classicSchema28());
//   - "all" -- todo campo que o registro tiver, na ordem em que foi
//     registrado (ver FieldRegistry<State>::all());
//   - uma lista ad-hoc, separada por espaco ("northM eastM hasContact").
//------------------------------------------------------------------------------
struct Schema
{
   std::string name;
   std::vector<std::string> fieldNames;
};

// Lancada por bind() quando o schema pede um nome que o registro nao tem.
// Coleta TODOS os nomes desconhecidos de uma vez -- um schema errado
// tipicamente erra em mais de um nome (typo sistematico, ou uma lista colada
// de outro modelo) -- e lista os nomes VALIDOS do registro, para quem le o
// log nao ter que ir abrir o cabecalho do WorldView para descobrir o que
// existe.
class SchemaError : public std::runtime_error
{
public:
   SchemaError(const std::string& schemaName, const std::vector<std::string>& unknownNames,
               const std::vector<std::string>& validNames)
      : std::runtime_error(montarMensagem(schemaName, unknownNames, validNames))
   {
   }

private:
   static std::string montarMensagem(const std::string& schemaName,
                                      const std::vector<std::string>& unknownNames,
                                      const std::vector<std::string>& validNames)
   {
      std::string msg{"schema '" + schemaName + "' tem " +
                      std::to_string(unknownNames.size()) + " campo(s) desconhecido(s): "};
      for (std::size_t i = 0; i < unknownNames.size(); ++i) {
         if (i > 0) msg += ", ";
         msg += "'" + unknownNames[i] + "'";
      }
      msg += ". Campos validos (" + std::to_string(validNames.size()) + "): ";
      for (std::size_t i = 0; i < validNames.size(); ++i) {
         if (i > 0) msg += ", ";
         msg += validNames[i];
      }
      return msg;
   }
};

// Um Schema ja RESOLVIDO contra um FieldRegistry<State> -- 'resolved' tem o
// MESMO tamanho e ORDEM de schema.fieldNames, cada posicao apontando para o
// FieldDecl concreto. O ponteiro e' estavel: FieldRegistry<State>::all() e'
// preenchido uma vez, na construcao do registro (um Meyers singleton nos
// consumidores reais), e nunca cresce depois de um BoundSchema existir.
template <class State>
struct BoundSchema
{
   Schema schema;
   std::vector<const FieldDecl<State>*> resolved;
};

template <class State>
BoundSchema<State> bind(const Schema& schema, const FieldRegistry<State>& registry)
{
   BoundSchema<State> bound;
   bound.schema = schema;
   bound.resolved.reserve(schema.fieldNames.size());

   std::vector<std::string> desconhecidos;
   for (const auto& nome : schema.fieldNames) {
      const auto* const decl = registry.find(nome);
      if (decl == nullptr) {
         desconhecidos.push_back(nome);
         continue;
      }
      bound.resolved.push_back(decl);
   }

   if (!desconhecidos.empty()) {
      std::vector<std::string> validos;
      validos.reserve(registry.all().size());
      for (const auto& f : registry.all()) validos.push_back(f.name);
      throw SchemaError(schema.name, desconhecidos, validos);
   }

   return bound;
}

// Escreve os valores de 'state' na ordem do schema resolvido -- bool ja vira
// 0.0/1.0 dentro do proprio FieldDecl::read(), entao pack() nao precisa
// distinguir kind nenhum. 'out' tem de ter pelo menos bound.resolved.size()
// posicoes.
//
// Templada tambem no tipo de SAIDA (Out deduzido do ponteiro passado): os
// nos que rodam um .onnx (libs/xinfer::run()) querem float; o no que chama
// Python embutido (libs/xpyembed::decide()) quer double -- um pack() so'
// serve aos dois sem duplicar o laco.
template <class State, class Out>
void pack(const BoundSchema<State>& bound, const State& state, Out* const out)
{
   if (out == nullptr) return;
   for (std::size_t i = 0; i < bound.resolved.size(); ++i) {
      out[i] = static_cast<Out>(bound.resolved[i]->read(state));
   }
}

} // namespace xrlbridge
} // namespace mixr

#endif
