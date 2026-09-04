#include "su/evaluator.hpp"
#include "su/spectre_session.hpp"

#include "spectre_external_env.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

su::EvaluatorOptions spectre_options() {
  su::EvaluatorOptions options;
  options.netlist_path = spectre_materials_netlist_path();
  options.num_workers = 1;
  options.work_dir_base = spectre_runtime_root("lifecycle");
  options.workspace_namespace = "lifecycle";
  options.timeout_seconds = 20;
  return options;
}

}  // namespace

TEST(SpectreSessionLifecycleTest, CanStartHandshakeAndStopWhenExternalSpectreIsEnabled) {
  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  auto options = spectre_options();
  su::SpectreSession session(0, options, spectre_runtime_root("lifecycle") + "/lifecycle/worker_0");

  ASSERT_NO_THROW(session.start());
  EXPECT_FALSE(session.recent_output().empty());
  EXPECT_NO_THROW(session.stop(true));
}

TEST(SpectreSessionLifecycleTest, CanRunSingleTaskWhenExternalSpectreIsEnabled) {
  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  auto options = spectre_options();
  options.workspace_namespace = "single_run";
  su::SpectreSession session(0, options,
                             spectre_runtime_root("lifecycle") + "/single_run/worker_0");

  ASSERT_NO_THROW(session.start());
  auto result = session.run({}, std::chrono::seconds(30));
  EXPECT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.result_format, su::ResultFormat::kBinPsf);
  EXPECT_EQ(result.work_dir, spectre_runtime_root("lifecycle") + "/single_run/worker_0");
  EXPECT_NO_THROW(session.stop(true));
}

TEST(SpectreSessionLifecycleTest, SpectreEvaluatorRunsBatchWhenExternalSpectreIsEnabled) {
  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  auto options = spectre_options();
  options.workspace_namespace = "batch_run";
  options.num_workers = 2;
  options.timeout_seconds = 30;

  auto evaluator = su::make_spectre_evaluator(options);
  auto results = evaluator.run({su::ParameterState{}, su::ParameterState{}});

  ASSERT_EQ(results.size(), 2U);
  EXPECT_TRUE(results[0].ok()) << results[0].error_message;
  EXPECT_TRUE(results[1].ok()) << results[1].error_message;
  EXPECT_NE(results[0].work_dir, results[1].work_dir);

  evaluator.cleanup();
}
