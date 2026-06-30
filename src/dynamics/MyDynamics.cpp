#include "poc/MyDynamics.hpp"

namespace poc {

IMPLEMENT_SUBCLASS(MyDynamics, "MyDynamics")
EMPTY_SLOTTABLE(MyDynamics)
EMPTY_DELETEDATA(MyDynamics)

MyDynamics::MyDynamics() { STANDARD_CONSTRUCTOR() }

void MyDynamics::copyData(const MyDynamics& org, const bool cc)
{
    BaseClass::copyData(org);
}

} // namespace poc
