#include "su/toolchain.hpp"

#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace su {
namespace {

namespace fs = std::filesystem;

bool is_executable_file(const std::string& path) {
  std::error_code ec;
  const auto status = fs::status(path, ec);
  if (ec || !fs::is_regular_file(status)) {
    return false;
  }
  const auto perms = status.permissions();
  constexpr auto kExecMask =
      fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
  return (perms & kExecMask) != fs::perms::none;
}

std::vector<std::string> split_path_list(const char* path_text) {
  std::vector<std::string> out;
  if (path_text == nullptr) {
    return out;
  }
  std::string text(path_text);
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto sep = text.find(':', start);
    const auto piece = text.substr(start, sep == std::string::npos
                                           ? std::string::npos
                                           : sep - start);
    if (!piece.empty()) {
      out.push_back(piece);
    }
    if (sep == std::string::npos) {
      break;
    }
    start = sep + 1;
  }
  return out;
}

std::string trim_copy(std::string value) {
  const auto not_space = [](char c) {
    return c != ' ' && c != '\t' && c != '\r' && c != '*';
  };
  const auto first = std::find_if(value.begin(), value.end(), not_space);
  const auto last = std::find_if(value.rbegin(), value.rend(), not_space).base();
  if (first >= last) {
    return {};
  }
  return std::string(first, last);
}

std::string first_meaningful_line(const std::string& output) {
  std::string current;
  for (const char c : output) {
    if (c != '\n') {
      current.push_back(c);
      continue;
    }
    const auto line = trim_copy(current);
    if (!line.empty()) {
      return line;
    }
    current.clear();
  }
  return trim_copy(current);
}

struct VersionProbe {
  std::string text;
  std::string number;
};

std::string parse_version_number(const std::string& version_text);

// 尽力读取 `<exe> --version` 输出。探测失败返回空串，不影响主路径。
// 注意：Cadence Spectre 对 `--version` 会崩溃（实测 segment fault），因此 Spectre
// 不执行版本探测，只报告可执行文件本身；版本号留待后续用静态/其它安全方式补齐。
VersionProbe probe_ngspice_version(const std::string& exe) {
  const std::string command = "'" + exe + "' --version 2>/dev/null";
  FILE* pipe = ::popen(command.c_str(), "r");
  if (pipe == nullptr) {
    return {};
  }
  std::array<char, 512> buffer{};
  const std::size_t n = std::fread(buffer.data(), 1, buffer.size() - 1, pipe);
  buffer[n] = '\0';
  ::pclose(pipe);
  const std::string output(buffer.data(), n);
  VersionProbe result;
  result.text = first_meaningful_line(output);
  result.number = parse_version_number(output);
  return result;
}

// ngspice 不同年代输出差异较大，按已知形态顺序解析：
//   "** ngspice-41 : Circuit level simulation program"
//   "ngspice compiled from ngspice revision 27"
std::string parse_version_number(const std::string& version_text) {
  std::smatch match;
  if (std::regex_search(version_text, match,
                        std::regex(R"(ngspice[\s-]?([0-9]+(?:\.[0-9]+)?))",
                                   std::regex::icase))) {
    return match[1].str();
  }
  if (std::regex_search(version_text, match,
                        std::regex(R"(revision[=:\s]+([0-9]+(?:\.[0-9]+)?))",
                                   std::regex::icase))) {
    return match[1].str();
  }
  if (std::regex_search(version_text, match,
                        std::regex(R"(([0-9]+(?:\.[0-9]+){1,2}))"))) {
    return match[1].str();
  }
  return {};
}

SimulatorHandle probe_one(SimulatorKind kind, const std::string& candidate,
                          const char* source) {
  SimulatorHandle handle;
  handle.kind = kind;
  if (candidate.empty() || !is_executable_file(candidate)) {
    return handle;
  }
  handle.found = true;
  handle.executable_path = candidate;
  handle.discovered_from = source;
  if (kind == SimulatorKind::kNgspice) {
    const auto version = probe_ngspice_version(candidate);
    handle.version_text = version.text;
    handle.version_number = version.number;
  }
  return handle;
}

}  // namespace

const char* simulator_env_var(SimulatorKind kind) {
  switch (kind) {
    case SimulatorKind::kSpectre:
      return "SPICEUNION_SPECTRE";
    case SimulatorKind::kNgspice:
      return "SPICEUNION_NGSPICE";
  }
  return "";
}

SimulatorHandle find_simulator(SimulatorKind kind) {
  const char* explicit_path = std::getenv(simulator_env_var(kind));
  if (explicit_path != nullptr && *explicit_path != '\0') {
    auto handle = probe_one(kind, explicit_path, "env");
    if (handle.found) {
      return handle;
    }
  }

  std::array<const char*, 2> names{};
  std::size_t name_count = 0;
  if (kind == SimulatorKind::kSpectre) {
    names[0] = "spectre";
    name_count = 1;
  } else {
    names[0] = "ngspice_con";
    names[1] = "ngspice";
    name_count = 2;
  }

  for (const auto& dir : split_path_list(std::getenv("PATH"))) {
    for (std::size_t i = 0; i < name_count; ++i) {
      auto candidate = dir;
      if (!candidate.empty() && candidate.back() != '/') {
        candidate += '/';
      }
      candidate += names[i];
      auto handle = probe_one(kind, candidate, "path");
      if (handle.found) {
        return handle;
      }
    }
  }
  return {};
}

}  // namespace su
