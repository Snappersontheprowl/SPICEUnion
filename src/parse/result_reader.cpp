#include "su/result_reader.hpp"

#ifdef SPICEUNION_ENABLE_LIBPSF_READER
#include "src/parse/libpsf_backend.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

namespace su {
namespace {

constexpr double kPi = 3.141592653589793238462643383279502884;

bool finite(double value) {
  return std::isfinite(value);
}

bool all_finite(const std::vector<double>& values) {
  return std::all_of(values.begin(), values.end(), [](double value) { return finite(value); });
}

}  // namespace

ReadResult<ResultDirectory> find_result_directory(const std::string& work_dir) {
  if (work_dir.empty()) {
    return ReadResult<ResultDirectory>::failure(ResultStatus::kInvalidInput,
                                                "work_dir must not be empty");
  }

  const std::filesystem::path work_path(work_dir);
  std::error_code error;
  if (!std::filesystem::exists(work_path, error) ||
      !std::filesystem::is_directory(work_path, error)) {
    return ReadResult<ResultDirectory>::failure(
        ResultStatus::kDirectoryNotFound,
        "work_dir does not exist or is not a directory: " + work_dir);
  }

  std::vector<std::filesystem::path> raw_directories;
  for (std::filesystem::directory_iterator it(work_path, error), end; !error && it != end;
       it.increment(error)) {
    const auto& entry = *it;
    if (entry.is_directory(error) && entry.path().extension() == ".raw") {
      raw_directories.push_back(entry.path());
    }
  }

  if (error) {
    return ReadResult<ResultDirectory>::failure(
        ResultStatus::kParseError, "failed to scan result directory: " + error.message());
  }

  if (raw_directories.empty()) {
    return ReadResult<ResultDirectory>::success({work_path.string(), false});
  }

  std::sort(raw_directories.begin(), raw_directories.end());
  return ReadResult<ResultDirectory>::success({raw_directories.front().string(), true});
}

ReadResult<ScalarResult> read_dc_value(const std::string& result_dir,
                                       const std::string& signal_name) {
#ifdef SPICEUNION_ENABLE_LIBPSF_READER
  return parse::read_dc_value_with_libpsf(result_dir, signal_name);
#else
  (void)result_dir;
  (void)signal_name;
  return ReadResult<ScalarResult>::failure(
      ResultStatus::kUnsupportedFormat, "dcOp.dc reading requires SPICEUNION_ENABLE_LIBPSF_READER");
#endif
}

ReadResult<AcResponse> read_ac_response(const std::string& result_dir,
                                        const std::string& signal_name,
                                        const std::string& filename) {
#ifdef SPICEUNION_ENABLE_LIBPSF_READER
  return parse::read_ac_response_with_libpsf(result_dir, signal_name, filename);
#else
  (void)result_dir;
  (void)signal_name;
  (void)filename;
  return ReadResult<AcResponse>::failure(
      ResultStatus::kUnsupportedFormat, "AC file reading requires SPICEUNION_ENABLE_LIBPSF_READER");
#endif
}

ReadResult<TranWaveform> read_tran_waveform(const std::string& result_dir,
                                            const std::string& signal_name,
                                            const std::string& filename) {
#ifdef SPICEUNION_ENABLE_LIBPSF_READER
  return parse::read_tran_waveform_with_libpsf(result_dir, signal_name, filename);
#else
  (void)result_dir;
  (void)signal_name;
  (void)filename;
  return ReadResult<TranWaveform>::failure(
      ResultStatus::kUnsupportedFormat,
      "tran file reading requires SPICEUNION_ENABLE_LIBPSF_READER");
#endif
}

ReadResult<std::vector<SensitivityEntry>> read_sensitivity_legacy(const std::string& work_dir) {
  (void)work_dir;
  return ReadResult<std::vector<SensitivityEntry>>::failure(
      ResultStatus::kUnsupportedFormat,
      "legacy sensitivity reading is not on the current mainline");
}

ReadResult<AcDerivedView> derive_ac_view(const AcResponse& response) {
  if (!response.shape_consistent()) {
    return ReadResult<AcDerivedView>::failure(ResultStatus::kInvalidInput,
                                              "AC response vectors must have the same length");
  }
  if (response.frequency_hz.empty()) {
    return ReadResult<AcDerivedView>::failure(ResultStatus::kInvalidInput,
                                              "AC response must not be empty");
  }
  if (!all_finite(response.frequency_hz) || !all_finite(response.real) ||
      !all_finite(response.imag)) {
    return ReadResult<AcDerivedView>::failure(ResultStatus::kInvalidInput,
                                              "AC response contains non-finite values");
  }

  AcDerivedView view;
  view.frequency_hz = response.frequency_hz;
  view.magnitude_db.reserve(response.size());
  view.phase_deg.reserve(response.size());

  for (std::size_t i = 0; i < response.size(); ++i) {
    const double real = response.real[i];
    const double imag = response.imag[i];
    const double magnitude = std::hypot(real, imag);
    if (magnitude <= 0.0) {
      return ReadResult<AcDerivedView>::failure(ResultStatus::kInvalidInput,
                                                "AC response contains zero magnitude");
    }

    view.magnitude_db.push_back(20.0 * std::log10(magnitude));
    view.phase_deg.push_back(std::atan2(imag, real) * 180.0 / kPi);
  }

  return ReadResult<AcDerivedView>::success(std::move(view));
}

ReadResult<UgbwPhaseMarginResult> calculate_ugbw_and_phase_margin(const AcDerivedView& response) {
  if (!response.shape_consistent()) {
    return ReadResult<UgbwPhaseMarginResult>::failure(
        ResultStatus::kInvalidInput, "AC derived view vectors must have the same length");
  }
  if (response.frequency_hz.empty()) {
    return ReadResult<UgbwPhaseMarginResult>::failure(ResultStatus::kInvalidInput,
                                                      "AC derived view must not be empty");
  }
  if (!all_finite(response.frequency_hz) || !all_finite(response.magnitude_db) ||
      !all_finite(response.phase_deg)) {
    return ReadResult<UgbwPhaseMarginResult>::failure(ResultStatus::kInvalidInput,
                                                      "AC derived view contains non-finite values");
  }

  const auto crossing = std::find_if(response.magnitude_db.begin(), response.magnitude_db.end(),
                                     [](double magnitude_db) { return magnitude_db < 0.0; });

  if (crossing == response.magnitude_db.end()) {
    return ReadResult<UgbwPhaseMarginResult>::success({response.frequency_hz.back(), 0.0});
  }

  const std::size_t index =
      static_cast<std::size_t>(std::distance(response.magnitude_db.begin(), crossing));
  if (index == 0) {
    return ReadResult<UgbwPhaseMarginResult>::success(
        {response.frequency_hz.front(), 180.0 + response.phase_deg.front()});
  }

  const double f1 = response.frequency_hz[index - 1];
  const double f2 = response.frequency_hz[index];
  const double m1 = response.magnitude_db[index - 1];
  const double m2 = response.magnitude_db[index];
  const double p1 = response.phase_deg[index - 1];
  const double p2 = response.phase_deg[index];

  if (m1 == m2) {
    return ReadResult<UgbwPhaseMarginResult>::failure(ResultStatus::kInvalidInput,
                                                      "cannot interpolate unity-gain crossing");
  }

  const double ratio = (0.0 - m1) / (m2 - m1);
  const double ugbw = f1 + ratio * (f2 - f1);
  const double phase_at_ugbw = p1 + ratio * (p2 - p1);
  return ReadResult<UgbwPhaseMarginResult>::success({ugbw, 180.0 + phase_at_ugbw});
}

ReadResult<SettlingTimeResult> calculate_settling_time(const TranWaveform& waveform,
                                                       double target_value, double error_band) {
  if (!waveform.shape_consistent()) {
    return ReadResult<SettlingTimeResult>::failure(ResultStatus::kInvalidInput,
                                                   "waveform vectors must have the same length");
  }
  if (waveform.time_s.empty()) {
    return ReadResult<SettlingTimeResult>::failure(ResultStatus::kInvalidInput,
                                                   "waveform must not be empty");
  }
  if (!finite(target_value) || !finite(error_band) || error_band < 0.0) {
    return ReadResult<SettlingTimeResult>::failure(
        ResultStatus::kInvalidInput, "target value must be finite and error_band must be >= 0");
  }
  if (!all_finite(waveform.time_s) || !all_finite(waveform.value)) {
    return ReadResult<SettlingTimeResult>::failure(ResultStatus::kInvalidInput,
                                                   "waveform contains non-finite values");
  }

  const double tolerance = std::abs(target_value) * error_band;
  const double lower = target_value - tolerance;
  const double upper = target_value + tolerance;

  std::size_t last_out_of_band = std::numeric_limits<std::size_t>::max();
  for (std::size_t i = 0; i < waveform.size(); ++i) {
    if (waveform.value[i] < lower || waveform.value[i] > upper) {
      last_out_of_band = i;
    }
  }

  if (last_out_of_band == std::numeric_limits<std::size_t>::max()) {
    return ReadResult<SettlingTimeResult>::success({0.0});
  }
  if (last_out_of_band >= waveform.size() - 1) {
    return ReadResult<SettlingTimeResult>::success({waveform.time_s.back()});
  }
  return ReadResult<SettlingTimeResult>::success({waveform.time_s[last_out_of_band + 1]});
}

}  // namespace su
