#include "domain/EclipseGeometry.hpp"

#include <cmath>
#include <cstring>

namespace mixr {
namespace models {
namespace xNavstar_3 {
namespace domain {

SunState sunState(const Vec3& r, const Vec3& s, const double earthRadiusM)
{
   // Componente axial (na direcao do sol) da posicao do satelite.
   const double h{r.x * s.x + r.y * s.y + r.z * s.z};
   if (h > 0.0) return SunState::SUNLIT;   // lado diurno do eixo de sombra

   // Componente perpendicular ao eixo sol-Terra -- distancia do satelite ao
   // proprio eixo do cilindro de sombra.
   const double px{r.x - h * s.x};
   const double py{r.y - h * s.y};
   const double pz{r.z - h * s.z};
   const double perpDist{std::sqrt(px * px + py * py + pz * pz)};

   return (perpDist >= earthRadiusM) ? SunState::SUNLIT : SunState::ECLIPSE;
}

const char* labelOf(const SunState s)
{
   return (s == SunState::ECLIPSE) ? "ECLIPSE" : "SUNLIT";
}

bool sunStateFromLabel(const char* const label, SunState& out)
{
   if (label == nullptr) return false;
   if (std::strcmp(label, "ECLIPSE") == 0) { out = SunState::ECLIPSE; return true; }
   if (std::strcmp(label, "SUNLIT") == 0)  { out = SunState::SUNLIT;  return true; }
   return false;
}

} // namespace domain
} // namespace xNavstar_3
} // namespace models
} // namespace mixr
