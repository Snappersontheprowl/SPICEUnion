#pragma once

#include "su/simulator_kind.hpp"

#include <string>

namespace su {

// 一次探测得到的仿真器事实。MVP 只负责“找到 + 报告”，不按版本做门控。
struct SimulatorHandle {
  SimulatorKind kind = SimulatorKind::kSpectre;
  bool found = false;
  std::string executable_path;   // 找到的可执行文件绝对/显式路径
  std::string version_text;      // `--version` 输出的首行原文（探测失败则为空）
  std::string version_number;    // 从 version_text 提取的版本号（尽力解析）
  std::string discovered_from;   // "env" 或 "path"
};

// 返回该仿真器使用的环境变量名，例如 SPICEUNION_NGSPICE。
const char* simulator_env_var(SimulatorKind kind);

// 按“环境变量显式指定 → PATH 候选”的顺序探测仿真器。
SimulatorHandle find_simulator(SimulatorKind kind);

}  // namespace su
