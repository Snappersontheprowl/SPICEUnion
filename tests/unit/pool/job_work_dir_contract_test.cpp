#include "src/pool/simulator_pool.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    path_ = fs::temp_directory_path() /
            ("su_job_work_dir_test_" + std::to_string(::getpid()) + "_" +
             std::to_string(counter++));
    fs::create_directories(path_);
  }
  ~TempDir() {
    std::error_code error;
    fs::remove_all(path_, error);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

// 模拟“产物写在固定 worker 目录”的 session：池子必须在每个 job 结束后把产物快照到
// 每 job 目录，否则后一个 job 会覆盖前一个 (P0 回归)。
class FixedDirFakeSession final : public su::SimulatorSession {
 public:
  FixedDirFakeSession(std::size_t worker_id, std::string work_dir)
      : worker_id_(worker_id), work_dir_(std::move(work_dir)) {}

  void start() override { fs::create_directories(work_dir_); }

  su::TaskResult run(const su::ParameterState& state, std::chrono::seconds) override {
    const auto value = state.at("value");
    std::ofstream out(fs::path(work_dir_) / "artifact.txt", std::ios::trunc);
    out << value;
    out.close();
    return su::TaskResult::success(work_dir_);
  }

  void stop(bool) noexcept override {}

  std::size_t worker_id() const noexcept override { return worker_id_; }

  const std::string& work_dir() const noexcept override { return work_dir_; }

 private:
  std::size_t worker_id_ = 0;
  std::string work_dir_;
};

double read_artifact(const std::string& work_dir) {
  std::ifstream in(fs::path(work_dir) / "artifact.txt");
  double value = 0.0;
  in >> value;
  return value;
}

}  // namespace

TEST(JobWorkDirContractTest, EachJobGetsIsolatedWorkDir) {
  TempDir temp;
  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.timeout_seconds = 5;
  options.workspace_namespace = "job_work_dir_contract";

  su::SimulatorPool pool(
      options, temp.path().string(),
      [](std::size_t worker_id, const su::EvaluatorOptions&,
         const std::string& work_dir) -> su::SimulatorSessionPtr {
        return su::SimulatorSessionPtr(new FixedDirFakeSession(worker_id, work_dir));
      });

  pool.start_all();
  const std::vector<su::ParameterState> states{{{"value", 1.0}}, {{"value", 2.0}},
                                               {{"value", 3.0}}};
  const auto results = pool.evaluate_batch(states);
  pool.shutdown_all();

  ASSERT_EQ(states.size(), results.size());
  std::set<std::string> work_dirs;
  for (std::size_t index = 0; index < results.size(); ++index) {
    EXPECT_TRUE(results[index].ok()) << results[index].error_message;
    work_dirs.insert(results[index].work_dir);
    EXPECT_NE(std::string::npos, results[index].work_dir.find("case_00000"));
    EXPECT_DOUBLE_EQ(states[index].at("value"), read_artifact(results[index].work_dir));
  }
  // 三个 job 必须各自持有独立目录，且目录内容分别对应自己的输入参数。
  EXPECT_EQ(states.size(), work_dirs.size());
}
