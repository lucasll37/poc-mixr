#ifndef __xrandom_DeterministicRng_H__
#define __xrandom_DeterministicRng_H__

#include <cstdint>
#include <random>
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
// Este arquivo tem DUAS camadas, e e' o unico lugar do repositorio onde
// qualquer uma das duas existe:
//
//   1. DERIVACAO de sementes a partir de identidade estavel -- fnv1a64() e
//      deriveSeed(), puras e sem estado.
//   2. O GERADOR em si -- a classe Rng, que embrulha o std::mt19937_64 e as
//      distribuicoes. Nenhuma outra classe do repositorio instancia um
//      gerador: e' esta que os consumidores guardam.
//
// A camada 2 morava DUPLICADA dentro de cada consumidor (domain::PatrolPlan e
// domain::AerobaticPlan tinham, cada um, o proprio std::mt19937_64 privado e
// a propria std::uniform_real_distribution), porque este header so fica
// visivel via dist/include e domain/ e' compilado por test_domain/test_tree
// SEM o SDK. O que destravou a unificacao: sendo header-only e sem NENHUMA
// dependencia (so <cstdint>/<random>/<string_view>), incluir este arquivo nao
// arrasta MIXR nenhum -- entao basta dar aos dois alvos de teste o CAMINHO DE
// INCLUDE do SDK (sdk_dep.partial_dependency(includes: true)), sem o link.
// A propriedade que importa ("test_tree NAO linka o MIXR", ver
// models/players/A-4/tests/meson.build) continua valendo e continua conferida
// por 'ldd'.
//
// A HIERARQUIA que este arquivo existe para viabilizar (ver o "porque" no
// CLAUDE.md, secao libs/xrandom):
//
//    patrolMasterSeed (1 slot, mesmo literal repetido em todo player)
//       -> deriveSeed(masterSeed, fnv1a64(player->getName()))   = instanceSeed
//       -> deriveSeed(instanceSeed, kAlgumSaltDePropoosito)     = seed final
//
// A sub-semente de cada player vem do NOME dele, nunca de uma posicao numa
// lista ou de ordem de descoberta -- a ordem de processamento entre players
// NAO e garantida neste framework (todo agente decide em paralelo,
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

//------------------------------------------------------------------------------
// Rng -- O gerador. Um por consumidor, semeado com o resultado FINAL da
// hierarquia acima.
//
// POR QUE UMA CLASSE, e nao so um alias para std::mt19937_64: o ponto nao e'
// esconder o mt19937_64 (ele nao tem nada de errado), e sim ter UM lugar onde
// "como este projeto sorteia" esta escrito. Duas coisas que estavam
// repetidas em cada consumidor e agora sao decididas aqui, uma vez:
//
//   * a distribuicao e' construida A CADA CHAMADA, nunca guardada. Uma
//     std::uniform_real_distribution guardada tem ESTADO proprio em algumas
//     implementacoes, e esse estado sobreviveria a um seed() -- ou seja,
//     resemear nao voltaria ao inicio da sequencia. Construir na hora torna
//     seed() um reset de verdade, que e' a propriedade da qual todo o
//     determinismo deste repositorio depende.
//   * a SEMENTE fica guardada aqui (seedValue()/reset()). Cada consumidor
//     mantinha a propria copia dela so para conseguir resemear no proprio
//     reset(); agora quem lembra e' o gerador.
//
// NAO ha gerador global, e nao ha construtor sem semente que "invente" uma
// (nada de std::random_device): um Rng recem-construido tem semente 0, que e'
// uma semente valida e reprodutivel como qualquer outra. Aleatoriedade que
// muda a cada execucao nao tem lugar neste projeto -- ver o comentario de
// hierarquia acima e tests/determinism/.
//------------------------------------------------------------------------------
class Rng
{
public:
   Rng() = default;
   explicit Rng(const std::uint64_t seed) noexcept { this->seed(seed); }

   // Reinicia a sequencia. Chamar com a MESMA semente devolve exatamente a
   // mesma sequencia de sorteios, sempre.
   void seed(const std::uint64_t s) noexcept
   {
      seed_ = s;
      engine_.seed(s);
   }

   // Volta ao inicio da sequencia SEM o chamador precisar carregar a semente
   // por fora. Antes disto, cada consumidor guardava uma copia da semente so
   // para poder resemear no proprio reset() -- exatamente a duplicacao que
   // esta classe existe para eliminar.
   void reset() noexcept { engine_.seed(seed_); }

   std::uint64_t seedValue() const noexcept { return seed_; }

   // Real uniforme em [lo, hi). Com lo >= hi devolve lo SEM consumir o
   // gerador -- e' o caso "faixa degenerada = valor fixo", e nao consumir
   // mantem a sequencia estavel para quem alterna entre faixa fixa e
   // sorteada (ver domain::AerobaticPlan::drawNextInterval()).
   double uniform(const double lo, const double hi)
   {
      if (!(hi > lo)) return lo;
      return std::uniform_real_distribution<double>(lo, hi)(engine_);
   }

   // Real uniforme em [-amplitude, +amplitude). amplitude <= 0 devolve 0 sem
   // consumir o gerador (mesma regra de uniform()) -- e' como um consumidor
   // desliga a variacao sem ramificar (ver domain::PatrolPlan).
   double symmetric(const double amplitude)
   {
      // O guard e' sobre a AMPLITUDE, nao delegado a uniform(): com
      // amplitude negativa, uniform(-a, a) receberia lo > hi e devolveria
      // 'lo' -- um numero positivo, o oposto de "variacao desligada".
      if (!(amplitude > 0.0)) return 0.0;
      return uniform(-amplitude, amplitude);
   }

private:
   std::uint64_t seed_{};
   std::mt19937_64 engine_{};
};

} // namespace xrandom
} // namespace mixr

#endif
