#include "su/ngspice_session.hpp"

#include "su/evaluator.hpp"
#include "su/result_reader.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path test_root(const std::string& name) {
  return std::filesystem::path("/tmp") /
         ("spiceunion_ngspice_" + name + "_" + std::to_string(::getpid()));
}

void write_text(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path);
  ASSERT_TRUE(output) << path;
  output << text;
}

bool ngspice_available() {
  const char* explicit_path = std::getenv("SPICEUNION_NGSPICE");
  if (explicit_path != nullptr && explicit_path[0] != '\0' && ::access(explicit_path, X_OK) == 0) {
    return true;
  }
  return std::system("command -v ngspice >/dev/null 2>&1") == 0 ||
         std::system("command -v ngspice_con >/dev/null 2>&1") == 0;
}

void skip_unless_external_ngspice_is_ready() {
#if !SPICEUNION_ENABLE_EXTERNAL_TESTS
  GTEST_SKIP() << "External Ngspice tests are disabled. Reconfigure with "
                  "-DSPICEUNION_ENABLE_EXTERNAL_TESTS=ON to run this test.";
#endif
  if (!ngspice_available()) {
    GTEST_SKIP() << "ngspice executable is not available in PATH";
  }
}

double find_frequency_closest_to_db(const su::AcDerivedView& view, double target_db) {
  double best_frequency = 0.0;
  double best_error = 1.0e300;
  for (std::size_t index = 0; index < view.size(); ++index) {
    const double error = std::abs(view.magnitude_db[index] - target_db);
    if (error < best_error) {
      best_error = error;
      best_frequency = view.frequency_hz[index];
    }
  }
  return best_frequency;
}

}  // namespace

TEST(NgspiceRcAcConfigTest, UsesDefaultsAndStateOverrides) {
  const auto defaults = su::ngspice_rc_ac_config_from_state({});
  EXPECT_DOUBLE_EQ(defaults.resistance_ohm, 1000.0);
  EXPECT_DOUBLE_EQ(defaults.capacitance_f, 1.0e-12);
  EXPECT_DOUBLE_EQ(defaults.start_hz, 1.0e6);
  EXPECT_DOUBLE_EQ(defaults.stop_hz, 1.0e10);
  EXPECT_EQ(defaults.points_per_decade, 20);

  const su::ParameterState state{
      {"resistance_ohm", 2000.0}, {"capacitance_f", 2.0e-12},  {"ac_start_hz", 10.0},
      {"ac_stop_hz", 1.0e9},      {"points_per_decade", 33.0},
  };

  const auto config = su::ngspice_rc_ac_config_from_state(state);
  EXPECT_DOUBLE_EQ(config.resistance_ohm, 2000.0);
  EXPECT_DOUBLE_EQ(config.capacitance_f, 2.0e-12);
  EXPECT_DOUBLE_EQ(config.start_hz, 10.0);
  EXPECT_DOUBLE_EQ(config.stop_hz, 1.0e9);
  EXPECT_EQ(config.points_per_decade, 33);
}

TEST(NgspiceRcAcNetlistTest, RendersBatchModeLowPassAcNetlist) {
  su::NgspiceRcAcConfig config;
  config.resistance_ohm = 1000.0;
  config.capacitance_f = 1.0e-12;
  config.start_hz = 1.0e6;
  config.stop_hz = 1.0e10;
  config.points_per_decade = 20;

  const auto netlist = su::render_ngspice_rc_ac_netlist(config, "rc_ac.out");

  EXPECT_NE(netlist.find("Vin in 0 dc 0 ac 1"), std::string::npos);
  EXPECT_NE(netlist.find("R1 in out 1.00000000000000000e+03"), std::string::npos);
  EXPECT_NE(netlist.find("C1 out 0 9.99999999999999980e-13"), std::string::npos);
  EXPECT_NE(netlist.find("ac dec 20 1.00000000000000000e+06 1.00000000000000000e+10"),
            std::string::npos);
  EXPECT_NE(netlist.find("wrdata rc_ac.out v(out)"), std::string::npos);
}

TEST(NgspiceWrdataParserTest, ParsesThreeColumnComplexAcOutput) {
  const auto root = test_root("parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_ac.out";
  write_text(data,
             "1.00000000e+06  9.99960523e-01 -6.28293727e-03\n"
             "1.12201845e+06  9.99950302e-01 -7.04949950e-03\n");

  const auto result = su::read_ngspice_wrdata_ac_response(data.string(), "v(out)");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "v(out)");
  ASSERT_EQ(result.value.size(), 2U);
  EXPECT_TRUE(result.value.shape_consistent());
  EXPECT_DOUBLE_EQ(result.value.frequency_hz[0], 1.0e6);
  EXPECT_NEAR(result.value.real[0], 0.999960523, 1.0e-12);
  EXPECT_NEAR(result.value.imag[0], -0.00628293727, 1.0e-14);

  std::filesystem::remove_all(root);
}

TEST(NgspiceWrdataParserTest, ReportsMalformedAcOutput) {
  const auto root = test_root("malformed_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_ac.out";
  write_text(data, "1.0 2.0\n");

  const auto result = su::read_ngspice_wrdata_ac_response(data.string(), "v(out)");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kParseError);

  std::filesystem::remove_all(root);
}

TEST(NgspiceWrdataParserTest, RejectsUnexpectedExtraColumns) {
  const auto root = test_root("extra_columns_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_ac.out";
  write_text(data, "1.0 1.0 0.0 1.0 1.0 0.0\n");

  const auto result = su::read_ngspice_wrdata_ac_response(data.string(), "v(out)");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kParseError);

  std::filesystem::remove_all(root);
}

TEST(NgspiceSessionExternalTest, RunsRcLowPassAcWhenExternalNgspiceIsEnabled) {
  skip_unless_external_ngspice_is_ready();

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
  EXPECT_NE(result.detail.find("samples="), std::string::npos);

  const auto ac = su::read_ngspice_wrdata_ac_response(
      (root / "single_worker" / "worker_0" / "rc_ac.out").string(), "v(out)");
  ASSERT_TRUE(ac.ok()) << ac.error_message;
  const auto view = su::derive_ac_view(ac.value);
  ASSERT_TRUE(view.ok()) << view.error_message;

  const double expected_fc = 1.0 / (2.0 * std::acos(-1.0) * 1000.0 * 1.0e-12);
  const double measured_fc = find_frequency_closest_to_db(view.value, -3.01029995664);
  EXPECT_NEAR(measured_fc, expected_fc, expected_fc * 0.03);

  session.stop(true);
  std::filesystem::remove_all(root);
}

TEST(NgspiceEvaluatorExternalTest, RunsBatchThroughEvaluatorWhenExternalNgspiceIsEnabled) {
  skip_unless_external_ngspice_is_ready();

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
