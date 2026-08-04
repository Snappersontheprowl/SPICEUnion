#include "src/parse/libpsf_backend.hpp"

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
