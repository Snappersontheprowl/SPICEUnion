// Spectre 常驻会话产物目录探针（手动工具，不进入 ctest）
//
// 目的：验证专题 06「方案三：worker 内每 job 子目录（会话常驻）」在 Spectre 上
// 是否可行。当前自动执行实验 A：
//   同一个常驻 interactive 会话连续运行两次 → 观察产物目录/文件是“每次新目录”
//   还是“复用同名路径被覆盖”。
//
// 实验 B（SKILL 是否支持 per-run 输出目录）需要人工按打印的清单探查，
// 结论写回：doc/develop_doc/20_专题记录/06_产物生命周期与指标提取设计.md
//          「方案三 -> 验证结果」；机器相关细节写本地私有笔记。
//
// 构建与运行：
//   cmake --build --preset default --target spiceunion_spectre_output_dir_probe
//   ./build/default/tests/spiceunion_spectre_output_dir_probe
//   # 或指定自己的 RC 网表（需含 ac 分析与 save）
//   ./build/default/tests/spiceunion_spectre_output_dir_probe --netlist path/to/input.scs

#include "su/spectre_session.hpp"
#include "su/toolchain.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr const char* kDefaultNetlist =
    "simulator lang=spectre\n"
    "\n"
    "parameters r=1k c=1p\n"
    "\n"
    "Vin (in 0) vsource dc=0 mag=1\n"
    "R1 (in out) resistor r=r\n"
    "C1 (out 0) capacitor c=c\n"
    "\n"
    "ac ac start=1Meg stop=10G dec=20\n"
    "save out\n";

struct FileEntry {
  std::string relative_path;
  std::uintmax_t size = 0;
  std::filesystem::file_time_type mtime{};
};

std::map<std::string, FileEntry> snapshot(const fs::path& root) {
  std::map<std::string, FileEntry> entries;
  std::error_code ec;
  if (!fs::exists(root, ec)) {
    return entries;
  }
  for (auto it = fs::recursive_directory_iterator(root, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    const auto& entry = *it;
    if (!entry.is_regular_file(ec)) {
      continue;
    }
    FileEntry item;
    item.relative_path = fs::relative(entry.path(), root, ec).string();
    item.size = fs::file_size(entry.path(), ec);
    item.mtime = fs::last_write_time(entry.path(), ec);
    entries[item.relative_path] = item;
  }
  return entries;
}

void print_snapshot(const char* label, const std::map<std::string, FileEntry>& files) {
  std::cout << "  [" << label << "] 产物文件（" << files.size() << "）:\n";
  for (const auto& [path, item] : files) {
    std::cout << "    " << path << "  " << item.size << " bytes\n";
  }
}

void print_delta(const std::map<std::string, FileEntry>& before,
                 const std::map<std::string, FileEntry>& after) {
  std::vector<std::string> added;
  std::vector<std::string> changed;
  for (const auto& [path, item] : after) {
    const auto it = before.find(path);
    if (it == before.end()) {
      added.push_back(path);
    } else if (it->second.size != item.size || it->second.mtime != item.mtime) {
      changed.push_back(path);
    }
  }

  std::cout << "  [差异] 新增 " << added.size() << " 个，变化 " << changed.size()
            << " 个\n";
  for (const auto& p : added) {
    std::cout << "    + " << p << "\n";
  }
  for (const auto& p : changed) {
    std::cout << "    ~ " << p << "（同名路径被重写）\n";
  }
  if (added.empty() && !changed.empty()) {
    std::cout << "  [结论] 两次 run 复用同名产物路径 → 当前会话下“每 job 独立目录”"
                 "不可直接通过换目录实现，需走实验 B 或快照方案。\n";
  } else if (!added.empty()) {
    std::cout << "  [结论] 两次 run 产生了新增路径 → 会话本身具备一定的 run 级隔离，"
                 "需进一步确认命名规则。\n";
  }
}

int write_default_netlist(const fs::path& path) {
  std::ofstream out(path);
  if (!out) {
    return 1;
  }
  out << kDefaultNetlist;
  return out ? 0 : 1;
}

void print_experiment_b_checklist() {
  std::cout << "\n实验 B（人工，判定 SKILL 能否 per-run 指定输出目录）：\n"
            << "  1. 在常驻 interactive 会话中，于两次 run 之间尝试设置结果输出位置，"
               "候选方向：\n"
            << "     - sclRun 之外的输出目录/结果名设置命令；\n"
            << "     - 通过 SKILL 改变当前工作目录或结果目录变量；\n"
            << "  2. 每次尝试后重复实验 A 的快照对比，观察是否生成独立目录；\n"
            << "  3. 记录命令、返回信息与产物路径，判定路径 ① 是否可行。\n"
            << "  结论写入专题 06「方案三 -> 验证结果」；机器相关细节写本地私有笔记。\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::string netlist_argument;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--netlist" && i + 1 < argc) {
      netlist_argument = argv[++i];
    }
  }

  const auto handle = su::find_simulator(su::SimulatorKind::kSpectre);
  if (!handle.found) {
    std::cout << "未找到 spectre：请设置 SPICEUNION_SPECTRE 或加入 PATH（"
              << su::simulator_env_var(su::SimulatorKind::kSpectre) << "）。\n"
              << "该探针需要真实 Spectre 许可，属手动工具，不进入 ctest。\n";
    return 2;
  }
  std::cout << "spectre: " << handle.executable_path << " (source="
            << handle.discovered_from << ", version="
            << (handle.version_number.empty() ? "unavailable" : handle.version_number)
            << ")\n";

  const auto run_root =
      fs::path("local/runtime/spectre_output_dir_probe") /
      ("run_" + std::to_string(static_cast<long long>(
                    std::chrono::system_clock::now().time_since_epoch().count())));
  fs::create_directories(run_root);

  fs::path netlist_path = netlist_argument;
  if (netlist_path.empty()) {
    netlist_path = run_root / "probe_input.scs";
    if (write_default_netlist(netlist_path) != 0) {
      std::cout << "无法写入默认网表: " << netlist_path << "\n";
      return 2;
    }
  }
  // Spectre 子进程会 chdir 到 worker 目录，网表路径必须能被该目录解析。
  netlist_path = fs::absolute(netlist_path);

  su::EvaluatorOptions options;
  options.netlist_path = netlist_path.string();
  options.num_workers = 1;
  options.timeout_seconds = 120;

  const auto worker_dir = run_root / "worker_0";
  std::cout << "会话根目录: " << worker_dir << "\n";

  su::SpectreSession session(0, options, worker_dir.string());
  std::cout << "[实验 A] 启动常驻交互会话并连续运行两次...\n";
  session.start();

  const auto first = session.run(su::ParameterState{{"r", 1000.0}, {"c", 1e-12}},
                                 std::chrono::seconds(120));
  std::cout << "  第 1 次 run: " << su::to_string(first.status)
            << " work_dir=" << first.work_dir << "\n";
  const auto after_first = snapshot(worker_dir);
  print_snapshot("第 1 次 run 后", after_first);

  const auto second = session.run(su::ParameterState{{"r", 1000.0}, {"c", 4e-12}},
                                  std::chrono::seconds(120));
  std::cout << "  第 2 次 run: " << su::to_string(second.status)
            << " work_dir=" << second.work_dir << "\n";
  const auto after_second = snapshot(worker_dir);
  print_snapshot("第 2 次 run 后", after_second);
  print_delta(after_first, after_second);

  session.stop(true);
  print_experiment_b_checklist();
  std::cout << "\n产物保留在 " << run_root << "，便于与专题结论对照。\n";
  return 0;
}
