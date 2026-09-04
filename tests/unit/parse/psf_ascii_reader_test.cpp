#include "su/result_reader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path ascii_fixture(const char* name) {
  return std::filesystem::path(SPICEUNION_FIXTURE_ROOT) / "psf_ascii" / name;
}

}  // namespace

TEST(PsfAsciiReaderTest, ReadsDcOpScalars) {
  const auto raw = ascii_fixture("bgr_amp_dc_op.raw");

  const auto vref = su::read_dc_value(raw.string(), "V_BGR");
  ASSERT_TRUE(vref.ok()) << vref.error_message;
  EXPECT_NEAR(vref.value.value, 1.182060051516776e0, 1e-12);

  const auto branch = su::read_dc_value(raw.string(), "V7:p");
  ASSERT_TRUE(branch.ok()) << branch.error_message;
  EXPECT_NEAR(branch.value.value, -7.773239008504587e-05, 1e-15);

  EXPECT_FALSE(su::read_dc_value(raw.string(), "missing_signal").ok());
}

TEST(PsfAsciiReaderTest, ReadsDcTempSweep) {
  const auto raw = ascii_fixture("bgr_amp_dc_sweep.raw");

  const auto sweep = su::read_dc_sweep(raw.string(), "temp", "V_BGR");
  ASSERT_TRUE(sweep.ok()) << sweep.error_message;
  ASSERT_EQ(sweep.value.size(), 17U);
  EXPECT_TRUE(sweep.value.shape_consistent());
  EXPECT_NEAR(sweep.value.sweep_values.front(), -40.0, 1e-12);
  EXPECT_NEAR(sweep.value.sweep_values.back(), 120.0, 1e-12);
  EXPECT_NEAR(sweep.value.values.front(), 1.179167826395816e0, 1e-12);
  EXPECT_NEAR(sweep.value.values.back(), 1.178645812598910e0, 1e-12);
}

TEST(PsfAsciiReaderTest, ReadsStbComplexSweep) {
  const auto raw = ascii_fixture("bgr_amp_stb.raw");

  const auto response = su::read_ac_response(raw.string(), "loopGain", "stb.stb");
  ASSERT_TRUE(response.ok()) << response.error_message;
  ASSERT_GT(response.value.size(), 10U);
  EXPECT_TRUE(response.value.shape_consistent());
  EXPECT_NEAR(response.value.frequency_hz.front(), 1.0, 1e-9);
  EXPECT_NEAR(response.value.real.front(), -23.5544, 1e-4);
}

TEST(PsfAsciiReaderTest, ReadsSyntheticTranWaveform) {
  // 当前无真实 ASCII tran 样本，用合成 PSFASCII 验证波形解析路径。
  const auto root = std::filesystem::temp_directory_path() / "spiceunion_psf_ascii_tran";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  {
    std::ofstream output(root / "tran.tran");
    output << "HEADER\n"
              "\"PSFversion\" \"1.00\"\n"
              "TYPE\n"
              "VALUE\n"
              "\"time\" 0.0\n"
              "\"v(out)\" 0.0\n"
              "\"time\" 1.0e-9\n"
              "\"v(out)\" 0.5\n"
              "\"time\" 2.0e-9\n"
              "\"v(out)\" 1.0\n"
              "END\n";
  }

  const auto waveform = su::read_tran_waveform(root.string(), "v(out)", "tran.tran");
  ASSERT_TRUE(waveform.ok()) << waveform.error_message;
  ASSERT_EQ(waveform.value.size(), 3U);
  EXPECT_TRUE(waveform.value.shape_consistent());
  EXPECT_NEAR(waveform.value.time_s[2], 2.0e-9, 1e-15);
  EXPECT_NEAR(waveform.value.value[2], 1.0, 1e-12);

  std::filesystem::remove_all(root);
}

TEST(PsfAsciiReaderTest, DeclaredFormatSkipsContentSniffing) {
  const auto raw = ascii_fixture("bgr_amp_dc_op.raw");

  const auto declared_ascii = su::read_dc_value(raw.string(), "V_BGR", su::ResultFormat::kPsfAscii);
  ASSERT_TRUE(declared_ascii.ok()) << declared_ascii.error_message;
  EXPECT_NEAR(declared_ascii.value.value, 1.182060051516776e0, 1e-12);

  const auto declared_psfxl = su::read_dc_value(raw.string(), "V_BGR", su::ResultFormat::kPsfxl);
  EXPECT_FALSE(declared_psfxl.ok());
  EXPECT_EQ(declared_psfxl.status, su::ResultStatus::kUnsupportedFormat);
}
