#include "su/ngspice_session.hpp"
#include "su/result_reader.hpp"

#include "support/ngspice_external_env.hpp"
#include "support/rc_semantics.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <chrono>
#include <filesystem>
#include <string>

namespace {

std::filesystem::path test_root(const std::string& name) {
  return std::filesystem::path(SPICEUNION_PROJECT_ROOT) / "local/runtime" /
         ("ngspice_" + name + "_" + std::to_string(::getpid()));
}

}  // namespace

TEST(NgspiceSessionExternalTest, RunsRcLowPassAcWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_session");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.work_dir_base = root.string();
  options.workspace_namespace = "single_worker";
  options.timeout_seconds = 20;

  su::NgspiceSession session(0, options, (root / "single_worker" / "worker_0").string());
  ASSERT_NO_THROW(session.start());
  const auto result = session.run(
      {{"resistance_ohm", 1000.0}, {"capacitance_f", 1.0e-12}, {"points_per_decade", 100.0}},
      std::chrono::seconds(20));

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.work_dir, (root / "single_worker" / "worker_0").string());
  EXPECT_EQ(result.result_format, su::ResultFormat::kNspiceWrdata);
  EXPECT_NE(result.detail.find("samples="), std::string::npos);

  const auto ac = su::read_ngspice_wrdata_ac_response(
      (root / "single_worker" / "worker_0" / "rc_ac.out").string(), "v(out)");
  ASSERT_TRUE(ac.ok()) << ac.error_message;
  spiceunion_test::expect_rc_lowpass_ac_semantics(ac.value, 1000.0, 1.0e-12);

  session.stop(true);
  std::filesystem::remove_all(root);
}

TEST(NgspiceSessionExternalTest, RunsRcChargingTranWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_tran_session");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.work_dir_base = root.string();
  options.workspace_namespace = "tran_single_worker";
  options.timeout_seconds = 20;

  su::NgspiceSession session(0, options, (root / "tran_single_worker" / "worker_0").string(),
                             su::NgspiceBuiltinTask::kRcTran);
  ASSERT_NO_THROW(session.start());
  EXPECT_EQ(session.task(), su::NgspiceBuiltinTask::kRcTran);

  const auto result = session.run({{"resistance_ohm", 1000.0},
                                   {"capacitance_f", 1.0e-12},
                                   {"input_voltage_v", 1.0},
                                   {"tran_step_s", 1.0e-11},
                                   {"tran_stop_s", 1.0e-8}},
                                  std::chrono::seconds(20));

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_NE(result.detail.find("ngspice_tran_output="), std::string::npos);
  EXPECT_NE(result.detail.find("samples="), std::string::npos);

  const auto waveform = su::read_ngspice_wrdata_tran_waveform(
      (root / "tran_single_worker" / "worker_0" / "rc_tran.out").string(), "v(out)");
  ASSERT_TRUE(waveform.ok()) << waveform.error_message;
  spiceunion_test::expect_rc_charging_tran_semantics(waveform.value, 1000.0, 1.0e-12, 1.0);

  session.stop(true);
  std::filesystem::remove_all(root);
}

TEST(NgspiceSessionExternalTest, RunsResistorDividerDcWhenExternalNgspiceIsEnabled) {
  std::string skip_reason;
  if (!spiceunion_test::external_ngspice_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }

  const auto root = test_root("external_dc_session");
  std::filesystem::remove_all(root);

  su::EvaluatorOptions options;
  options.num_workers = 1;
  options.work_dir_base = root.string();
  options.workspace_namespace = "dc_single_worker";
  options.timeout_seconds = 20;

  su::NgspiceSession session(0, options, (root / "dc_single_worker" / "worker_0").string(),
                             su::NgspiceBuiltinTask::kResistorDividerDc);
  ASSERT_NO_THROW(session.start());
  EXPECT_EQ(session.task(), su::NgspiceBuiltinTask::kResistorDividerDc);

  const auto result = session.run({{"top_resistance_ohm", 3000.0},
                                   {"bottom_resistance_ohm", 1000.0},
                                   {"dc_start_v", 0.0},
                                   {"dc_stop_v", 1.0},
                                   {"dc_step_v", 0.1}},
                                  std::chrono::seconds(20));

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_NE(result.detail.find("ngspice_dc_output="), std::string::npos);
  EXPECT_NE(result.detail.find("samples="), std::string::npos);

  const auto sweep = su::read_ngspice_wrdata_dc_sweep(
      (root / "dc_single_worker" / "worker_0" / "resistor_divider_dc.out").string(), "Vin",
      "v(out)");
  ASSERT_TRUE(sweep.ok()) << sweep.error_message;
  ASSERT_GT(sweep.value.size(), 5U);
  ASSERT_TRUE(sweep.value.shape_consistent());
  spiceunion_test::expect_resistor_divider_dc_semantics(sweep.value, 3000.0, 1000.0);

  session.stop(true);
  std::filesystem::remove_all(root);
}
