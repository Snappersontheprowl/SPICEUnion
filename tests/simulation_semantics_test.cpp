#include "su/result_reader.hpp"

#include "support/rc_semantics.hpp"

#include <gtest/gtest.h>

#include <filesystem>

TEST(RcAcSemanticContractTest, SpectreRcLowpassAcFixtureUsesCommonAcResponseSemantics) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" / "spectre_rc_lowpass_ac.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "ac.ac")) << fixture;

#if SPICEUNION_ENABLE_LIBPSF_READER
  const auto result = su::read_ac_response(fixture.string(), "out");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "out");
  spiceunion_test::expect_rc_lowpass_ac_semantics(result.value, 1000.0, 1.0e-12);
#else
  const auto result = su::read_ac_response(fixture.string(), "out");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kUnsupportedFormat);
#endif
}

TEST(RcTranSemanticContractTest, SpectreRcChargingTranFixtureUsesCommonTranWaveformSemantics) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" / "spectre_rc_charging_tran.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "tran.tran.tran")) << fixture;

#if SPICEUNION_ENABLE_LIBPSF_READER
  const auto result = su::read_tran_waveform(fixture.string(), "out", "tran.tran.tran");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.signal, "out");
  spiceunion_test::expect_rc_charging_tran_semantics(result.value, 1000.0, 1.0e-12, 1.0);
#else
  const auto result = su::read_tran_waveform(fixture.string(), "out", "tran.tran.tran");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kUnsupportedFormat);
#endif
}

TEST(ResistorDividerDcSemanticContractTest, SpectreFixtureUsesCommonDcSweepSemantics) {
  const std::filesystem::path fixture =
      std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf" /
      "spectre_resistor_divider_dc.raw";

  ASSERT_TRUE(std::filesystem::is_regular_file(fixture / "dc.dc")) << fixture;

#if SPICEUNION_ENABLE_LIBPSF_READER
  const auto result = su::read_dc_sweep(fixture.string(), "vin_dc", "out");

  ASSERT_TRUE(result.ok()) << result.error_message;
  EXPECT_EQ(result.value.sweep_name, "vin_dc");
  EXPECT_EQ(result.value.signal, "out");
  spiceunion_test::expect_resistor_divider_dc_semantics(result.value, 3000.0, 1000.0);
#else
  const auto result = su::read_dc_sweep(fixture.string(), "vin_dc", "out");

  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status, su::ResultStatus::kUnsupportedFormat);
#endif
}
