#include "su/result.hpp"

namespace su {

const char* to_string(ResultStatus status) noexcept {
  switch (status) {
    case ResultStatus::kOk:
      return "ok";
    case ResultStatus::kDirectoryNotFound:
      return "directory_not_found";
    case ResultStatus::kFileNotFound:
      return "file_not_found";
    case ResultStatus::kSignalNotFound:
      return "signal_not_found";
    case ResultStatus::kUnsupportedFormat:
      return "unsupported_format";
    case ResultStatus::kParseError:
      return "parse_error";
    case ResultStatus::kInvalidInput:
      return "invalid_input";
  }
  return "unknown";
}

}  // namespace su
