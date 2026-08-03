#include "src/parse/libpsf_backend.hpp"

#include <filesystem>
#include <memory>
#include <string>

#include "psf.h"

namespace su::parse {
namespace {

std::filesystem::path dcop_file_path(const std::string& result_dir) {
  return std::filesystem::path(result_dir) / "dcOp.dc";
}

ReadResult<ScalarResult> make_psf_exception_failure(const std::filesystem::path& psf_file,
                                                    const std::string& signal_name) {
  return ReadResult<ScalarResult>::failure(
      ResultStatus::kParseError,
      "failed to read signal '" + signal_name + "' from PSF file: " + psf_file.string());
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

}  // namespace su::parse
