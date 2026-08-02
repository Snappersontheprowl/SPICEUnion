#include "su/spectre_session.hpp"

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

TEST(SpectreSessionLifecycleTest, RunIsExplicitlyDeferredInM11) {
  auto options = spectre_options();
  su::SpectreSession session(0, options, "/tmp/spiceunion_spectre_deferred/worker_0");

  auto result = session.run({}, std::chrono::seconds(1));

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::TaskStatus::kSimulationFailed);
  EXPECT_NE(result.error_message.find("not implemented"), std::string::npos);
}

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
