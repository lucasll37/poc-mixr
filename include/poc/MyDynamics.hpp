#pragma once
#include "mixr/models/dynamics/JSBSimModel.hpp"

namespace poc {

//------------------------------------------------------------------------------
// MyDynamics — wrapper de JSBSimModel (6DOF completo via JSBSim)
//
// Slots herdados relevantes:
//   model:   <String>  nome do modelo JSBSim (ex: "f16", "c172p")
//   rootDir: <String>  diretório raiz dos arquivos de aeronave JSBSim
//------------------------------------------------------------------------------
class MyDynamics final : public mixr::models::JSBSimModel
{
    DECLARE_SUBCLASS(MyDynamics, mixr::models::JSBSimModel)
public:
    MyDynamics();
    static const char* getFactoryName() { return "MyDynamics"; }
};

} // namespace poc
