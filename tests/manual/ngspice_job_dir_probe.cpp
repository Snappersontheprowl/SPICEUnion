// Ngspice 每 job 子目录探针（手动工具，不进入 ctest）
//
// 目的：验证专题 06「方案三」在 Ngspice 侧的可行性——每个 job 使用独立的
// worker 子目录作为子进程工作目录，产物互不覆盖，且各自可读。
//
// 说明：Ngspice backend 的 run 本身每次都会新起 `ngspice -b` 子进程，不存在需要
// 复用的长驻进程；因此“每 job 子目录”只需为每个 job 指定不同 work_dir。
//
// 构建与运行：
//   cmake --build --preset default --target spiceunion_ngspice_job_dir_probe
//   ./build/default/tests/spiceunion_ngspice_job_dir_probe

#include "su/ngspice_session.hpp"
#include "su/toolchain.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

struct CaseResult {
  int index = 0;
  double capacitance = 0.0;
  std::string work_dir;
  std::size_t artifact_count = 0;
  double fc_estimate_hz = 0.0;
  bool read_ok = false;
};

std::vector<fs::path> list_files(const fs::path& dir) {
  std::vector<fs::path> files;
  std::error_code ec;
  if (!fs::exists(dir, ec)) {
    return files;
  }
  for (auto it = fs::recursive_directory_iterator(dir, ec);
       !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (it->is_regular_file(ec)) {
      files.push_back(fs::relative(it->path(), dir, ec));
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

// 一阶 RC 低通：找 |H| 首次跌破 1/sqrt(2) 的频点，按对数频率线性插值。
double estimate_fc(const su::AcResponse& ac) {
  const double threshold = 1.0 / std::sqrt(2.0);
  for (std::size_t i = 1; i < ac.frequency_hz.size(); ++i) {
    const double previous = std::hypot(ac.real[i - 1], ac.imag[i - 1]);
    const double current = std::hypot(ac.real[i], ac.imag[i]);
    if (previous >= threshold && current < threshold && previous > current) {
      const double log_previous = std::log(ac.frequency_hz[i - 1]);
      const double log_current = std::log(ac.frequency_hz[i]);
      const double ratio = (previous - threshold) / (previous - current);
      return std::exp(log_previous + ratio * (log_current - log_previous));
    }
  }
  return 0.0;
}

}  // namespace

int main() {
  const auto handle = su::find_simulator(su::SimulatorKind::kNgspice);
  if (!handle.found) {
    std::cout << "未找到 ngspice：请设置 SPICEUNION_NGSPICE 或加入 PATH。\n";
    return 2;
  }
  std::cout << "ngspice: " << handle.executable_path << " (source="
            << handle.discovered_from << ", version="
            << (handle.version_number.empty() ? "unavailable" : handle.version_number)
            << ")\n";

  const auto run_root =
      fs::path("local/runtime/ngspice_job_dir_probe") /
      ("run_" + std::to_string(static_cast<long long>(
                    std::chrono::system_clock::now().time_since_epoch().count())));
  fs::create_directories(run_root);

  const std::vector<double> capacitances{1e-12, 2e-12, 4e-12};
  std::vector<CaseResult> results;

  for (std::size_t index = 0; index < capacitances.size(); ++index) {
    // 每个 job 一个独立目录，模拟“worker 内每 job 子目录”。
    const auto job_dir =
        run_root / "worker_0" /
        ("job_" + std::string(index < 10 ? "000" : "00") + std::to_string(index) +
         "_case_" + (index < 10 ? std::string("000") : std::string("00")) +
         std::to_string(index));

    su::EvaluatorOptions options;
    options.netlist_path = "ngspice_builtin.cir";
    options.num_workers = 1;
    options.timeout_seconds = 60;
    options.result_format = su::ResultFormat::kNspiceWrdata;

    su::NgspiceSession session(0, options, job_dir.string(),
                               su::NgspiceBuiltinTask::kRcAc);
    session.start();
    const auto task = session.run(
        su::ParameterState{{"resistance_ohm", 1000.0},
                           {"capacitance_f", capacitances[index]}},
        std::chrono::seconds(60));
    session.stop(true);

    CaseResult item;
    item.index = static_cast<int>(index);
    item.capacitance = capacitances[index];
    item.work_dir = task.work_dir;
    item.artifact_count = list_files(job_dir).size();

    const auto ac = su::read_ngspice_wrdata_ac_response(
        (job_dir / "rc_ac.out").string(), "v(out)");
    if (ac.ok()) {
      item.read_ok = true;
      item.fc_estimate_hz = estimate_fc(ac.value);
    }

    std::cout << "case " << index << ": task=" << su::to_string(task.status)
              << " C=" << std::setprecision(3) << capacitances[index] * 1e12 << "p"
              << " work_dir=" << task.work_dir
              << " 产物=" << item.artifact_count << " 个"
              << " read_ok=" << (item.read_ok ? "yes" : "no")
              << " fc≈" << std::setprecision(4) << item.fc_estimate_hz / 1e6 << " MHz"
              << "（理论 " << 1.0 / (2 * M_PI * 1000.0 * capacitances[index]) / 1e6
              << " MHz）\n";
    results.push_back(item);
  }

  bool distinct_dirs = true;
  bool distinct_values = true;
  for (std::size_t i = 1; i < results.size(); ++i) {
    if (results[i].work_dir == results[i - 1].work_dir) {
      distinct_dirs = false;
    }
    if (std::abs(results[i].fc_estimate_hz - results[i - 1].fc_estimate_hz) < 1.0) {
      distinct_values = false;
    }
  }
  const bool all_readable = std::all_of(results.begin(), results.end(),
                                        [](const CaseResult& r) { return r.read_ok; });

  std::cout << "\n[结论] work_dir 各不相同=" << (distinct_dirs ? "yes" : "no")
            << "，读数各不相同=" << (distinct_values ? "yes" : "no")
            << "，全部可读=" << (all_readable ? "yes" : "no") << "\n";
  std::cout << "产物保留在 " << run_root << "，结论回填专题 06「验证结果」。\n";
  return (distinct_dirs && distinct_values && all_readable) ? 0 : 1;
}
