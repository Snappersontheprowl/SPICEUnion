#include "su/result_reader.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>

namespace {

class TemporaryDirectory {
 public:
  TemporaryDirectory() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("spiceunion_result_reader_test_" + std::to_string(now));
    std::filesystem::create_directories(path_);
  }

  ~TemporaryDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path_, error);
  }

  const std::filesystem::path& path() const {
    return path_;
  }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST(ResultDirectoryTest, FindsLexicographicallyFirstRawDirectory) {
  TemporaryDirectory temp;
  std::filesystem::create_directories(temp.path() / "z.raw");
  std::filesystem::create_directories(temp.path() / "a.raw");
  std::filesystem::create_directories(temp.path() / "not_raw");

  const auto result = su::find_result_directory(temp.path().string());

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_TRUE(result.value.raw_directory_found);
  EXPECT_EQ(std::filesystem::path(result.value.path).filename(), "a.raw");
}

TEST(ResultDirectoryTest, FallsBackToWorkDirWhenRawDirectoryIsAbsent) {
  TemporaryDirectory temp;
  std::filesystem::create_directories(temp.path() / "spectre.out");

  const auto result = su::find_result_directory(temp.path().string());

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_FALSE(result.value.raw_directory_found);
  EXPECT_EQ(std::filesystem::path(result.value.path), temp.path());
}

TEST(ResultDirectoryTest, ReportsMissingWorkDir) {
  TemporaryDirectory temp;
  const auto missing = temp.path() / "missing";

  const auto result = su::find_result_directory(missing.string());

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kDirectoryNotFound);
}

TEST(ResultDirectoryTest, RejectsEmptyWorkDir) {
  const auto result = su::find_result_directory("");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(AcMathTest, DerivesMagnitudeDbAndPhaseDegFromComplexResponse) {
  su::AcResponse response;
  response.signal = "vout";
  response.frequency_hz = {1.0, 10.0};
  response.real = {1.0, 0.0};
  response.imag = {0.0, 1.0};

  const auto result = su::derive_ac_view(response);

  ASSERT_TRUE(result.ok()) << result.error_message;
  ASSERT_EQ(result.value.size(), 2U);
  EXPECT_DOUBLE_EQ(result.value.frequency_hz[0], 1.0);
  EXPECT_NEAR(result.value.magnitude_db[0], 0.0, 1.0e-12);
  EXPECT_NEAR(result.value.phase_deg[0], 0.0, 1.0e-12);
  EXPECT_NEAR(result.value.magnitude_db[1], 0.0, 1.0e-12);
  EXPECT_NEAR(result.value.phase_deg[1], 90.0, 1.0e-12);
}

TEST(AcMathTest, RejectsInconsistentAcResponseShape) {
  su::AcResponse response;
  response.frequency_hz = {1.0, 10.0};
  response.real = {1.0};
  response.imag = {0.0, 0.0};

  const auto result = su::derive_ac_view(response);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(AcMathTest, RejectsZeroMagnitudeWhenDerivingDbView) {
  su::AcResponse response;
  response.frequency_hz = {1.0};
  response.real = {0.0};
  response.imag = {0.0};

  const auto result = su::derive_ac_view(response);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(AcMathTest, CalculatesUgbwAndPhaseMarginWithInterpolation) {
  su::AcDerivedView view;
  view.frequency_hz = {1.0e6, 2.0e6};
  view.magnitude_db = {6.0, -6.0};
  view.phase_deg = {-90.0, -150.0};

  const auto result = su::calculate_ugbw_and_phase_margin(view);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_NEAR(result.value.unity_gain_bandwidth_hz, 1.5e6, 1.0e-6);
  EXPECT_NEAR(result.value.phase_margin_deg, 60.0, 1.0e-12);
}

TEST(AcMathTest, UsesLastFrequencyWhenNoUnityGainCrossingExists) {
  su::AcDerivedView view;
  view.frequency_hz = {1.0, 10.0, 100.0};
  view.magnitude_db = {20.0, 10.0, 1.0};
  view.phase_deg = {-10.0, -20.0, -30.0};

  const auto result = su::calculate_ugbw_and_phase_margin(view);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_DOUBLE_EQ(result.value.unity_gain_bandwidth_hz, 100.0);
  EXPECT_DOUBLE_EQ(result.value.phase_margin_deg, 0.0);
}

TEST(AcMathTest, RejectsEmptyDerivedViewForUgbw) {
  const auto result = su::calculate_ugbw_and_phase_margin({});

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(TranMathTest, CalculatesSettlingTimeAfterLastOutOfBandSample) {
  su::TranWaveform waveform;
  waveform.signal = "vout";
  waveform.time_s = {0.0, 1.0, 2.0, 3.0};
  waveform.value = {0.0, 0.95, 1.02, 1.0};

  const auto result = su::calculate_settling_time(waveform, 1.0, 0.03);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_DOUBLE_EQ(result.value.settling_time_s, 2.0);
}

TEST(TranMathTest, ReturnsZeroWhenWaveformAlwaysWithinBand) {
  su::TranWaveform waveform;
  waveform.time_s = {0.0, 1.0, 2.0};
  waveform.value = {0.99, 1.0, 1.01};

  const auto result = su::calculate_settling_time(waveform, 1.0, 0.02);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_DOUBLE_EQ(result.value.settling_time_s, 0.0);
}

TEST(TranMathTest, ReturnsLastTimeWhenWaveformNeverSettles) {
  su::TranWaveform waveform;
  waveform.time_s = {0.0, 1.0, 2.0};
  waveform.value = {0.0, 0.5, 0.8};

  const auto result = su::calculate_settling_time(waveform, 1.0, 0.01);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_DOUBLE_EQ(result.value.settling_time_s, 2.0);
}

TEST(TranMathTest, HandlesZeroTargetWithExactBand) {
  su::TranWaveform waveform;
  waveform.time_s = {0.0, 1.0, 2.0};
  waveform.value = {0.1, 0.0, 0.0};

  const auto result = su::calculate_settling_time(waveform, 0.0, 0.01);

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_DOUBLE_EQ(result.value.settling_time_s, 1.0);
}

TEST(TranMathTest, RejectsInvalidWaveformShape) {
  su::TranWaveform waveform;
  waveform.time_s = {0.0, 1.0};
  waveform.value = {0.0};

  const auto result = su::calculate_settling_time(waveform, 1.0);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(ResultReaderBackendStatusTest, FileReadersReportCurrentBackendStatus) {
#if SPICEUNION_ENABLE_LIBPSF_READER
  EXPECT_EQ(su::read_dc_value("result.raw", "vout").status, su::ResultStatus::kFileNotFound);
#else
  EXPECT_EQ(su::read_dc_value("result.raw", "vout").status, su::ResultStatus::kUnsupportedFormat);
#endif
  EXPECT_EQ(su::read_ac_response("result.raw", "vout").status,
            su::ResultStatus::kUnsupportedFormat);
  EXPECT_EQ(su::read_tran_waveform("result.raw", "vout").status,
            su::ResultStatus::kUnsupportedFormat);
  EXPECT_EQ(su::read_sensitivity_legacy("work").status, su::ResultStatus::kUnsupportedFormat);
}

#if SPICEUNION_ENABLE_LIBPSF_READER

TEST(LibpsfDcReaderTest, ReadsMinimalProjectFixtureDcOpScalar) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" / "dc_op_minimal.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "dcOp.dc")) << fixture;

  const auto result = su::read_dc_value(fixture.string(), "vout");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "vout");
  EXPECT_DOUBLE_EQ(result.value.value, 2.5);
}

TEST(LibpsfDcReaderTest, ReportsMissingSignal) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" / "dc_op_minimal.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "dcOp.dc")) << fixture;

  const auto result = su::read_dc_value(fixture.string(), "missing_signal");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kSignalNotFound);
}

TEST(LibpsfDcReaderTest, ReadsSpectreClProjectFixtureDcOpScalar) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" / "spectre_materials_dc_op.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "dcOp.dc")) << fixture;

  const auto result = su::read_dc_value(fixture.string(), "net6");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "net6");
  EXPECT_DOUBLE_EQ(result.value.value, 0.8);
}

#endif
