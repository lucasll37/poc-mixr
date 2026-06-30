#include "poc/MyStation.hpp"

namespace poc {
IMPLEMENT_SUBCLASS(MyStation, "MyStation")
EMPTY_SLOTTABLE(MyStation)
EMPTY_DELETEDATA(MyStation)
MyStation::MyStation() { STANDARD_CONSTRUCTOR() }
void MyStation::copyData(const MyStation& org, const bool cc) { BaseClass::copyData(org); }
} // namespace poc
