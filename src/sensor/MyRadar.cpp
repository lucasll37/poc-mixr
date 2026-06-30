#include "poc/MyRadar.hpp"

namespace poc {

IMPLEMENT_SUBCLASS(MyRadar, "MyRadar")
EMPTY_SLOTTABLE(MyRadar)
EMPTY_DELETEDATA(MyRadar)

MyRadar::MyRadar() { STANDARD_CONSTRUCTOR() }

void MyRadar::copyData(const MyRadar& org, const bool cc)
{
    BaseClass::copyData(org);
}

} // namespace poc
