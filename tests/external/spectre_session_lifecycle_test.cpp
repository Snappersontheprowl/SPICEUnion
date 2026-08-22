#include "su/evaluator.hpp"
#include "su/spectre_session.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <cstdlib>
#include <string>

namespace {

bool file_exists(const char* path) {
  return ::access(path, F_OK) == 0;
}

bool spectre_available() {
  return std::system("command -v spectre >/dev/null 2>&1") == 0;
}

std::string spectre_materials_netlist_path() {
  return std::string(SPICEUNION_SPECTRE_MATERIALS_DIR) +
         "/external/netlist/AMP/dc/input.scs";
}

std::string spectre_materials_pdk_toplevel_path() {
  return std::string(SPICEUNION_SPECTRE_MATERIALS_DIR) +
         "/external/pdk/tsmcN65/toplevel.scs";
}

su::EvaluatorOptions spectre_options() {
  su::EvaluatorOptions options;
  options.netlist_path = spectre_materials_netlist_path();
  options.num_workers = 1;
  options.work_dir_base = "/dev/shm/spiceunion_spectre_lifecycle";
  options.workspace_namespace = "lifecycle";
  options.timeout_seconds = 20;
  return options;
}

bool external_spectre_environment_is_ready(std::string* reason) {
#if !SPICEUNION_ENABLE_EXTERNAL_TESTS
  *reason =
      "External Spectre tests are disabled. Reconfigure with "
      "-DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON to run this test.";
  return false;
#endif
  if (!spectre_available()) {
    *reason = "spectre executable is not available in PATH";
    return false;
  }
  const auto pdk_toplevel = spectre_materials_pdk_toplevel_path();
  if (!file_exists(pdk_toplevel.c_str())) {
    *reason = "required PDK include is missing: " + pdk_toplevel;
    return false;
  }
  const auto netlist = spectre_materials_netlist_path();
  if (!file_exists(netlist.c_str())) {
    *reason = "baseline Spectre netlist is missing: " + netlist;
    return false;
  }
  return true;
}

}  // namespace

TEST(SpectreSessionLifecycleTest, CanStartHandshakeAndStopWhenExternalSpectreIsEnabled) {
  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  auto options = spectre_options();
  su::SpectreSession session(0, options,
                             "/dev/shm/spiceunion_spectre_lifecycle/lifecycle/worker_0");

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
                             "/dev/shm/spiceunion_spectre_lifecycle/single_run/worker_0");

  ASSERT_NO_THROW(session.start());
  auto result = session.run({}, std::chrono::seconds(30));
  EXPECT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.work_dir, "/dev/shm/spiceunion_spectre_lifecycle/single_run/worker_0");
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
