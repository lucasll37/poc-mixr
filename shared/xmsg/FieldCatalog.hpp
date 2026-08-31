#pragma once

#include "xmsg/Snapshot.hpp"

#include <string>

namespace mixr {
namespace xmsg {

//------------------------------------------------------------------------------
// FieldCatalog -- a lista fechada de grandezas que se pode pedir no EDL.
//
// Uma tabela estatica { nome, grupo, indice, dimensao }. Resolver um nome
// custa uma busca linear, e isso acontece UMA vez, no reset(); depois disso o
// caminho quente le snap.v[indice].
//
// A DIMENSAO existe para o slot de limiar aceitar tanto numero cru quanto
// objeto de unidade do MIXR: 'by: 100' e 'by: ( Meters 100 )' tem de dizer a
// mesma coisa, e '( Seconds 100 )' num campo de distancia tem de ser recusado.
//
// Nome desconhecido e ERRO ALTO, nunca silencio: e o antidoto para a familia
// de armadilhas "nao constroi nada e nao reclama" que este repositorio ja
// catalogou quatro vezes (factory nao encadeada, dataLogTime zero, valor sem
// aspas virando Identifier, comentario acentuado no .epp).
//------------------------------------------------------------------------------

enum class Dim { None, Distance, Angle, Speed, Time };

struct FieldInfo
{
   const char* name{};
   Group group{};
   int index{};
   Dim dim{};
   const char* unit{};      // so para o registro de schema e para mensagem de erro
};

// Devolve nullptr se o nome nao existir.
const FieldInfo* findField(const std::string& name);

// Para a mensagem de erro: todos os nomes, separados por espaco.
std::string allFieldNames();

const FieldInfo* fieldByIndex(int index);

} // namespace xmsg
} // namespace mixr
