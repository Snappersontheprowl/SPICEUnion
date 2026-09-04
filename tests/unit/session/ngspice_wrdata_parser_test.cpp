#include "su/ngspice_session.hpp"

#include <gtest/gtest.h>

#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path test_root(const std::string& name) {
  return std::filesystem::path(SPICEUNION_PROJECT_ROOT) / "local/runtime" /
         ("ngspice_" + name + "_" + std::to_string(::getpid()));
}

void write_text(const std::filesystem::path& path, const std::string& text) {
  std::ofstream output(path);
  ASSERT_TRUE(output) << path;
  output << text;
}

}  // namespace

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

TEST(NgspiceWrdataParserTest, ParsesTwoColumnTranOutput) {
  const auto root = test_root("tran_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_tran.out";
  write_text(data,
             "0.00000000e+00  0.00000000e+00\n"
             "1.00000000e-09  6.32120559e-01\n"
             "2.00000000e-09  8.64664717e-01\n");

  const auto result = su::read_ngspice_wrdata_tran_waveform(data.string(), "v(out)");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "v(out)");
  ASSERT_EQ(result.value.size(), 3U);
  EXPECT_TRUE(result.value.shape_consistent());
  EXPECT_DOUBLE_EQ(result.value.time_s[1], 1.0e-9);
  EXPECT_NEAR(result.value.value[1], 0.632120559, 1.0e-12);

  std::filesystem::remove_all(root);
}

TEST(NgspiceWrdataParserTest, ParsesTwoColumnDcSweepOutput) {
  const auto root = test_root("dc_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "resistor_divider_dc.out";
  write_text(data,
             "0.00000000e+00  0.00000000e+00\n"
             "5.00000000e-01  2.50000000e-01\n"
             "1.00000000e+00  5.00000000e-01\n");

  const auto result = su::read_ngspice_wrdata_dc_sweep(data.string(), "Vin", "v(out)");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.sweep_name, "Vin");
  EXPECT_EQ(result.value.signal, "v(out)");
  ASSERT_EQ(result.value.size(), 3U);
  EXPECT_TRUE(result.value.shape_consistent());
  EXPECT_DOUBLE_EQ(result.value.sweep_values[1], 0.5);
  EXPECT_DOUBLE_EQ(result.value.values[1], 0.25);

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

TEST(NgspiceWrdataParserTest, ReportsMalformedTranOutput) {
  const auto root = test_root("malformed_tran_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_tran.out";
  write_text(data, "1.0\n");

  const auto result = su::read_ngspice_wrdata_tran_waveform(data.string(), "v(out)");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kParseError);

  std::filesystem::remove_all(root);
}

TEST(NgspiceWrdataParserTest, ReportsMalformedDcOutput) {
  const auto root = test_root("malformed_dc_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "resistor_divider_dc.out";
  write_text(data, "0.0\n");

  const auto result = su::read_ngspice_wrdata_dc_sweep(data.string(), "Vin", "v(out)");

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

TEST(NgspiceWrdataParserTest, RejectsUnexpectedTranExtraColumns) {
  const auto root = test_root("extra_columns_tran_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "rc_tran.out";
  write_text(data, "1.0 1.0 0.0\n");

  const auto result = su::read_ngspice_wrdata_tran_waveform(data.string(), "v(out)");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kParseError);

  std::filesystem::remove_all(root);
}

TEST(NgspiceWrdataParserTest, RejectsUnexpectedDcExtraColumns) {
  const auto root = test_root("extra_columns_dc_parser");
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  const auto data = root / "resistor_divider_dc.out";
  write_text(data, "1.0 0.5 0.25\n");

  const auto result = su::read_ngspice_wrdata_dc_sweep(data.string(), "Vin", "v(out)");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kParseError);

  std::filesystem::remove_all(root);
}
