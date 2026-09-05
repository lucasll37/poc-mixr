#ifndef __xrandom_DeterministicRng_H__
#define __xrandom_DeterministicRng_H__

#include <cstdint>
#include <string_view>

//------------------------------------------------------------------------------
// Derivacao de sementes reprodutiveis -- header-only, de proposito.
//
// O MIXR nao tem NENHUM gerador de numeros aleatorios nativo (avaliado antes
// de escrever isto: nada em base::, nada registrado em factory nenhuma). O
// unico achado parecido, em contexts/MIXR-PATTERN-CONTEXT.md, e um exemplo
// pedagogico de tutorial oficial (examples/tutorial02-04) que nem esta
// compilado no pacote Conan -- nao vem de graca linkando mixr_dep.
//
// Este arquivo NAO e um gerador em si -- e so a camada de DERIVACAO de
// sementes a partir de identidade estavel. O gerador de verdade
// (std::mt19937_64) mora dentro de cada consumidor (ex.: domain::PatrolPlan),
// privado, sem incluir este header: domain/ e bt/ deste modelo sao
// compilados por test_domain/test_tree SEM o SDK (ver models/player/A4/tests/
// meson.build, "sem MIXR, sem BehaviorTree.CPP" / "test_tree NAO linka o
// MIXR") -- fazer uma classe pura depender de um header publicado pelo SDK
// quebraria essa fronteira. Por isso o unico consumidor real deste header e
// BtBehavior::configurePlans(), que ja depende do SDK (ja inclui
// xlog/Log.hpp).
//
// A HIERARQUIA que este arquivo existe para viabilizar (ver o "porque" no
// CLAUDE.md, secao shared/xrandom):
//
//    patrolMasterSeed (1 slot, mesmo literal repetido em todo player)
//       -> deriveSeed(masterSeed, fnv1a64(player->getName()))   = instanceSeed
//       -> deriveSeed(instanceSeed, kAlgumSaltDePropoosito)     = seed final
//
// A sub-semente de cada player vem do NOME dele, nunca de uma posicao numa
// lista ou de ordem de descoberta -- a ordem de processamento entre players
// NAO e garantida neste framework (a poc multi-thread decide em paralelo,
// um por thread do pool de tempo critico), e qualquer esquema baseado em
// ordem quebraria o determinismo entre 1/2/4 threads. Um hash da identidade
// do proprio player elimina qualquer coordenacao: cada BtBehavior calcula a
// propria semente sozinho, sem saber nada sobre os outros.
//------------------------------------------------------------------------------

namespace mixr {
namespace xrandom {

//------------------------------------------------------------------------------
// FNV-1a de 64 bits -- constantes padrao do algoritmo.
//
// Mesma tecnica ja em producao em app/src/app/FleetPanel.cpp (fnv1a, 32
// bits, usado para mapear o rotulo bt= a uma cor fixa da paleta). Esta
// versao e de 64 bits para casar com std::uint64_t sem cast, e vive aqui
// por ser especificamente sobre DERIVAR SEMENTES de identidade estavel --
// nao sobre cor de paleta, que continua onde esta.
//------------------------------------------------------------------------------
constexpr std::uint64_t fnv1a64(const std::string_view s) noexcept
{
   std::uint64_t h{14695981039346656037ULL};
   for (const char c : s) {
      h ^= static_cast<unsigned char>(c);
      h *= 1099511628211ULL;
   }
   return h;
}

//------------------------------------------------------------------------------
// splitmix64 -- mistura (seed, salt) num std::uint64_t novo, sem estado.
//
// Usado nas duas derivacoes da hierarquia acima: master->instancia (salt =
// fnv1a64 do nome do player) e instancia->proposito (salt = uma constante
// fixa por consumidor, ex. "qual gerador dentro do player"). E "splitting"
// de proposito, nao so somar/xor os dois numeros: dois consumidores que
// derivem do MESMO instanceSeed com salts diferentes precisam de sequencias
// SEM correlacao entre si, nao so sementes numericamente diferentes.
//------------------------------------------------------------------------------
constexpr std::uint64_t deriveSeed(std::uint64_t seed, const std::uint64_t salt) noexcept
{
   seed += salt + 0x9E3779B97F4A7C15ULL;
   std::uint64_t z{seed};
   z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
   z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
   return z ^ (z >> 31);
}

} // namespace xrandom
} // namespace mixr

#endif
