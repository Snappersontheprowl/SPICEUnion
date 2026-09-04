#include "su/evaluator.hpp"
#include "su/ngspice_session.hpp"

#include "support/ngspice_external_env.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path test_root(const std::string& name) {
  return std::filesystem::path(SPICEUNION_PROJECT_ROOT) / "local/runtime" /
         ("ngspice_" + name + "_" + std::to_string(::getpid()));
}

}  // namespace

TEST(NgspiceEvaluatorExternalTest, RunsBatchThroughEvaluatorWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_evaluator");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 2;
  options.work_dir_base = root.string();
  options.workspace_namespace = "batch";
  options.timeout_seconds = 20;

  auto evaluator = su::make_ngspice_evaluator(options);
  const auto results = evaluator.run({
      su::ParameterState{{"resistance_ohm", 1000.0}, {"capacitance_f", 1.0e-12}},
      su::ParameterState{{"resistance_ohm", 10000.0}, {"capacitance_f", 1.0e-12}},
  });

  ASSERT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].ok()) << results[0].error_message;
  EXPECT_TRUE(results[1].ok()) << results[1].error_message;
  EXPECT_NE(results[0].work_dir, results[1].work_dir);

  evaluator.cleanup();
  std::filesystem::remove_all(root);
}

TEST(NgspiceEvaluatorExternalTest, RunsDcBatchThroughEvaluatorWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_dc_evaluator");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 2;
  options.work_dir_base = root.string();
  options.workspace_namespace = "dc_batch";
  options.timeout_seconds = 20;

  auto evaluator = su::make_ngspice_evaluator(options, su::NgspiceBuiltinTask::kResistorDividerDc);
  const auto results = evaluator.run({
      su::ParameterState{
          {"top_resistance_ohm", 1000.0}, {"bottom_resistance_ohm", 1000.0}, {"dc_step_v", 0.2}},
      su::ParameterState{
          {"top_resistance_ohm", 3000.0}, {"bottom_resistance_ohm", 1000.0}, {"dc_step_v", 0.1}},
  });

  ASSERT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].ok()) << results[0].error_message;
  EXPECT_TRUE(results[1].ok()) << results[1].error_message;
  EXPECT_NE(results[0].work_dir, results[1].work_dir);
  EXPECT_NE(results[0].detail.find("ngspice_dc_output="), std::string::npos);
  EXPECT_NE(results[1].detail.find("ngspice_dc_output="), std::string::npos);

  evaluator.cleanup();
  std::filesystem::remove_all(root);
}

TEST(NgspiceEvaluatorExternalTest, RunsTranBatchThroughEvaluatorWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_tran_evaluator");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 2;
  options.work_dir_base = root.string();
  options.workspace_namespace = "tran_batch";
  options.timeout_seconds = 20;

  auto evaluator = su::make_ngspice_evaluator(options, su::NgspiceBuiltinTask::kRcTran);
  const auto results = evaluator.run({
      su::ParameterState{
          {"resistance_ohm", 1000.0}, {"capacitance_f", 1.0e-12}, {"tran_stop_s", 1.0e-8}},
      su::ParameterState{
          {"resistance_ohm", 10000.0}, {"capacitance_f", 1.0e-12}, {"tran_stop_s", 1.0e-7}},
  });

  ASSERT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].ok()) << results[0].error_message;
  EXPECT_TRUE(results[1].ok()) << results[1].error_message;
  EXPECT_NE(results[0].work_dir, results[1].work_dir);
  EXPECT_NE(results[0].detail.find("ngspice_tran_output="), std::string::npos);
  EXPECT_NE(results[1].detail.find("ngspice_tran_output="), std::string::npos);

  evaluator.cleanup();
  std::filesystem::remove_all(root);
}
