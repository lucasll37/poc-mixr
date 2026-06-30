#include "poc/MyNavigation.hpp"

namespace poc {

IMPLEMENT_SUBCLASS(MyNavigation, "MyNavigation")
EMPTY_SLOTTABLE(MyNavigation)
EMPTY_DELETEDATA(MyNavigation)

MyNavigation::MyNavigation() { STANDARD_CONSTRUCTOR() }

void MyNavigation::copyData(const MyNavigation& org, const bool cc)
{
    BaseClass::copyData(org);
}

} // namespace poc
