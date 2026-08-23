#include "events.hpp"

namespace events {

IMPLEMENT_SUBCLASS(RadarContactMessage, "RadarContactMessage")
EMPTY_SLOTTABLE(RadarContactMessage)

RadarContactMessage::RadarContactMessage(const int trackId_, const std::string& targetName_,
                                          const double rangeNM_, const double bearingDeg_,
                                          const std::string& sourcePlayerName_)
   : trackId(trackId_), targetName(targetName_), rangeNM(rangeNM_), bearingDeg(bearingDeg_),
     sourcePlayerName(sourcePlayerName_)
{
   STANDARD_CONSTRUCTOR()
}

void RadarContactMessage::copyData(const RadarContactMessage& org, const bool)
{
   BaseClass::copyData(org);
   trackId = org.trackId;
   targetName = org.targetName;
   rangeNM = org.rangeNM;
   bearingDeg = org.bearingDeg;
   sourcePlayerName = org.sourcePlayerName;
}

void RadarContactMessage::deleteData()
{
}

} // namespace events
