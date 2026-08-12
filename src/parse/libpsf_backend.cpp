#include "src/parse/libpsf_backend.hpp"

#include <algorithm>
#include <complex>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "psf.h"

namespace su::parse {
namespace {

std::filesystem::path dcop_file_path(const std::string& result_dir) {
  return std::filesystem::path(result_dir) / "dcOp.dc";
}

std::filesystem::path result_file_path(const std::string& result_dir, const std::string& filename) {
  return std::filesystem::path(result_dir) / filename;
}

bool looks_like_psfxl_transient(const std::filesystem::path& psf_file) {
  return std::filesystem::is_regular_file(psf_file.string() + ".psfxl") ||
         std::filesystem::is_regular_file(psf_file.string() + ".sig");
}

ReadResult<ScalarResult> make_psf_exception_failure(const std::filesystem::path& psf_file,
                                                    const std::string& signal_name) {
  return ReadResult<ScalarResult>::failure(
      ResultStatus::kParseError,
      "failed to read signal '" + signal_name + "' from PSF file: " + psf_file.string());
}

ReadResult<TranWaveform> make_psf_tran_exception_failure(const std::filesystem::path& psf_file,
                                                         const std::string& signal_name) {
  return ReadResult<TranWaveform>::failure(
      ResultStatus::kParseError,
      "failed to read transient signal '" + signal_name + "' from PSF file: " + psf_file.string());
}

ReadResult<AcResponse> make_psf_ac_exception_failure(const std::filesystem::path& psf_file,
                                                     const std::string& signal_name) {
  return ReadResult<AcResponse>::failure(
      ResultStatus::kParseError,
      "failed to read complex response '" + signal_name + "' from PSF file: " + psf_file.string());
}

ReadResult<DcSweep> make_psf_dc_sweep_exception_failure(const std::filesystem::path& psf_file,
                                                        const std::string& signal_name) {
  return ReadResult<DcSweep>::failure(
      ResultStatus::kParseError,
      "failed to read DC sweep signal '" + signal_name + "' from PSF file: " + psf_file.string());
}

template <typename T>
std::vector<T> copy_vector(const std::vector<T>& values) {
  return {values.begin(), values.end()};
}

}  // namespace

ReadResult<ScalarResult> read_dc_value_with_libpsf(const std::string& result_dir,
                                                   const std::string& signal_name) {
  if (result_dir.empty()) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kInvalidInput,
                                             "result_dir must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kInvalidInput,
                                             "signal_name must not be empty");
  }

  const std::filesystem::path psf_file = dcop_file_path(result_dir);
  std::error_code error;
  if (!std::filesystem::exists(psf_file, error) ||
      !std::filesystem::is_regular_file(psf_file, error)) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kFileNotFound,
                                             "dcOp.dc file was not found: " + psf_file.string());
  }

  try {
    PSFDataSet dataset(psf_file.string());
    std::unique_ptr<PSFBase> signal(dataset.get_signal(signal_name));

    auto* scalar = dynamic_cast<PSFScalar*>(signal.get());
    if (scalar == nullptr) {
      return ReadResult<ScalarResult>::failure(
          ResultStatus::kUnsupportedFormat,
          "signal is not a scalar in dcOp.dc: " + signal_name);
    }

    return ReadResult<ScalarResult>::success({signal_name, static_cast<double>(*scalar)});
  } catch (const FileOpenError&) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kFileNotFound,
                                             "failed to open dcOp.dc file: " + psf_file.string());
  } catch (const NotFound&) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kSignalNotFound,
                                             "signal was not found in dcOp.dc: " + signal_name);
  } catch (const InvalidFileError&) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kParseError,
                                             "invalid PSF file: " + psf_file.string());
  } catch (const DataSetNotOpen&) {
    return make_psf_exception_failure(psf_file, signal_name);
  } catch (const std::exception& error) {
    return ReadResult<ScalarResult>::failure(
        ResultStatus::kParseError,
        "std exception while reading PSF file '" + psf_file.string() + "': " + error.what());
  } catch (...) {
    return make_psf_exception_failure(psf_file, signal_name);
  }
}

ReadResult<DcSweep> read_dc_sweep_with_libpsf(const std::string& result_dir,
                                              const std::string& sweep_name,
                                              const std::string& signal_name,
                                              const std::string& filename) {
  if (result_dir.empty()) {
    return ReadResult<DcSweep>::failure(ResultStatus::kInvalidInput,
                                        "result_dir must not be empty");
  }
  if (sweep_name.empty()) {
    return ReadResult<DcSweep>::failure(ResultStatus::kInvalidInput,
                                        "sweep_name must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<DcSweep>::failure(ResultStatus::kInvalidInput,
                                        "signal_name must not be empty");
  }
  if (filename.empty()) {
    return ReadResult<DcSweep>::failure(ResultStatus::kInvalidInput,
                                        "filename must not be empty");
  }

  const std::filesystem::path psf_file = result_file_path(result_dir, filename);
  std::error_code error;
  if (!std::filesystem::exists(psf_file, error) ||
      !std::filesystem::is_regular_file(psf_file, error)) {
    return ReadResult<DcSweep>::failure(ResultStatus::kFileNotFound,
                                        "DC sweep PSF file was not found: " + psf_file.string());
  }

  try {
    PSFDataSet dataset(psf_file.string());
    if (!dataset.is_swept() || dataset.get_nsweeps() != 1) {
      return ReadResult<DcSweep>::failure(
          ResultStatus::kUnsupportedFormat,
          "DC sweep PSF file must contain exactly one sweep axis: " + psf_file.string());
    }

    const auto sweep_names = dataset.get_sweep_param_names();
    if (std::find(sweep_names.begin(), sweep_names.end(), sweep_name) == sweep_names.end()) {
      return ReadResult<DcSweep>::failure(ResultStatus::kSignalNotFound,
                                          "sweep was not found in DC sweep PSF: " + sweep_name);
    }

    std::unique_ptr<PSFVector> sweep_values(dataset.get_sweep_values());
    auto* sweep_vector = dynamic_cast<PSFDoubleVector*>(sweep_values.get());
    if (sweep_vector == nullptr) {
      return ReadResult<DcSweep>::failure(
          ResultStatus::kUnsupportedFormat,
          "DC sweep values are not a double vector: " + psf_file.string());
    }

    std::unique_ptr<PSFBase> signal(dataset.get_signal(signal_name));
    auto* values = dynamic_cast<PSFDoubleVector*>(signal.get());
    if (values == nullptr) {
      return ReadResult<DcSweep>::failure(
          ResultStatus::kUnsupportedFormat,
          "DC sweep signal is not a real double vector: " + signal_name);
    }

    DcSweep sweep;
    sweep.sweep_name = sweep_name;
    sweep.signal = signal_name;
    sweep.sweep_values = copy_vector(*sweep_vector);
    sweep.values = copy_vector(*values);

    if (!sweep.shape_consistent()) {
      return ReadResult<DcSweep>::failure(
          ResultStatus::kParseError,
          "DC sweep axis and signal vector lengths differ: " + signal_name);
    }
    if (sweep.sweep_values.empty()) {
      return ReadResult<DcSweep>::failure(ResultStatus::kParseError,
                                          "DC sweep must not be empty: " + psf_file.string());
    }

    return ReadResult<DcSweep>::success(std::move(sweep));
  } catch (const FileOpenError&) {
    return ReadResult<DcSweep>::failure(ResultStatus::kFileNotFound,
                                        "failed to open DC sweep PSF file: " +
                                            psf_file.string());
  } catch (const NotFound&) {
    return ReadResult<DcSweep>::failure(ResultStatus::kSignalNotFound,
                                        "signal was not found in DC sweep PSF: " + signal_name);
  } catch (const InvalidFileError&) {
    return ReadResult<DcSweep>::failure(ResultStatus::kParseError,
                                        "invalid DC sweep PSF file: " + psf_file.string());
  } catch (const DataSetNotOpen&) {
    return make_psf_dc_sweep_exception_failure(psf_file, signal_name);
  } catch (const std::exception& error) {
    return ReadResult<DcSweep>::failure(
        ResultStatus::kParseError,
        "std exception while reading DC sweep PSF file '" + psf_file.string() + "': " +
            error.what());
  } catch (...) {
    return make_psf_dc_sweep_exception_failure(psf_file, signal_name);
  }
}

ReadResult<AcResponse> read_ac_response_with_libpsf(const std::string& result_dir,
                                                    const std::string& signal_name,
                                                    const std::string& filename) {
  if (result_dir.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kInvalidInput,
                                           "result_dir must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kInvalidInput,
                                           "signal_name must not be empty");
  }
  if (filename.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kInvalidInput,
                                           "filename must not be empty");
  }

  const std::filesystem::path psf_file = result_file_path(result_dir, filename);
  std::error_code error;
  if (!std::filesystem::exists(psf_file, error) ||
      !std::filesystem::is_regular_file(psf_file, error)) {
    return ReadResult<AcResponse>::failure(ResultStatus::kFileNotFound,
                                           "AC/STB PSF file was not found: " + psf_file.string());
  }

  try {
    PSFDataSet dataset(psf_file.string());
    std::unique_ptr<PSFVector> sweep(dataset.get_sweep_values());
    auto* frequency = dynamic_cast<PSFDoubleVector*>(sweep.get());
    if (frequency == nullptr) {
      return ReadResult<AcResponse>::failure(
          ResultStatus::kUnsupportedFormat,
          "AC/STB sweep values are not a double vector: " + psf_file.string());
    }

    std::unique_ptr<PSFBase> signal(dataset.get_signal(signal_name));
    auto* values = dynamic_cast<PSFComplexDoubleVector*>(signal.get());
    if (values == nullptr) {
      return ReadResult<AcResponse>::failure(
          ResultStatus::kUnsupportedFormat,
          "AC/STB signal is not a complex double vector: " + signal_name);
    }

    AcResponse response;
    response.signal = signal_name;
    response.frequency_hz = copy_vector(*frequency);
    response.real.reserve(values->size());
    response.imag.reserve(values->size());
    for (const auto& value : *values) {
      response.real.push_back(value.real());
      response.imag.push_back(value.imag());
    }

    if (!response.shape_consistent()) {
      return ReadResult<AcResponse>::failure(
          ResultStatus::kParseError,
          "AC/STB frequency and signal vector lengths differ: " + signal_name);
    }

    return ReadResult<AcResponse>::success(std::move(response));
  } catch (const FileOpenError&) {
    return ReadResult<AcResponse>::failure(ResultStatus::kFileNotFound,
                                           "failed to open AC/STB PSF file: " +
                                               psf_file.string());
  } catch (const NotFound&) {
    return ReadResult<AcResponse>::failure(ResultStatus::kSignalNotFound,
                                           "signal was not found in AC/STB PSF: " + signal_name);
  } catch (const InvalidFileError&) {
    return ReadResult<AcResponse>::failure(ResultStatus::kParseError,
                                           "invalid AC/STB PSF file: " + psf_file.string());
  } catch (const DataSetNotOpen&) {
    return make_psf_ac_exception_failure(psf_file, signal_name);
  } catch (const std::exception& error) {
    return ReadResult<AcResponse>::failure(
        ResultStatus::kParseError,
        "std exception while reading AC/STB PSF file '" + psf_file.string() + "': " +
            error.what());
  } catch (...) {
    return make_psf_ac_exception_failure(psf_file, signal_name);
  }
}

ReadResult<TranWaveform> read_tran_waveform_with_libpsf(const std::string& result_dir,
                                                        const std::string& signal_name,
                                                        const std::string& filename) {
  if (result_dir.empty()) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kInvalidInput,
                                             "result_dir must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kInvalidInput,
                                             "signal_name must not be empty");
  }
  if (filename.empty()) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kInvalidInput,
                                             "filename must not be empty");
  }

  const std::filesystem::path psf_file = result_file_path(result_dir, filename);
  std::error_code error;
  if (!std::filesystem::exists(psf_file, error) ||
      !std::filesystem::is_regular_file(psf_file, error)) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kFileNotFound,
                                             "transient PSF file was not found: " +
                                                 psf_file.string());
  }

  if (looks_like_psfxl_transient(psf_file)) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kUnsupportedFormat,
        "PSFXL transient files are not supported by the henjo/libpsf backend: " +
            psf_file.string());
  }

  try {
    PSFDataSet dataset(psf_file.string());
    std::unique_ptr<PSFVector> sweep(dataset.get_sweep_values());
    auto* time = dynamic_cast<PSFDoubleVector*>(sweep.get());
    if (time == nullptr) {
      return ReadResult<TranWaveform>::failure(
          ResultStatus::kUnsupportedFormat,
          "transient sweep values are not a double vector: " + psf_file.string());
    }

    std::unique_ptr<PSFBase> signal(dataset.get_signal(signal_name));
    auto* values = dynamic_cast<PSFDoubleVector*>(signal.get());
    if (values == nullptr) {
      return ReadResult<TranWaveform>::failure(
          ResultStatus::kUnsupportedFormat,
          "transient signal is not a real double vector: " + signal_name);
    }

    TranWaveform waveform;
    waveform.signal = signal_name;
    waveform.time_s = copy_vector(*time);
    waveform.value = copy_vector(*values);
    if (!waveform.shape_consistent()) {
      return ReadResult<TranWaveform>::failure(
          ResultStatus::kParseError,
          "transient time and signal vector lengths differ: " + signal_name);
    }

    return ReadResult<TranWaveform>::success(std::move(waveform));
  } catch (const FileOpenError&) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kFileNotFound,
                                             "failed to open transient PSF file: " +
                                                 psf_file.string());
  } catch (const NotFound&) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kSignalNotFound, "signal was not found in transient PSF: " + signal_name);
  } catch (const InvalidFileError&) {
    return ReadResult<TranWaveform>::failure(ResultStatus::kParseError,
                                             "invalid transient PSF file: " + psf_file.string());
  } catch (const DataSetNotOpen&) {
    return make_psf_tran_exception_failure(psf_file, signal_name);
  } catch (const std::exception& error) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kParseError,
        "std exception while reading transient PSF file '" + psf_file.string() +
            "': " + error.what());
  } catch (...) {
    return make_psf_tran_exception_failure(psf_file, signal_name);
  }
}

}  // namespace su::parse
