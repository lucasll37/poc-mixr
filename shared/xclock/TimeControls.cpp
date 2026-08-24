#include "xclock/TimeControls.hpp"

#include "xclock/ClockStation.hpp"

#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace mixr {
namespace xclock {

namespace {

const double kLadder[]{0.1, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0};
const int kLadderSize{static_cast<int>(sizeof(kLadder) / sizeof(kLadder[0]))};
const int kRealTimeIndex{3};   // kLadder[3] == 1.0

// Degrau mais proximo do valor corrente -- usado na partida, para que a
// primeira tecla continue de onde o 'fastForwardRate' do .epp parou, em vez
// de dar um salto.
int nearestLadderIndex(const double scale)
{
   int best{kRealTimeIndex};
   double bestDist{-1.0};
   for (int i = 0; i < kLadderSize; i++) {
      const double dist{std::fabs(kLadder[i] - scale)};
      if (bestDist < 0.0 || dist < bestDist) {
         bestDist = dist;
         best = i;
      }
   }
   return best;
}

std::string formatScale(const double scale)
{
   std::ostringstream oss;
   if (scale < 1.0) {
      oss << std::fixed << std::setprecision(2) << scale << "x";
   } else {
      oss << std::fixed << std::setprecision(0) << scale << "x";
   }
   return oss.str();
}

}

TimeControls::TimeControls(ClockStation* const s) : station(s)
{
   if (station != nullptr) {
      ladderIndex = nearestLadderIndex(station->getTimeScale());
   } else {
      ladderIndex = kRealTimeIndex;
   }
}

bool TimeControls::isAvailable() const
{
   return keyboard.isActive();
}

bool TimeControls::poll()
{
   if (station == nullptr) return false;

   bool changed{};
   // Laco: um frame de background pode ter acumulado varias teclas.
   for (char key{keyboard.poll()}; key != '\0'; key = keyboard.poll()) {
      if (apply(key)) changed = true;
   }
   return changed;
}

bool TimeControls::apply(const char key)
{
   switch (key) {

      case '+':
      case '=':
         if (ladderIndex >= (kLadderSize - 1)) return false;
         ladderIndex++;
         return station->setTimeScale(kLadder[ladderIndex]);

      case '-':
      case '_':
         if (ladderIndex <= 0) return false;
         ladderIndex--;
         return station->setTimeScale(kLadder[ladderIndex]);

      case ' ':
      case 'p':
      case 'P':
         return station->togglePaused();

      case '1':
         ladderIndex = kRealTimeIndex;
         station->setPaused(false);
         return station->setTimeScale(kLadder[ladderIndex]);

      case 'h':
      case 'H':
      case '?':
         std::printf("%s", helpText().c_str());
         std::fflush(stdout);
         return false;

      default:
         return false;
   }
}

std::string TimeControls::describe() const
{
   if (station == nullptr) return "1x";

   const std::string scale{formatScale(station->getTimeScale())};
   if (station->isPaused()) return "PAUSADO (" + scale + ")";
   return scale;
}

std::string TimeControls::helpText()
{
   std::ostringstream oss;
   oss << "Controle de tempo:  [+] acelerar   [-] frear   [espaco] pausar/retomar"
       << "   [1] tempo real   [h] ajuda\n"
       << "                    escala: 0.10x 0.25x 0.50x 1x 2x 4x 8x 16x 32x 64x\n";
   return oss.str();
}

}
}
