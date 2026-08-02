#include "su/spectre_protocol.hpp"

#include <gtest/gtest.h>

TEST(SpectreProtocolTest, FormatsParameterUpdateAndRunCommand) {
  su::ParameterState state = {
      {"wp1", 1.25e-6},
      {"wn1", 2.0e-6},
  };

  auto command = su::format_spectre_run_command(state);

  EXPECT_NE(command.find("(progn"), std::string::npos);
  EXPECT_NE(
      command.find("(sclSetAttribute (sclGetParameter top \"wp1\") \"value\" 1.250000e-06)"),
      std::string::npos);
  EXPECT_NE(
      command.find("(sclSetAttribute (sclGetParameter top \"wn1\") \"value\" 2.000000e-06)"),
      std::string::npos);
  EXPECT_NE(command.find("(sclRun \"all\")"), std::string::npos);
  EXPECT_EQ(command.back(), '\n');
}

TEST(SpectreProtocolTest, RecognizesResourceStatsThenTAsSuccess) {
  bool seen = false;

  EXPECT_EQ(
      su::classify_spectre_completion_line("Peak resident memory used = 1 Mbytes", &seen),
      su::SpectreCompletion::kContinue);
  EXPECT_TRUE(seen);
  EXPECT_EQ(
      su::classify_spectre_completion_line("t", &seen),
      su::SpectreCompletion::kSucceeded);
}

TEST(SpectreProtocolTest, RecognizesNilAfterResourceStatsAsFailure) {
  bool seen = true;

  EXPECT_EQ(
      su::classify_spectre_completion_line("nil", &seen),
      su::SpectreCompletion::kFailed);
}

TEST(SpectreProtocolTest, RecognizesClassicCompletesLineAsSuccess) {
  bool seen = false;

  EXPECT_EQ(
      su::classify_spectre_completion_line("spectre completes with 0 errors, 0 warnings", &seen),
      su::SpectreCompletion::kSucceeded);
}
