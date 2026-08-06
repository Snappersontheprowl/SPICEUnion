#include "su/result.hpp"
#include "su/result_reader.hpp"

#include <gtest/gtest.h>

#include <string>
#include <type_traits>

TEST(ResultStatusTest, ConvertsStatusToStableText) {
  EXPECT_STREQ(su::to_string(su::ResultStatus::kOk), "ok");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kDirectoryNotFound), "directory_not_found");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kFileNotFound), "file_not_found");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kSignalNotFound), "signal_not_found");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kUnsupportedFormat), "unsupported_format");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kParseError), "parse_error");
  EXPECT_STREQ(su::to_string(su::ResultStatus::kInvalidInput), "invalid_input");
}

TEST(ReadResultTest, SuccessCanCarryLegitimateZeroValue) {
  auto result = su::ReadResult<su::ScalarResult>::success({"vout", 0.0});

  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(static_cast<bool>(result));
  EXPECT_EQ(result.status, su::ResultStatus::kOk);
  EXPECT_EQ(result.value.signal, "vout");
  EXPECT_DOUBLE_EQ(result.value.value, 0.0);
  EXPECT_TRUE(result.error_message.empty());
}

TEST(ReadResultTest, FailureIsDistinctFromZeroValue) {
  auto result =
      su::ReadResult<su::ScalarResult>::failure(su::ResultStatus::kSignalNotFound, "missing vout");

  EXPECT_FALSE(result.ok());
  EXPECT_FALSE(static_cast<bool>(result));
  EXPECT_EQ(result.status, su::ResultStatus::kSignalNotFound);
  EXPECT_EQ(result.value.value, 0.0);
  EXPECT_EQ(result.error_message, "missing vout");
}

TEST(ReadResultTest, FailureFactoryDoesNotCreateOkResult) {
  auto result = su::ReadResult<su::ScalarResult>::failure(su::ResultStatus::kOk);

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kInvalidInput);
}

TEST(ResultIrTest, AcResponseReportsShapeConsistency) {
  su::AcResponse response;
  response.signal = "vout";
  response.frequency_hz = {1.0, 10.0, 100.0};
  response.real = {2.0, 1.0, 0.5};
  response.imag = {0.0, -0.1, -0.2};

  EXPECT_EQ(response.size(), 3U);
  EXPECT_TRUE(response.shape_consistent());

  response.imag.pop_back();
  EXPECT_FALSE(response.shape_consistent());
}

TEST(ResultIrTest, DcSweepReportsShapeConsistency) {
  su::DcSweep sweep;
  sweep.sweep_name = "Vin";
  sweep.signal = "vout";
  sweep.sweep_values = {0.0, 0.5, 1.0};
  sweep.values = {0.0, 0.25, 0.5};

  EXPECT_EQ(sweep.size(), 3U);
  EXPECT_TRUE(sweep.shape_consistent());

  sweep.values.pop_back();
  EXPECT_FALSE(sweep.shape_consistent());
}

TEST(ResultIrTest, AcDerivedViewReportsShapeConsistency) {
  su::AcDerivedView view;
  view.frequency_hz = {1.0, 10.0};
  view.magnitude_db = {20.0, 0.0};
  view.phase_deg = {-45.0, -120.0};

  EXPECT_EQ(view.size(), 2U);
  EXPECT_TRUE(view.shape_consistent());

  view.phase_deg.push_back(-180.0);
  EXPECT_FALSE(view.shape_consistent());
}

TEST(ResultIrTest, TranWaveformReportsShapeConsistency) {
  su::TranWaveform waveform;
  waveform.signal = "vout";
  waveform.time_s = {0.0, 1.0e-9, 2.0e-9};
  waveform.value = {0.0, 0.5, 1.0};

  EXPECT_EQ(waveform.size(), 3U);
  EXPECT_TRUE(waveform.shape_consistent());

  waveform.value.clear();
  EXPECT_FALSE(waveform.shape_consistent());
}

TEST(ResultIrTest, SensitivityEntryStoresRawSimulationMeaningOnly) {
  su::SensitivityEntry entry;
  entry.key = "net1:I5.M3:w";
  entry.node = "net1";
  entry.parameter = "I5.M3:w";
  entry.sensitivity = 1.25;
  entry.parameter_value = 2.0e-6;

  EXPECT_EQ(entry.key, "net1:I5.M3:w");
  EXPECT_EQ(entry.node, "net1");
  EXPECT_EQ(entry.parameter, "I5.M3:w");
  EXPECT_DOUBLE_EQ(entry.sensitivity, 1.25);
  EXPECT_DOUBLE_EQ(entry.parameter_value, 2.0e-6);
}

TEST(ResultReaderApiTest, PublicHeaderDoesNotRequireReaderImplementationAtCompileTime) {
  static_assert(std::is_same<decltype(su::find_result_directory(std::string{})),
                             su::ReadResult<su::ResultDirectory>>::value,
                "find_result_directory must return ResultDirectory ReadResult");
  static_assert(std::is_same<decltype(su::derive_ac_view(su::AcResponse{})),
                             su::ReadResult<su::AcDerivedView>>::value,
                "derive_ac_view must return AcDerivedView ReadResult");
}
