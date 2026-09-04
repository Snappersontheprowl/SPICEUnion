// spiceunion doctor：一次性报告本机可探测到的仿真器与版本。
//
// 用途：在新机器上快速确认 SPICEUnion 能调用哪些仿真器、分别是什么版本、
//       以及缺失时的修复建议。MVP 只报告“探测事实”，不做版本门控。
//
// 构建（在启用 BUILD_TESTS 的任意 preset 后）：
//   cmake --build --preset default
//   ./build/default/spiceunion_doctor

#include "su/toolchain.hpp"

#include <iostream>
#include <string>

namespace {

const char* kind_name(su::SimulatorKind kind) {
  return kind == su::SimulatorKind::kSpectre ? "Spectre" : "Ngspice";
}

const char* suggestion(su::SimulatorKind kind) {
  if (kind == su::SimulatorKind::kSpectre) {
    return "user action required: set SPICEUNION_SPECTRE=<path>, or install "
           "Cadence Spectre yourself and put "
           "`spectre` on PATH";
  }
  return "user action required: set SPICEUNION_NGSPICE=<path>, or install "
         "ngspice yourself (e.g. conda "
         "create -n ngspice -c conda-forge ngspice) and put it on PATH";
}

void report(su::SimulatorKind kind) {
  const auto handle = su::find_simulator(kind);
  std::cout << "[" << kind_name(kind) << "]\n";
  if (!handle.found) {
    std::cout << "  found: no\n  suggestion: " << suggestion(kind) << "\n";
    return;
  }
  std::cout << "  found: yes\n";
  std::cout << "  executable: " << handle.executable_path << "\n";
  std::cout << "  source: " << handle.discovered_from << "\n";
  if (!handle.version_text.empty()) {
    std::cout << "  version text: " << handle.version_text << "\n";
  }
  if (!handle.version_number.empty()) {
    std::cout << "  version number: " << handle.version_number << "\n";
  } else {
    std::cout << "  version number: (unavailable)\n";
  }
}

}  // namespace

int main() {
  report(su::SimulatorKind::kSpectre);
  std::cout << "\n";
  report(su::SimulatorKind::kNgspice);
  return 0;
}
