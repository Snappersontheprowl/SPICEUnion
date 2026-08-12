#include "su/result_reader.hpp"
#include "su/version.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace {

template <typename T>
void fill_status(const su::ReadResult<T>& source, su::ResultStatus& status,
                 std::string& message) {
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

  std::size_t size() const noexcept {
    return sweep_values.size();
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

  std::size_t size() const noexcept {
    return frequency_hz.size();
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

  std::size_t size() const noexcept {
    return frequency_hz.size();
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

  std::size_t size() const noexcept {
    return time_s.size();
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
};

struct PySettlingTimeResult {
  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  std::string message;
  double settling_time_s = 0.0;

  bool ok() const noexcept {
    return status == su::ResultStatus::kOk;
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
  module.doc() = "Minimal pybind11 bindings for SPICEUnion result readers";

  py::enum_<su::ResultStatus>(module, "ResultStatus")
      .value("OK", su::ResultStatus::kOk)
      .value("DIRECTORY_NOT_FOUND", su::ResultStatus::kDirectoryNotFound)
      .value("FILE_NOT_FOUND", su::ResultStatus::kFileNotFound)
      .value("SIGNAL_NOT_FOUND", su::ResultStatus::kSignalNotFound)
      .value("UNSUPPORTED_FORMAT", su::ResultStatus::kUnsupportedFormat)
      .value("PARSE_ERROR", su::ResultStatus::kParseError)
      .value("INVALID_INPUT", su::ResultStatus::kInvalidInput);

  py::class_<PyScalarResult>(module, "ScalarResult")
      .def(py::init<>())
      .def("ok", &PyScalarResult::ok)
      .def_readwrite("status", &PyScalarResult::status)
      .def_readwrite("message", &PyScalarResult::message)
      .def_readwrite("signal", &PyScalarResult::signal)
      .def_readwrite("value", &PyScalarResult::value);

  py::class_<PyDcSweep>(module, "DcSweep")
      .def(py::init<>())
      .def("ok", &PyDcSweep::ok)
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
      .def("__len__", &PyAcDerivedView::size)
      .def_readwrite("status", &PyAcDerivedView::status)
      .def_readwrite("message", &PyAcDerivedView::message)
      .def_readwrite("frequency_hz", &PyAcDerivedView::frequency_hz)
      .def_readwrite("magnitude_db", &PyAcDerivedView::magnitude_db)
      .def_readwrite("phase_deg", &PyAcDerivedView::phase_deg);

  py::class_<PyTranWaveform>(module, "TranWaveform")
      .def(py::init<>())
      .def("ok", &PyTranWaveform::ok)
      .def("__len__", &PyTranWaveform::size)
      .def_readwrite("status", &PyTranWaveform::status)
      .def_readwrite("message", &PyTranWaveform::message)
      .def_readwrite("signal", &PyTranWaveform::signal)
      .def_readwrite("time_s", &PyTranWaveform::time_s)
      .def_readwrite("value", &PyTranWaveform::value);

  py::class_<PyUgbwPhaseMarginResult>(module, "UgbwPhaseMarginResult")
      .def(py::init<>())
      .def("ok", &PyUgbwPhaseMarginResult::ok)
      .def_readwrite("status", &PyUgbwPhaseMarginResult::status)
      .def_readwrite("message", &PyUgbwPhaseMarginResult::message)
      .def_readwrite("unity_gain_bandwidth_hz",
                     &PyUgbwPhaseMarginResult::unity_gain_bandwidth_hz)
      .def_readwrite("phase_margin_deg", &PyUgbwPhaseMarginResult::phase_margin_deg);

  py::class_<PySettlingTimeResult>(module, "SettlingTimeResult")
      .def(py::init<>())
      .def("ok", &PySettlingTimeResult::ok)
      .def_readwrite("status", &PySettlingTimeResult::status)
      .def_readwrite("message", &PySettlingTimeResult::message)
      .def_readwrite("settling_time_s", &PySettlingTimeResult::settling_time_s);

  module.def("version", &su::version);

  module.def("libpsf_reader_enabled", []() {
#if SPICEUNION_ENABLE_LIBPSF_READER_FOR_PYTHON
    return true;
#else
    return false;
#endif
  });

  module.def("read_dc_value",
             [](const std::string& result_dir, const std::string& signal_name) {
               return to_python(su::read_dc_value(result_dir, signal_name));
             },
             py::arg("result_dir"), py::arg("signal_name"));

  module.def("read_dc_sweep",
             [](const std::string& result_dir, const std::string& sweep_name,
                const std::string& signal_name, const std::string& filename) {
               return to_python(
                   su::read_dc_sweep(result_dir, sweep_name, signal_name, filename));
             },
             py::arg("result_dir"), py::arg("sweep_name"), py::arg("signal_name"),
             py::arg("filename") = "dc.dc");

  module.def("read_ac_response",
             [](const std::string& result_dir, const std::string& signal_name,
                const std::string& filename) {
               return to_python(su::read_ac_response(result_dir, signal_name, filename));
             },
             py::arg("result_dir"), py::arg("signal_name"), py::arg("filename") = "ac.ac");

  module.def("read_tran_waveform",
             [](const std::string& result_dir, const std::string& signal_name,
                const std::string& filename) {
               return to_python(su::read_tran_waveform(result_dir, signal_name, filename));
             },
             py::arg("result_dir"), py::arg("signal_name"),
             py::arg("filename") = "tran.tran");

  module.def("derive_ac_view",
             [](const PyAcResponse& response) {
               return to_python(su::derive_ac_view(to_cpp(response)));
             },
             py::arg("response"));

  module.def("calculate_ugbw_and_phase_margin",
             [](const PyAcDerivedView& response) {
               return to_python(su::calculate_ugbw_and_phase_margin(to_cpp(response)));
             },
             py::arg("response"));

  module.def("calculate_settling_time",
             [](const PyTranWaveform& waveform, double target_value, double error_band) {
               return to_python(
                   su::calculate_settling_time(to_cpp(waveform), target_value, error_band));
             },
             py::arg("waveform"), py::arg("target_value"), py::arg("error_band") = 0.01);
}
