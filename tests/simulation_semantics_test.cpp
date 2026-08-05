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
