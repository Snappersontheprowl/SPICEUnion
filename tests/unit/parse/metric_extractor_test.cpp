#include "su/metrics.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

fs::path ascii_fixture(const char* name) {
  return fs::path(SPICEUNION_FIXTURE_ROOT) / "psf_ascii" / name;
}

}  // namespace

TEST(MetricExtractorTest, ReadsDcValueFromPsfAsciiFixture) {
  su::MetricRequest request;
  request.kind = su::MetricKind::kDcValue;
  request.signal = "V_BGR";

  const auto outcome = su::extract_metrics(ascii_fixture("bgr_amp_dc_op.raw").string(),
                                          su::ResultFormat::kPsfAscii, {request});
  ASSERT_EQ(su::MetricsStatus::kOk, outcome.status) << outcome.message;
  ASSERT_EQ(1u, outcome.values.size());
  EXPECT_EQ("V_BGR", outcome.values[0].name);
  EXPECT_NE(0.0, outcome.values[0].value);
}

TEST(MetricExtractorTest, DerivesUgbwAndPhaseMarginFromAcFixture) {
  su::MetricRequest request;
  request.kind = su::MetricKind::kAcResponse;
  request.signal = "loopGain";
  request.filename = "stb.stb";
  request.derived = {su::DerivedMetric::kUgbwHz, su::DerivedMetric::kPhaseMarginDeg};

  const auto outcome = su::extract_metrics(ascii_fixture("bgr_amp_stb.raw").string(),
                                          su::ResultFormat::kPsfAscii, {request});
  ASSERT_EQ(su::MetricsStatus::kOk, outcome.status) << outcome.message;
  ASSERT_EQ(2u, outcome.values.size());
  EXPECT_EQ("ugbw_hz", outcome.values[0].name);
  EXPECT_EQ("Hz", outcome.values[0].unit);
  EXPECT_EQ("phase_margin_deg", outcome.values[1].name);
  EXPECT_EQ("deg", outcome.values[1].unit);
}

TEST(MetricExtractorTest, ReportsSignalNotFound) {
  su::MetricRequest request;
  request.kind = su::MetricKind::kDcValue;
  request.signal = "does_not_exist";

  const auto outcome = su::extract_metrics(ascii_fixture("bgr_amp_dc_op.raw").string(),
                                          su::ResultFormat::kPsfAscii, {request});
  EXPECT_EQ(su::MetricsStatus::kSignalNotFound, outcome.status);
  ASSERT_EQ(1u, outcome.values.size());
  EXPECT_EQ(su::MetricsStatus::kSignalNotFound, outcome.values[0].status);
}

TEST(MetricExtractorTest, EmptyRequestsProduceEmptySuccess) {
  const auto outcome = su::extract_metrics(ascii_fixture("bgr_amp_dc_op.raw").string(),
                                          su::ResultFormat::kPsfAscii, {});
  EXPECT_EQ(su::MetricsStatus::kOk, outcome.status);
  EXPECT_TRUE(outcome.values.empty());
}
