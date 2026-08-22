#include "su/evaluator.hpp"
#include "su/result_reader.hpp"
#include "su/spectre_session.hpp"

#include "spectre_external_env.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

su::EvaluatorOptions spectre_real_result_options() {
  su::EvaluatorOptions options;
  options.netlist_path = spectre_materials_netlist_path();
  options.num_workers = 1;
  options.work_dir_base = "/dev/shm/spiceunion_spectre_real_result";
  options.workspace_namespace = "real_result";
  options.timeout_seconds = 30;
  return options;
}

}  // namespace

TEST(SpectreRealResultTest, RunsRealAmpDcNetlistAndParsesRealDcResult) {
  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto options = spectre_real_result_options();
  su::SpectreSession session(
      0, options, "/dev/shm/spiceunion_spectre_real_result/real_result/worker_0");

  ASSERT_NO_THROW(session.start());
  const auto result = session.run({}, std::chrono::seconds(30));
  ASSERT_TRUE(result.ok()) << result.error_message;

  const auto directory = su::find_result_directory(result.work_dir);
  ASSERT_TRUE(directory.ok()) << directory.error_message;

  const auto v_net7 = su::read_dc_value(directory.value.path, "net7");
  const auto v_net2 = su::read_dc_value(directory.value.path, "net2");
  const auto i_v0 = su::read_dc_value(directory.value.path, "V0:p");
  ASSERT_TRUE(v_net7.ok()) << v_net7.error_message;
  ASSERT_TRUE(v_net2.ok()) << v_net2.error_message;
  ASSERT_TRUE(i_v0.ok()) << i_v0.error_message;

  const double vos = v_net7.value.value - v_net2.value.value;
  const double idd = -i_v0.value.value;

  // Calibrated from spectre_materials external/profiles/amp_dc:
  // Vos = V(net7) - V(net2), Idd = -I(V0:p).
  // Real run measured Vos ~= -22.5 uV, Idd ~= 69.66 uA.
  EXPECT_NEAR(vos, -22.5e-6, 2.0e-6);
  EXPECT_NEAR(idd, 69.66e-6, 2.0e-6);

  EXPECT_NO_THROW(session.stop(true));
}
