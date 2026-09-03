#include "NativeSimulation.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace {

//------------------------------------------------------------------------------
// xrlbridge::Observation -> dict Python, campo a campo. Deliberadamente um
// dict solto (nao um py::class_ com atributos) -- e o formato mais simples
// de manter em sincronia com domain::WorldView (que muda com mais frequencia
// que este arquivo), e mixr_gym/env.py ja consome dicts em todo o resto do
// contrato de dados (ver rl/README.md).
//------------------------------------------------------------------------------
py::dict toDict(const mixr::xrlbridge::Observation& obs)
{
   py::dict d;
   d["valid"] = obs.valid;
   d["northM"] = obs.northM;
   d["eastM"] = obs.eastM;
   d["altitudeM"] = obs.altitudeM;
   d["headingDeg"] = obs.headingDeg;
   d["speedKts"] = obs.speedKts;
   d["rollDeg"] = obs.rollDeg;
   d["pitchDeg"] = obs.pitchDeg;
   d["fuelFraction"] = obs.fuelFraction;
   d["mach"] = obs.mach;
   d["gLoad"] = obs.gLoad;
   d["alphaDeg"] = obs.alphaDeg;
   d["terrainValid"] = obs.terrainValid;
   d["terrainElevM"] = obs.terrainElevM;
   d["altitudeAglM"] = obs.altitudeAglM;
   d["hasContact"] = obs.hasContact;
   d["contactName"] = obs.contactName;
   d["contactRangeM"] = obs.contactRangeM;
   d["contactRelBearingDeg"] = obs.contactRelBearingDeg;
   d["contactDeltaAltM"] = obs.contactDeltaAltM;
   d["contactNorthM"] = obs.contactNorthM;
   d["contactEastM"] = obs.contactEastM;
   d["contactAltitudeM"] = obs.contactAltitudeM;
   d["hasAlert"] = obs.hasAlert;
   d["alertSender"] = obs.alertSender;
   d["alertContactName"] = obs.alertContactName;
   d["alertNorthM"] = obs.alertNorthM;
   d["alertEastM"] = obs.alertEastM;
   d["alertAltitudeM"] = obs.alertAltitudeM;
   d["alertRangeM"] = obs.alertRangeM;
   d["weaponReady"] = obs.weaponReady;
   return d;
}

//------------------------------------------------------------------------------
// Wrapper fino em cima de rl::NativeSimulation: traduz dict <-> xrlbridge nas
// duas pontas, para NativeSimulation em si (e shared/xrlbridge) nao
// precisarem saber nada de Python/pybind11.
//------------------------------------------------------------------------------
class PyNativeSimulation
{
public:
   PyNativeSimulation(std::string scenarioPath, std::string playerName)
      : sim_(std::move(scenarioPath), std::move(playerName))
   {
   }

   py::dict reset() { return toDict(sim_.reset()); }

   py::tuple step(const double headingDeg, const double altitudeM, const double speedKts)
   {
      mixr::xrlbridge::Command cmd;
      cmd.headingDeg = headingDeg;
      cmd.altitudeM = altitudeM;
      cmd.speedKts = speedKts;

      const auto [obs, terminated] = sim_.step(cmd);
      return py::make_tuple(toDict(obs), terminated);
   }

   // No-op explicito -- ver o cabecalho de NativeSimulation.cpp: nunca
   // dlclose() do plugin do modelo, mesma regra do resto do repositorio.
   // Existe so para o gymnasium.Env ter um close() para chamar.
   void close() {}

private:
   rl::NativeSimulation sim_;
};

} // namespace

PYBIND11_MODULE(_native, m)
{
   m.doc() = "Bindings pybind11 sobre a simulacao MIXR/flight -- ver rl/README.md";

   py::class_<PyNativeSimulation>(m, "NativeSimulation")
      .def(py::init<std::string, std::string>(),
           py::arg("scenario_path"), py::arg("player_name") = "falcon1")
      .def("reset", &PyNativeSimulation::reset,
           "Reseta o cenario (RESET_EVENT) e devolve a observacao inicial (dict).")
      .def("step", &PyNativeSimulation::step,
           py::arg("heading_deg"), py::arg("altitude_m"), py::arg("speed_kts"),
           "Aplica o comando, avanca um frame de decisao, devolve (observacao: dict, terminated: bool).")
      .def("close", &PyNativeSimulation::close);
}
