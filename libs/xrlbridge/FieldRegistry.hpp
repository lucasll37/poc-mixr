#ifndef __xrlbridge_FieldRegistry_H__
#define __xrlbridge_FieldRegistry_H__

#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mixr {
namespace xrlbridge {

//------------------------------------------------------------------------------
// FieldRegistry<State> -- "quais campos numericos/booleanos este tipo de
// estado (domain::WorldView de um modelo, xrlbridge::Observation do core...)
// PODE expor, e como ler cada um pelo NOME".
//
// Existe para dar, em runtime, o que a X-macro de ObservationFields.hpp ja
// dava em tempo de compilacao -- SEM abrir mao da checagem de compilacao:
// quem monta um FieldRegistry<State> continua expandindo a MESMA macro, so
// que contra um lambda (`[](const State& s){ return s.nome; }`) em vez de
// escrever direto num vetor -- um `nome` que nao existe em State continua
// nao compilando (ver domain/WorldViewFieldRegistry.cpp, em
// models/players/air/A-4, para o uso real).
//
// O registro em si e' so' o CATALOGO ("quais campos existem, como ler cada
// um"). A ESCOLHA de quais entram num vetor, e em que ordem, e' o Schema
// (ver Schema.hpp) -- resolvido em runtime contra este registro.
//
// Header-only, template sobre State -- nao e' compilado em lugar nenhum,
// so' incluido. E' por isso que fica em libs/xrlbridge/ (publicado pelo SDK,
// ver o install_headers() do meson.build raiz) sem precisar entrar em
// RLBridge.cpp: um modelo pode instanciar FieldRegistry<SeuProprioState> sem
// que libs/xrlbridge precise saber que esse State existe.
//------------------------------------------------------------------------------

enum class FieldKind { kFloat, kBool };

template <class State>
struct FieldDecl
{
   std::string name;
   FieldKind kind{FieldKind::kFloat};
   std::function<double(const State&)> read;
};

template <class State>
class FieldRegistry
{
public:
   // Lanca std::logic_error em nome duplicado -- um erro de PROGRAMACAO (duas
   // expansoes de XRLBRIDGE_F/XRLBRIDGE_B com o mesmo nome na macro que monta
   // o registro), nunca um erro de configuracao de cenario. Acontece uma vez,
   // na inicializacao estatica do processo (ver o Meyers singleton em
   // WorldViewFieldRegistry.cpp) -- nunca por escolha de um .edl/schema, que
   // e' o caminho que usa SchemaError (ver Schema.hpp) em vez de excecao.
   void add(FieldDecl<State> decl)
   {
      if (find(decl.name) != nullptr) {
         throw std::logic_error("FieldRegistry: campo duplicado '" + decl.name + "'");
      }
      fields_.push_back(std::move(decl));
   }

   const FieldDecl<State>* find(const std::string& name) const
   {
      for (const auto& f : fields_) {
         if (f.name == name) return &f;
      }
      return nullptr;
   }

   const std::vector<FieldDecl<State>>& all() const { return fields_; }

private:
   std::vector<FieldDecl<State>> fields_;
};

} // namespace xrlbridge
} // namespace mixr

#endif
