#include "src/parse/psf_ascii_backend.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace su {
namespace parse {
namespace {

std::string trim(const std::string& line) {
  const auto begin = line.find_first_not_of(" \t\r");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = line.find_last_not_of(" \t\r");
  return line.substr(begin, end - begin + 1);
}

struct ParsedValue {
  std::string name;
  double value = 0.0;
  double imag = 0.0;
  bool is_complex = false;
};

// 解析 VALUE 段的一行：`"name" "type" value`、`"name" value` 或
// `"name" (real imag)`；PROP( 块内的属性行解析失败时返回 false 并被跳过。
bool parse_value_line(const std::string& line, ParsedValue* out) {
  const auto text = trim(line);
  if (text.empty() || text.front() != '"') {
    return false;
  }
  const auto close = text.find('"', 1);
  if (close == std::string::npos) {
    return false;
  }
  out->name = text.substr(1, close - 1);

  std::istringstream rest(text.substr(close + 1));
  char first = 0;
  if (!(rest >> first)) {
    return false;
  }
  if (first == '"') {
    std::string type_token;
    if (!(rest >> type_token)) {
      return false;
    }
    if (!(rest >> out->value)) {
      return false;
    }
    return true;
  }
  if (first == '(') {
    char close_paren = 0;
    if (!(rest >> out->value >> out->imag >> close_paren)) {
      return false;
    }
    out->is_complex = true;
    return true;
  }
  rest.putback(first);
  if (!(rest >> out->value)) {
    return false;
  }
  return true;
}

std::filesystem::path psf_file_path(const std::string& result_dir, const std::string& filename) {
  return std::filesystem::path(result_dir) / filename;
}

ReadResult<ScalarResult> missing_file(const std::string& filename,
                                      const std::filesystem::path& path) {
  return ReadResult<ScalarResult>::failure(
      ResultStatus::kFileNotFound, filename + " file was not found: " + path.string());
}

}  // namespace

bool is_psf_ascii_file(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return false;
  }
  char buffer[64] = {};
  input.read(buffer, sizeof(buffer));
  const auto count = input.gcount();
  for (std::streamsize index = 0; index < count; ++index) {
    if (buffer[index] == '\0') {
      return false;
    }
  }
  return count > 0;
}

ReadResult<ScalarResult> read_dc_value_with_ascii(const std::string& result_dir,
                                                  const std::string& signal_name) {
  if (result_dir.empty()) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kInvalidInput,
                                             "result_dir must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<ScalarResult>::failure(ResultStatus::kInvalidInput,
                                             "signal_name must not be empty");
  }

  const auto path = psf_file_path(result_dir, "dcOp.dc");
  std::ifstream input(path);
  if (!input) {
    return missing_file("dcOp.dc", path);
  }

  bool in_value = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto text = trim(line);
    if (text == "VALUE") {
      in_value = true;
      continue;
    }
    if (text == "END") {
      break;
    }
    if (!in_value) {
      continue;
    }
    ParsedValue value;
    if (!parse_value_line(text, &value) || value.is_complex) {
      continue;
    }
    if (value.name == signal_name) {
      return ReadResult<ScalarResult>::success({signal_name, value.value});
    }
  }

  return ReadResult<ScalarResult>::failure(
      ResultStatus::kSignalNotFound, "signal was not found in dcOp.dc: " + signal_name);
}

ReadResult<DcSweep> read_dc_sweep_with_ascii(const std::string& result_dir,
                                             const std::string& sweep_name,
                                             const std::string& signal_name,
                                             const std::string& filename) {
  const auto path = psf_file_path(result_dir, filename);
  std::ifstream input(path);
  if (!input) {
    return ReadResult<DcSweep>::failure(
        ResultStatus::kFileNotFound,
        "DC sweep PSF file was not found: " + path.string());
  }

  std::vector<double> sweep_values;
  std::vector<double> values;
  bool in_value = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto text = trim(line);
    if (text == "VALUE") {
      in_value = true;
      continue;
    }
    if (text == "END") {
      break;
    }
    if (!in_value) {
      continue;
    }
    ParsedValue value;
    if (!parse_value_line(text, &value) || value.is_complex) {
      continue;
    }
    if (value.name == sweep_name) {
      sweep_values.push_back(value.value);
    } else if (value.name == signal_name) {
      values.push_back(value.value);
    }
  }

  if (sweep_values.empty() || values.empty()) {
    return ReadResult<DcSweep>::failure(
        ResultStatus::kSignalNotFound,
        "sweep or signal was not found in " + filename + ": sweep=" + sweep_name +
            ", signal=" + signal_name);
  }
  if (sweep_values.size() != values.size()) {
    return ReadResult<DcSweep>::failure(
        ResultStatus::kParseError,
        "DC sweep shape mismatch in " + filename + ": sweep=" +
            std::to_string(sweep_values.size()) + ", signal=" + std::to_string(values.size()));
  }

  return ReadResult<DcSweep>::success(
      DcSweep{sweep_name, signal_name, std::move(sweep_values), std::move(values)});
}

ReadResult<AcResponse> read_ac_response_with_ascii(const std::string& result_dir,
                                                   const std::string& signal_name,
                                                   const std::string& filename) {
  const auto path = psf_file_path(result_dir, filename);
  std::ifstream input(path);
  if (!input) {
    return ReadResult<AcResponse>::failure(
        ResultStatus::kFileNotFound,
        "AC/STB PSF file was not found: " + path.string());
  }

  std::vector<double> frequencies;
  std::vector<double> real;
  std::vector<double> imag;
  bool in_value = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto text = trim(line);
    if (text == "VALUE") {
      in_value = true;
      continue;
    }
    if (text == "END") {
      break;
    }
    if (!in_value) {
      continue;
    }
    ParsedValue value;
    if (!parse_value_line(text, &value)) {
      continue;
    }
    if (value.name == "freq") {
      frequencies.push_back(value.value);
    } else if (value.name == signal_name && value.is_complex) {
      real.push_back(value.value);
      imag.push_back(value.imag);
    }
  }

  if (frequencies.empty() || real.empty()) {
    return ReadResult<AcResponse>::failure(
        ResultStatus::kSignalNotFound,
        "sweep or signal was not found in " + filename + ": signal=" + signal_name);
  }
  if (frequencies.size() != real.size() || real.size() != imag.size()) {
    return ReadResult<AcResponse>::failure(
        ResultStatus::kParseError,
        "AC/STB sweep shape mismatch in " + filename);
  }

  return ReadResult<AcResponse>::success(
      AcResponse{signal_name, std::move(frequencies), std::move(real), std::move(imag)});
}

ReadResult<TranWaveform> read_tran_waveform_with_ascii(const std::string& result_dir,
                                                       const std::string& signal_name,
                                                       const std::string& filename) {
  const auto path = psf_file_path(result_dir, filename);
  std::ifstream input(path);
  if (!input) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kFileNotFound,
        "transient PSF file was not found: " + path.string());
  }

  std::vector<double> time_values;
  std::vector<double> values;
  bool in_value = false;
  std::string line;
  while (std::getline(input, line)) {
    const auto text = trim(line);
    if (text == "VALUE") {
      in_value = true;
      continue;
    }
    if (text == "END") {
      break;
    }
    if (!in_value) {
      continue;
    }
    ParsedValue value;
    if (!parse_value_line(text, &value) || value.is_complex) {
      continue;
    }
    if (value.name == "time") {
      time_values.push_back(value.value);
    } else if (value.name == signal_name) {
      values.push_back(value.value);
    }
  }

  if (time_values.empty() || values.empty()) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kSignalNotFound,
        "sweep or signal was not found in " + filename + ": signal=" + signal_name);
  }
  if (time_values.size() != values.size()) {
    return ReadResult<TranWaveform>::failure(
        ResultStatus::kParseError,
        "transient waveform shape mismatch in " + filename);
  }

  return ReadResult<TranWaveform>::success(
      TranWaveform{signal_name, std::move(time_values), std::move(values)});
}

}  // namespace parse
}  // namespace su
