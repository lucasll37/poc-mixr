#pragma once

// namespace ANINHADO em mixr::models::xaaa -- ver o comentario equivalente,
// mais detalhado, que existia no domain/ExampleThreshold.hpp deste mesmo
// scaffold: um cenario pode carregar mais de um plugin no mesmo processo, e
// dois tipos com o MESMO nome qualificado em dois .so's distintos colidem
// sob RTLD_LOCAL. Aninhar sob o namespace proprio deste modelo elimina a
// colisao de graca.
namespace mixr {
namespace models {
namespace xaaa {
namespace domain {

//------------------------------------------------------------------------------
// Dome / inDome -- o "domo" de alcance de uma antiaerea.
//
// Regra pura, sem MIXR: dentro do domo quando o alcance ao alvo cai no
// intervalo [minRangeM, maxRangeM], os dois limites INCLUSIVOS. Ao
// contrario de domain::LaunchPolicy (A-4), nao ha cone de marcacao aqui --
// uma bateria terrestre engaja em qualquer azimute dentro do alcance, entao
// "domo" e' so' os dois limites de distancia (as vezes chamado de raio
// minimo/maximo de engajamento, ou "dead zone" + "alcance maximo" em
// literatura de defesa antiaerea).
//
// Os proprios limites vem dos slots NATIVOS de mixr::models::SamVehicle
// (minLaunchRange/maxLaunchRange, ver xnative::AaaSite) -- esta struct so'
// os carrega ate' a regra, sem reimplementar o que o MIXR ja da de graca.
//------------------------------------------------------------------------------
struct Dome
{
   double minRangeM{0.0};
   double maxRangeM{40000.0};
};

bool inDome(const Dome& dome, double rangeM);

} // namespace domain
} // namespace xaaa
} // namespace models
} // namespace mixr
