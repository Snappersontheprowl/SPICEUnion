#include "src/pool/simulator_pool.hpp"

#include <gtest/gtest.h>

#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    path_ = fs::temp_directory_path() /
            ("su_metrics_pipeline_test_" + std::to_string(::getpid()) + "_" +
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

// 每个 job 把 PSFASCII fixture 复制进固定 worker 目录（模拟工具产物），
// 由池子负责快照到每 job 目录并在归还 worker 前完成指标提取。
class AsciiFixtureSession final : public su::SimulatorSession {
 public:
  AsciiFixtureSession(std::size_t worker_id, std::string work_dir)
      : worker_id_(worker_id), work_dir_(std::move(work_dir)) {}

  void start() override { fs::create_directories(work_dir_); }

  su::TaskResult run(const su::ParameterState&, std::chrono::seconds) override {
    const fs::path artifact_dir = fs::path(work_dir_) / "probe.raw";
    fs::create_directories(artifact_dir);
    fs::copy_file(fs::path(SPICEUNION_FIXTURE_ROOT) / "psf_ascii" / "bgr_amp_dc_op.raw" /
                      "dcOp.dc",
                  artifact_dir / "dcOp.dc", fs::copy_options::overwrite_existing);
    auto result = su::TaskResult::success(work_dir_);
    result.result_format = su::ResultFormat::kPsfAscii;
    return result;
  }

  void stop(bool) noexcept override {}

  std::size_t worker_id() const noexcept override { return worker_id_; }

  const std::string& work_dir() const noexcept override { return work_dir_; }

 private:
  std::size_t worker_id_ = 0;
  std::string work_dir_;
};

}  // namespace

TEST(MetricsPipelineContractTest, ExtractsMetricsInsideWorkerAndRetainsArtifacts) {
  TempDir temp;
  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.timeout_seconds = 5;
  options.workspace_namespace = "metrics_pipeline_contract";
  su::MetricRequest request;
  request.kind = su::MetricKind::kDcValue;
  request.signal = "V_BGR";
  options.metrics = {request};

  su::SimulatorPool pool(
      options, temp.path().string(),
      [](std::size_t worker_id, const su::EvaluatorOptions&,
         const std::string& work_dir) -> su::SimulatorSessionPtr {
        return su::SimulatorSessionPtr(new AsciiFixtureSession(worker_id, work_dir));
      });

  pool.start_all();
  const auto results = pool.evaluate_batch({{{"tag", 1.0}}});
  pool.shutdown_all();

  ASSERT_EQ(1u, results.size());
  const auto& result = results[0];
  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(su::MetricsStatus::kOk, result.metrics.status) << result.metrics.message;
  ASSERT_EQ(1u, result.metrics.values.size());
  EXPECT_EQ("V_BGR", result.metrics.values[0].name);
  EXPECT_NE(0.0, result.metrics.values[0].value);
  // 方案一只改变提取时机，不改变产物保留策略：产物仍在 job 目录中。
  EXPECT_TRUE(fs::exists(result.work_dir));
  EXPECT_FALSE(fs::is_empty(result.work_dir));
}

TEST(MetricsPipelineContractTest, ReportsExtractionFailureWithoutFailingSimulation) {
  TempDir temp;
  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.timeout_seconds = 5;
  options.workspace_namespace = "metrics_pipeline_contract_fail";
  su::MetricRequest request;
  request.kind = su::MetricKind::kDcValue;
  request.signal = "does_not_exist";
  options.metrics = {request};

  su::SimulatorPool pool(
      options, temp.path().string(),
      [](std::size_t worker_id, const su::EvaluatorOptions&,
         const std::string& work_dir) -> su::SimulatorSessionPtr {
        return su::SimulatorSessionPtr(new AsciiFixtureSession(worker_id, work_dir));
      });

  pool.start_all();
  const auto results = pool.evaluate_batch({{{"tag", 1.0}}});
  pool.shutdown_all();

  ASSERT_EQ(1u, results.size());
  const auto& result = results[0];
  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(su::MetricsStatus::kSignalNotFound, result.metrics.status);
  EXPECT_FALSE(fs::is_empty(result.work_dir));
}
