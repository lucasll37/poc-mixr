#include "poc/MySimulation.hpp"

namespace poc {
IMPLEMENT_SUBCLASS(MySimulation, "MySimulation")
EMPTY_SLOTTABLE(MySimulation)
EMPTY_DELETEDATA(MySimulation)
MySimulation::MySimulation() { STANDARD_CONSTRUCTOR() }
void MySimulation::copyData(const MySimulation& org, const bool cc) { BaseClass::copyData(org); }
} // namespace poc
