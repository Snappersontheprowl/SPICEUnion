#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace su {

enum class ResultStatus {
  kOk,
  kDirectoryNotFound,
  kFileNotFound,
  kSignalNotFound,
  kUnsupportedFormat,
  kParseError,
  kInvalidInput,
};

const char* to_string(ResultStatus status) noexcept;

struct ResultError {
  ResultStatus status = ResultStatus::kOk;
  std::string message;

  bool ok() const noexcept {
    return status == ResultStatus::kOk;
  }
};

template <typename T>
struct ReadResult {
  ResultStatus status = ResultStatus::kInvalidInput;
  T value{};
  std::string error_message;

  bool ok() const noexcept {
    return status == ResultStatus::kOk;
  }

  explicit operator bool() const noexcept {
    return ok();
  }

  static ReadResult success(T value) {
    ReadResult result;
    result.status = ResultStatus::kOk;
    result.value = std::move(value);
    return result;
  }

  static ReadResult failure(ResultStatus status, std::string error_message = {}) {
    ReadResult result;
    result.status = status == ResultStatus::kOk ? ResultStatus::kInvalidInput : status;
    result.error_message = std::move(error_message);
    return result;
  }
};

struct ResultDirectory {
  std::string path;
  bool raw_directory_found = false;
};

struct ScalarResult {
  std::string signal;
  double value = 0.0;
};

struct DcSweep {
  std::string sweep_name;
  std::string signal;
  std::vector<double> sweep_values;
  std::vector<double> values;

  std::size_t size() const noexcept {
    return sweep_values.size();
  }

  bool shape_consistent() const noexcept {
    return sweep_values.size() == values.size();
  }
};

struct AcResponse {
  std::string signal;
  std::vector<double> frequency_hz;
  std::vector<double> real;
  std::vector<double> imag;

  std::size_t size() const noexcept {
    return frequency_hz.size();
  }

  bool shape_consistent() const noexcept {
    return frequency_hz.size() == real.size() && real.size() == imag.size();
  }
};

struct AcDerivedView {
  std::vector<double> frequency_hz;
  std::vector<double> magnitude_db;
  std::vector<double> phase_deg;

  std::size_t size() const noexcept {
    return frequency_hz.size();
  }

  bool shape_consistent() const noexcept {
    return frequency_hz.size() == magnitude_db.size() && magnitude_db.size() == phase_deg.size();
  }
};

struct UgbwPhaseMarginResult {
  double unity_gain_bandwidth_hz = 0.0;
  double phase_margin_deg = 0.0;
};

struct TranWaveform {
  std::string signal;
  std::vector<double> time_s;
  std::vector<double> value;

  std::size_t size() const noexcept {
    return time_s.size();
  }

  bool shape_consistent() const noexcept {
    return time_s.size() == value.size();
  }
};

struct SettlingTimeResult {
  double settling_time_s = 0.0;
};

struct SensitivityEntry {
  std::string key;
  std::string node;
  std::string parameter;
  double sensitivity = 0.0;
  double parameter_value = 0.0;
};

}  // namespace su
