#include "su/result_reader.hpp"
#include "su/version.hpp"
#include "su/workflow.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

std::string status_text(su::ResultStatus status) {
  return su::to_string(status);
}

std::string task_status_text(su::TaskStatus status) {
  return su::to_string(status);
}

std::string result_format_text(su::ResultFormat format) {
  switch (format) {
    case su::ResultFormat::kUnknown:
      return "unknown";
    case su::ResultFormat::kPsfAscii:
      return "psf_ascii";
    case su::ResultFormat::kBinPsf:
      return "bin_psf";
    case su::ResultFormat::kPsfxl:
      return "psfxl";
    case su::ResultFormat::kNspiceWrdata:
      return "nspice_wrdata";
  }
  return "unknown";
}

su::SimulatorKind parse_simulator_kind(const std::string& value) {
  if (value == "spectre") {
    return su::SimulatorKind::kSpectre;
  }
  if (value == "ngspice") {
    return su::SimulatorKind::kNgspice;
  }
  throw std::invalid_argument("unsupported simulator: " + value);
}

su::ResultFormat parse_result_format(const std::string& value) {
  if (value == "unknown") {
    return su::ResultFormat::kUnknown;
  }
  if (value == "psf_ascii") {
    return su::ResultFormat::kPsfAscii;
  }
  if (value == "bin_psf") {
    return su::ResultFormat::kBinPsf;
  }
  if (value == "psfxl") {
    return su::ResultFormat::kPsfxl;
  }
  if (value == "nspice_wrdata") {
    return su::ResultFormat::kNspiceWrdata;
  }
  throw std::invalid_argument("unsupported result_format: " + value);
}

su::NgspiceBuiltinTask parse_ngspice_task(const std::string& value) {
  if (value == "rc_ac") {
    return su::NgspiceBuiltinTask::kRcAc;
  }
  if (value == "rc_tran") {
    return su::NgspiceBuiltinTask::kRcTran;
  }
  if (value == "resistor_divider_dc") {
    return su::NgspiceBuiltinTask::kResistorDividerDc;
  }
  throw std::invalid_argument("unsupported ngspice_task: " + value);
}

template <typename T>
void fill_status(const su::ReadResult<T>& source, su::ResultStatus& status, std::string& message) {
  status = source.status;
  message = source.error_message;
}

struct PyScalarResult {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  std::string signal;
  double value = 0.0;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }
};

struct PyDcSweep {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  std::string sweep_name;
  std::string signal;
  std::vector<double> sweep_values;
  std::vector<double> values;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }

  std::size_t size() const noexcept {
    return sweep_values.size();
  }

  bool shape_consistent() const noexcept {
    return sweep_values.size() == values.size();
  }
};

struct PyAcResponse {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  std::string signal;
  std::vector<double> frequency_hz;
  std::vector<double> real;
  std::vector<double> imag;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }

  std::size_t size() const noexcept {
    return frequency_hz.size();
  }

  bool shape_consistent() const noexcept {
    return frequency_hz.size() == real.size() && real.size() == imag.size();
  }
};

struct PyAcDerivedView {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
  std::vector<double> phase_deg;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }

  std::size_t size() const noexcept {
    return frequency_hz.size();
  }

  bool shape_consistent() const noexcept {
    return frequency_hz.size() == magnitude_db.size() && magnitude_db.size() == phase_deg.size();
  }
};

struct PyTranWaveform {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  std::string signal;
  std::vector<double> time_s;
  std::vector<double> value;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }

  std::size_t size() const noexcept {
    return time_s.size();
  }

  bool shape_consistent() const noexcept {
    return time_s.size() == value.size();
  }
};

struct PyUgbwPhaseMarginResult {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  double unity_gain_bandwidth_hz = 0.0;
  double phase_margin_deg = 0.0;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }
};

struct PySettlingTimeResult {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  double settling_time_s = 0.0;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
  }

  std::string status_text() const {
    return ::status_text(status);
  }
};

PyScalarResult to_python(su::ReadResult<su::ScalarResult> result) {
  PyScalarResult output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.signal = std::move(result.value.signal);
    output.value = result.value.value;
  }
  return output;
}

PyDcSweep to_python(su::ReadResult<su::DcSweep> result) {
  PyDcSweep output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.sweep_name = std::move(result.value.sweep_name);
    output.signal = std::move(result.value.signal);
    output.sweep_values = std::move(result.value.sweep_values);
    output.values = std::move(result.value.values);
  }
  return output;
}

PyAcResponse to_python(su::ReadResult<su::AcResponse> result) {
  PyAcResponse output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.signal = std::move(result.value.signal);
    output.frequency_hz = std::move(result.value.frequency_hz);
    output.real = std::move(result.value.real);
    output.imag = std::move(result.value.imag);
  }
  return output;
}

PyAcDerivedView to_python(su::ReadResult<su::AcDerivedView> result) {
  PyAcDerivedView output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.frequency_hz = std::move(result.value.frequency_hz);
    output.magnitude_db = std::move(result.value.magnitude_db);
    output.phase_deg = std::move(result.value.phase_deg);
  }
  return output;
}

PyTranWaveform to_python(su::ReadResult<su::TranWaveform> result) {
  PyTranWaveform output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.signal = std::move(result.value.signal);
    output.time_s = std::move(result.value.time_s);
    output.value = std::move(result.value.value);
  }
  return output;
}

PyUgbwPhaseMarginResult to_python(su::ReadResult<su::UgbwPhaseMarginResult> result) {
  PyUgbwPhaseMarginResult output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.unity_gain_bandwidth_hz = result.value.unity_gain_bandwidth_hz;
    output.phase_margin_deg = result.value.phase_margin_deg;
  }
  return output;
}

PySettlingTimeResult to_python(su::ReadResult<su::SettlingTimeResult> result) {
  PySettlingTimeResult output;
  fill_status(result, output.status, output.message);
  if (result.ok()) {
    output.settling_time_s = result.value.settling_time_s;
  }
  return output;
}

struct PySimulationResult {
  explicit PySimulationResult(su::SimulationResult result) : result(std::move(result)) {}

  bool ok() const noexcept {
    return result.ok();
  }

  su::TaskStatus status() const noexcept {
    return result.status();
  }

  std::string status_text() const {
    return result.status_text();
  }

  std::string message() const {
    return result.message();
  }

  std::string detail() const {
    return result.detail();
  }

  std::string work_dir() const {
    return result.work_dir();
  }

  std::string result_format() const {
    return result_format_text(result.result_format());
  }

  PyScalarResult read_dc(const std::string& signal_name) const {
    return to_python(result.read_dc(signal_name));
  }

  PyDcSweep read_dc_sweep(const std::string& sweep_name, const std::string& signal_name,
                          const std::string& filename) const {
    return to_python(result.read_dc_sweep(sweep_name, signal_name, filename));
  }

  PyAcResponse read_ac(const std::string& signal_name, const std::string& filename) const {
    return to_python(result.read_ac(signal_name, filename));
  }

  PyTranWaveform read_tran(const std::string& signal_name, const std::string& filename) const {
    return to_python(result.read_tran(signal_name, filename));
  }

  su::SimulationResult result;
};

struct PySimulation {
  PySimulation(std::string netlist_path, const std::string& simulator, int workers,
               std::string work_dir_base, std::string workspace_namespace, int timeout_seconds,
               int restart_attempts, const std::string& result_format,
               const std::string& ngspice_task) {
    su::SimulationOptions options;
    options.simulator = parse_simulator_kind(simulator);
    options.netlist_path = std::move(netlist_path);
    options.workers = workers;
    options.work_dir_base = std::move(work_dir_base);
    options.workspace_namespace = std::move(workspace_namespace);
    options.timeout_seconds = timeout_seconds;
    options.restart_attempts = restart_attempts;
    options.result_format = parse_result_format(result_format);
    options.ngspice_task = parse_ngspice_task(ngspice_task);
    simulation.reset(new su::Simulation(std::move(options)));
  }

  void add_parameter(const std::string& name, py::object default_value) {
    if (default_value.is_none()) {
      simulation->add_parameter(name);
      return;
    }
    simulation->add_parameter(name, default_value.cast<double>());
  }

  std::vector<PySimulationResult> run(const std::vector<su::SimulationCase>& cases) {
    std::vector<su::SimulationResult> cpp_results;
    {
      py::gil_scoped_release release;
      cpp_results = simulation->run(cases);
    }

    std::vector<PySimulationResult> results;
    results.reserve(cpp_results.size());
    for (auto& result : cpp_results) {
      results.emplace_back(std::move(result));
    }
    return results;
  }

  void cleanup() noexcept {
    simulation->cleanup();
  }

  std::string workspace_root() const {
    return simulation->workspace_root();
  }

  PySimulation& enter() noexcept {
    return *this;
  }

  bool exit(const py::object&, const py::object&, const py::object&) noexcept {
    cleanup();
    return false;
  }

  std::unique_ptr<su::Simulation> simulation;
};

su::AcResponse to_cpp(const PyAcResponse& response) {
  su::AcResponse output;
  output.signal = response.signal;
  output.frequency_hz = response.frequency_hz;
  output.real = response.real;
  output.imag = response.imag;
  return output;
}

su::AcDerivedView to_cpp(const PyAcDerivedView& response) {
  su::AcDerivedView output;
  output.frequency_hz = response.frequency_hz;
  output.magnitude_db = response.magnitude_db;
  output.phase_deg = response.phase_deg;
  return output;
}

su::TranWaveform to_cpp(const PyTranWaveform& waveform) {
  su::TranWaveform output;
  output.signal = waveform.signal;
  output.time_s = waveform.time_s;
  output.value = waveform.value;
  return output;
}

}  // namespace

PYBIND11_MODULE(spiceunion, module) {
  module.doc() = "pybind11 bindings for SPICEUnion workflow and result readers";

  py::enum_<su::ResultStatus>(module, "ResultStatus")
      .value("OK", su::ResultStatus::kOk)
      .value("DIRECTORY_NOT_FOUND", su::ResultStatus::kDirectoryNotFound)
      .value("FILE_NOT_FOUND", su::ResultStatus::kFileNotFound)
      .value("SIGNAL_NOT_FOUND", su::ResultStatus::kSignalNotFound)
      .value("UNSUPPORTED_FORMAT", su::ResultStatus::kUnsupportedFormat)
      .value("PARSE_ERROR", su::ResultStatus::kParseError)
      .value("INVALID_INPUT", su::ResultStatus::kInvalidInput);

  py::enum_<su::TaskStatus>(module, "TaskStatus")
      .value("SUCCESS", su::TaskStatus::kSuccess)
      .value("SIMULATION_FAILED", su::TaskStatus::kSimulationFailed)
      .value("STARTUP_FAILED", su::TaskStatus::kStartupFailed)
      .value("TIMEOUT", su::TaskStatus::kTimeout)
      .value("TRANSPORT_FAILURE", su::TaskStatus::kTransportFailure)
      .value("EXCEPTION", su::TaskStatus::kException);

  py::enum_<su::SimulatorKind>(module, "SimulatorKind")
      .value("SPECTRE", su::SimulatorKind::kSpectre)
      .value("NGSPICE", su::SimulatorKind::kNgspice);

  py::enum_<su::ResultFormat>(module, "ResultFormat")
      .value("UNKNOWN", su::ResultFormat::kUnknown)
      .value("PSF_ASCII", su::ResultFormat::kPsfAscii)
      .value("BIN_PSF", su::ResultFormat::kBinPsf)
      .value("PSFXL", su::ResultFormat::kPsfxl)
      .value("NSPICE_WRDATA", su::ResultFormat::kNspiceWrdata);

  py::enum_<su::NgspiceBuiltinTask>(module, "NgspiceBuiltinTask")
      .value("RC_AC", su::NgspiceBuiltinTask::kRcAc)
      .value("RC_TRAN", su::NgspiceBuiltinTask::kRcTran)
      .value("RESISTOR_DIVIDER_DC", su::NgspiceBuiltinTask::kResistorDividerDc);

  py::class_<PyScalarResult>(module, "ScalarResult")
      .def(py::init<>())
      .def("ok", &PyScalarResult::ok)
      .def("status_text", &PyScalarResult::status_text)
      .def_readwrite("status", &PyScalarResult::status)
      .def_readwrite("message", &PyScalarResult::message)
      .def_readwrite("signal", &PyScalarResult::signal)
      .def_readwrite("value", &PyScalarResult::value);

  py::class_<PyDcSweep>(module, "DcSweep")
      .def(py::init<>())
      .def("ok", &PyDcSweep::ok)
      .def("status_text", &PyDcSweep::status_text)
      .def("shape_consistent", &PyDcSweep::shape_consistent)
      .def("__len__", &PyDcSweep::size)
      .def_readwrite("status", &PyDcSweep::status)
      .def_readwrite("message", &PyDcSweep::message)
      .def_readwrite("sweep_name", &PyDcSweep::sweep_name)
      .def_readwrite("signal", &PyDcSweep::signal)
      .def_readwrite("sweep_values", &PyDcSweep::sweep_values)
      .def_readwrite("values", &PyDcSweep::values);

  py::class_<PyAcResponse>(module, "AcResponse")
      .def(py::init<>())
      .def("ok", &PyAcResponse::ok)
      .def("status_text", &PyAcResponse::status_text)
      .def("shape_consistent", &PyAcResponse::shape_consistent)
      .def("__len__", &PyAcResponse::size)
      .def_readwrite("status", &PyAcResponse::status)
      .def_readwrite("message", &PyAcResponse::message)
      .def_readwrite("signal", &PyAcResponse::signal)
      .def_readwrite("frequency_hz", &PyAcResponse::frequency_hz)
      .def_readwrite("real", &PyAcResponse::real)
      .def_readwrite("imag", &PyAcResponse::imag);

  py::class_<PyAcDerivedView>(module, "AcDerivedView")
      .def(py::init<>())
      .def("ok", &PyAcDerivedView::ok)
      .def("status_text", &PyAcDerivedView::status_text)
      .def("shape_consistent", &PyAcDerivedView::shape_consistent)
      .def("__len__", &PyAcDerivedView::size)
      .def_readwrite("status", &PyAcDerivedView::status)
      .def_readwrite("message", &PyAcDerivedView::message)
      .def_readwrite("frequency_hz", &PyAcDerivedView::frequency_hz)
      .def_readwrite("magnitude_db", &PyAcDerivedView::magnitude_db)
      .def_readwrite("phase_deg", &PyAcDerivedView::phase_deg);

  py::class_<PyTranWaveform>(module, "TranWaveform")
      .def(py::init<>())
      .def("ok", &PyTranWaveform::ok)
      .def("status_text", &PyTranWaveform::status_text)
      .def("shape_consistent", &PyTranWaveform::shape_consistent)
      .def("__len__", &PyTranWaveform::size)
      .def_readwrite("status", &PyTranWaveform::status)
      .def_readwrite("message", &PyTranWaveform::message)
      .def_readwrite("signal", &PyTranWaveform::signal)
      .def_readwrite("time_s", &PyTranWaveform::time_s)
      .def_readwrite("value", &PyTranWaveform::value);

  py::class_<PyUgbwPhaseMarginResult>(module, "UgbwPhaseMarginResult")
      .def(py::init<>())
      .def("ok", &PyUgbwPhaseMarginResult::ok)
      .def("status_text", &PyUgbwPhaseMarginResult::status_text)
      .def_readwrite("status", &PyUgbwPhaseMarginResult::status)
      .def_readwrite("message", &PyUgbwPhaseMarginResult::message)
      .def_readwrite("unity_gain_bandwidth_hz", &PyUgbwPhaseMarginResult::unity_gain_bandwidth_hz)
      .def_readwrite("phase_margin_deg", &PyUgbwPhaseMarginResult::phase_margin_deg);

  py::class_<PySettlingTimeResult>(module, "SettlingTimeResult")
      .def(py::init<>())
      .def("ok", &PySettlingTimeResult::ok)
      .def("status_text", &PySettlingTimeResult::status_text)
      .def_readwrite("status", &PySettlingTimeResult::status)
      .def_readwrite("message", &PySettlingTimeResult::message)
      .def_readwrite("settling_time_s", &PySettlingTimeResult::settling_time_s);

  py::class_<PySimulationResult>(module, "SimulationResult")
      .def("ok", &PySimulationResult::ok)
      .def("status_text", &PySimulationResult::status_text)
      .def_property_readonly("status", &PySimulationResult::status)
      .def_property_readonly("message", &PySimulationResult::message)
      .def_property_readonly("detail", &PySimulationResult::detail)
      .def_property_readonly("work_dir", &PySimulationResult::work_dir)
      .def_property_readonly("result_format", &PySimulationResult::result_format)
      .def("read_dc", &PySimulationResult::read_dc, py::arg("signal_name"))
      .def("read_dc_sweep", &PySimulationResult::read_dc_sweep, py::arg("sweep_name"),
           py::arg("signal_name"), py::arg("filename") = "dc.dc")
      .def("read_ac", &PySimulationResult::read_ac, py::arg("signal_name"),
           py::arg("filename") = "ac.ac")
      .def("read_tran", &PySimulationResult::read_tran, py::arg("signal_name"),
           py::arg("filename") = "tran.tran");

  py::class_<PySimulation>(module, "Simulation")
      .def(py::init<std::string, const std::string&, int, std::string, std::string, int, int,
                    const std::string&, const std::string&>(),
           py::arg("netlist_path"), py::arg("simulator") = "spectre", py::arg("workers") = 1,
           py::arg("work_dir_base") = "local/runtime/simulations",
           py::arg("workspace_namespace") = "", py::arg("timeout_seconds") = 60,
           py::arg("restart_attempts") = 1, py::arg("result_format") = "unknown",
           py::arg("ngspice_task") = "rc_ac")
      .def("add_parameter", &PySimulation::add_parameter, py::arg("name"),
           py::arg("default_value") = py::none())
      .def("run", &PySimulation::run, py::arg("cases"))
      .def("cleanup", &PySimulation::cleanup)
      .def_property_readonly("workspace_root", &PySimulation::workspace_root)
      .def("__enter__", &PySimulation::enter, py::return_value_policy::reference_internal)
      .def("__exit__", &PySimulation::exit, py::arg("exc_type"), py::arg("exc"),
           py::arg("traceback"));

  module.def("version", &su::version);
  module.def("status_text", &status_text, py::arg("status"));
  module.def("task_status_text", &task_status_text, py::arg("status"));

  module.def("libpsf_reader_enabled", []() {
#if SPICEUNION_ENABLE_LIBPSF_READER_FOR_PYTHON
    return true;
#else
    return false;
#endif
  });

  module.def(
      "read_dc_value",
      [](const std::string& result_dir, const std::string& signal_name) {
        return to_python(su::read_dc_value(result_dir, signal_name));
      },
      py::arg("result_dir"), py::arg("signal_name"));

  module.def(
      "read_dc_sweep",
      [](const std::string& result_dir, const std::string& sweep_name,
         const std::string& signal_name, const std::string& filename) {
        return to_python(su::read_dc_sweep(result_dir, sweep_name, signal_name, filename));
      },
      py::arg("result_dir"), py::arg("sweep_name"), py::arg("signal_name"),
      py::arg("filename") = "dc.dc");

  module.def(
      "read_ac_response",
      [](const std::string& result_dir, const std::string& signal_name,
         const std::string& filename) {
        return to_python(su::read_ac_response(result_dir, signal_name, filename));
      },
      py::arg("result_dir"), py::arg("signal_name"), py::arg("filename") = "ac.ac");

  module.def(
      "read_tran_waveform",
      [](const std::string& result_dir, const std::string& signal_name,
         const std::string& filename) {
        return to_python(su::read_tran_waveform(result_dir, signal_name, filename));
      },
      py::arg("result_dir"), py::arg("signal_name"), py::arg("filename") = "tran.tran");

  module.def(
      "derive_ac_view",
      [](const PyAcResponse& response) { return to_python(su::derive_ac_view(to_cpp(response))); },
      py::arg("response"));

  module.def(
      "calculate_ugbw_and_phase_margin",
      [](const PyAcDerivedView& response) {
        return to_python(su::calculate_ugbw_and_phase_margin(to_cpp(response)));
      },
      py::arg("response"));

  module.def(
      "calculate_settling_time",
      [](const PyTranWaveform& waveform, double target_value, double error_band) {
        return to_python(su::calculate_settling_time(to_cpp(waveform), target_value, error_band));
      },
      py::arg("waveform"), py::arg("target_value"), py::arg("error_band") = 0.01);
}
