#include "su/spectre_session.hpp"
#include "su/evaluator.hpp"

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <unistd.h>

namespace {

bool file_exists(const char* path) {
  return ::access(path, F_OK) == 0;
}

bool spectre_available() {
  return std::system("command -v spectre >/dev/null 2>&1") == 0;
}

su::EvaluatorOptions spectre_options() {
  su::EvaluatorOptions options;
  options.netlist_path = "~/my_lab/projects/spectre_materials/netlist/AMP/dc/input.scs";
  options.num_workers = 1;
  options.work_dir_base = "/dev/shm/spiceunion_spectre_lifecycle";
  options.workspace_namespace = "lifecycle";
  options.timeout_seconds = 20;
  return options;
}

void skip_unless_external_environment_is_ready() {
#if !SPICEUNION_ENABLE_EXTERNAL_TESTS
  GTEST_SKIP() << "External Spectre tests are disabled. Reconfigure with "
                  "-DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON to run this test.";
#endif
  if (!spectre_available()) {
    GTEST_SKIP() << "spectre executable is not available in PATH";
  }
  if (!file_exists("/dev/shm/pdk_cache/toplevel.scs")) {
    GTEST_SKIP() << "required PDK include is missing: /dev/shm/pdk_cache/toplevel.scs";
  }
  if (!file_exists("~/my_lab/projects/spectre_materials/netlist/AMP/dc/input.scs")) {
    GTEST_SKIP() << "baseline Spectre netlist is missing";
  }
}

}  // namespace

TEST(SpectreSessionLifecycleTest, CanStartHandshakeAndStopWhenExternalSpectreIsEnabled) {
  skip_unless_external_environment_is_ready();

  auto options = spectre_options();
  su::SpectreSession session(
      0,
      options,
      "/dev/shm/spiceunion_spectre_lifecycle/lifecycle/worker_0");

  ASSERT_NO_THROW(session.start());
  EXPECT_FALSE(session.recent_output().empty());
  EXPECT_NO_THROW(session.stop(true));
}

TEST(SpectreSessionLifecycleTest, CanRunSingleTaskWhenExternalSpectreIsEnabled) {
  skip_unless_external_environment_is_ready();

  auto options = spectre_options();
  options.workspace_namespace = "single_run";
  su::SpectreSession session(
      0,
      options,
      "/dev/shm/spiceunion_spectre_lifecycle/single_run/worker_0");

  ASSERT_NO_THROW(session.start());
  auto result = session.run({}, std::chrono::seconds(30));
  EXPECT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.work_dir, "/dev/shm/spiceunion_spectre_lifecycle/single_run/worker_0");
  EXPECT_NO_THROW(session.stop(true));
}

TEST(SpectreSessionLifecycleTest, SpectreEvaluatorRunsBatchWhenExternalSpectreIsEnabled) {
  skip_unless_external_environment_is_ready();

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
