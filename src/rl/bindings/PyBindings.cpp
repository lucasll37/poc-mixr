#include "NativeSimulation.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "xrlbridge/ObservationFields.hpp"

#include <algorithm>
#include <cmath>

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
   // Os 28 campos numericos vem da MACRO -- nao ha lista escrita aqui. Um
   // campo novo em ObservationFields.hpp aparece neste dict sozinho.
#define XRLBRIDGE_F(nome) d[#nome] = obs.nome;
#define XRLBRIDGE_B(nome) d[#nome] = obs.nome;
   XRLBRIDGE_OBSERVATION_FIELDS
#undef XRLBRIDGE_F
#undef XRLBRIDGE_B
   // Os campos de TEXTO ficam fora da macro (nao entram num tensor), mas
   // continuam no dict, para info["raw_state"].
   d["contactName"] = obs.contactName;
   d["alertSender"] = obs.alertSender;
   d["alertContactName"] = obs.alertContactName;
   // Achado por auditoria: de quem e' esta observacao -- ver o comentario de
   // Observation::ownerName em libs/xrlbridge/RLBridge.hpp.
   d["ownerName"] = obs.ownerName;
   return d;
}

//------------------------------------------------------------------------------
// Wrapper fino em cima de rl::NativeSimulation: traduz dict <-> xrlbridge nas
// duas pontas, para NativeSimulation em si (e libs/xrlbridge) nao
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
      // ACHADO POR AUDITORIA (revisao completa do repositorio): nada nesta
      // fronteira validava o comando antes de escreve-lo em libs/xrlbridge
      // -- ao contrario do caminho ONNX irmao (OnnxPolicyAction ->
      // xrlbridge::unscaleCommand(), que recorta [-1,1] explicitamente
      // antes de escalar), uma chamada manual ou uma politica em TREINO
      // instavel que emitisse NaN/Inf chegava direto no Autopilot::
      // setCommandedHeadingD/setCommandedAltitudeFt/setCommandedVelocityKts
      // sem rede de seguranca nenhuma -- e nenhum DynamicsModel nativo
      // deste fork aplica os limites de manobra do Autopilot por conta
      // propria (ver docs/manual, aba Referencia/Autopilot: os slots de
      // limite chegam como parametro SEM NOME em RacModel/JSBSimModel,
      // descartados em tempo de compilacao). Nao-finito vira 0 (o mesmo
      // "nunca trava a simulacao, degrada" ja usado no resto do
      // repositorio -- ver libs/xmsg, "condicao sobre campo invalido nao
      // avalia"); heading e' periodico (fmod + wrap pra [0,360)); altitude/
      // velocidade sao recortadas pra uma faixa fisicamente plausivel --
      // generosa o bastante para nao brigar com heading_range/
      // altitude_range_m/speed_range_kts configuraveis de env.py (cujos
      // DEFAULTS sao mais estreitos: 0-360/0-8000 m/0-400 kt), so existe
      // pra barrar NaN/Inf/absurdo, nao pra reimpor a faixa do lado Python.
      const double headingFinite{std::isfinite(headingDeg) ? std::fmod(headingDeg, 360.0) : 0.0};
      const double headingWrapped{headingFinite < 0.0 ? headingFinite + 360.0 : headingFinite};
      const double altitude{std::isfinite(altitudeM) ? std::clamp(altitudeM, 0.0, 20000.0) : 0.0};
      const double speed{std::isfinite(speedKts) ? std::clamp(speedKts, 0.0, 800.0) : 0.0};

      mixr::xrlbridge::Command cmd;
      cmd.valid = true;   // so' este ponto publica uma acao de VERDADE -- ver xrlbridge::Command
      cmd.headingDeg = headingWrapped;
      cmd.altitudeM = altitude;
      cmd.speedKts = speed;

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

   // O CONTRATO DE DADOS, exposto ao Python. env.py e
   // src/rl/tools/export_onnx.py consomem estas duas em vez de repetir a
   // lista de campos -- ver libs/xrlbridge/ObservationFields.hpp.
   m.def("observation_field_names", &mixr::xrlbridge::observationFieldNames,
         "Os nomes dos campos numericos da observacao, na ORDEM CANONICA "
         "(a mesma que o .onnx espera na entrada).");
   m.def("observation_bool_fields", &mixr::xrlbridge::observationBoolFields,
         "Quais desses nomes sao booleanos.");

   // O preset "classic28" -- os 28 nomes historicos, na ordem historica.
   // flatten_obs.py/export_onnx.py usam isto como default EXPLICITO em vez
   // de observation_field_names() (que agora devolve 38): a lista canonica
   // cresceu para expor RWR/navegacao aos nos de arvore, mas o vetor que
   // alimenta uma rede ja treinada nao pode mudar de forma sem ninguem pedir.
   m.def("classic_schema_28", [] { return mixr::xrlbridge::classicSchema28().fieldNames; },
         "Os 28 nomes historicos, na ordem historica -- o schema 'classic28' do lado C++ "
         "(ver libs/xrlbridge/RLBridge.hpp). Default de todo consumidor Python que precisa "
         "de um vetor de tamanho fixo (flatten_obs.py, export_onnx.py).");
}
