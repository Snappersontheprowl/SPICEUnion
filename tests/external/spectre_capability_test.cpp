#include "su/evaluator.hpp"
#include "su/result_reader.hpp"
#include "su/spectre_session.hpp"

#include "spectre_external_env.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <string>

namespace {

enum class ReaderKind { kDcValue, kAcResponse, kTranWaveform };

struct CapabilityCase {
  const char* id;
  const char* netlist_dir;
  ReaderKind reader;
  const char* signal;
  const char* filename;  // nullptr 时使用 reader 默认文件名
  su::ResultStatus expected_status;
};

const CapabilityCase kCapabilityCases[] = {
    // 支持路径：BINPSF 输出
    {"amp_dc", "AMP/dc", ReaderKind::kDcValue, "net7", nullptr, su::ResultStatus::kOk},
    {"amp_ac", "AMP/ac", ReaderKind::kAcResponse, "net1", nullptr, su::ResultStatus::kOk},
    // 已知边界：真实仿真可跑，解析命中当前 backend 限制
    {"amp_tran_psfxl", "AMP/tran", ReaderKind::kTranWaveform, "net6", "tran.tran.tran",
     su::ResultStatus::kUnsupportedFormat},
    {"bgr_amp_dc_psfascii", "BGR_AMP/dc", ReaderKind::kDcValue, "V_BGR", nullptr,
     su::ResultStatus::kParseError},
    {"bgr_amp_stb_psfascii", "BGR_AMP/stb", ReaderKind::kAcResponse, "loopGain", "stb.stb",
     su::ResultStatus::kParseError},
};

void PrintTo(const CapabilityCase& test_case, std::ostream* os) {
  *os << test_case.id;
}

su::EvaluatorOptions capability_options(const CapabilityCase& test_case) {
  su::EvaluatorOptions options;
  options.netlist_path = spectre_materials_netlist_path(test_case.netlist_dir);
  options.num_workers = 1;
  options.work_dir_base = spectre_runtime_root("capability");
  options.workspace_namespace = test_case.id;
  options.timeout_seconds = 90;
  return options;
}

}  // namespace

class SpectreCapabilityTest : public ::testing::TestWithParam<CapabilityCase> {};

INSTANTIATE_TEST_SUITE_P(NetlistMatrix, SpectreCapabilityTest,
                         ::testing::ValuesIn(kCapabilityCases));

TEST_P(SpectreCapabilityTest, RunsRealNetlistAndParsesWithDocumentedStatus) {
  const auto& test_case = GetParam();

  std::string skip_reason;
  if (!external_spectre_environment_is_ready(&skip_reason)) {
    GTEST_SKIP() << skip_reason;
  }
  const auto netlist = spectre_materials_netlist_path(test_case.netlist_dir);
  if (!spectre_file_exists(netlist.c_str())) {
    GTEST_SKIP() << "netlist missing: " << netlist;
  }

  const auto options = capability_options(test_case);
  const auto work_dir =
      spectre_runtime_root("capability") + "/" + test_case.id + "/worker_0";
  su::SpectreSession session(0, options, work_dir);

  // 执行能力：真实仿真必须跑通并交付结果目录。
  ASSERT_NO_THROW(session.start());
  const auto result = session.run({}, std::chrono::seconds(90));
  ASSERT_TRUE(result.ok()) << result.error_message;

  const auto directory = su::find_result_directory(result.work_dir);
  ASSERT_TRUE(directory.ok()) << directory.error_message;

  su::ResultStatus status = su::ResultStatus::kInvalidInput;
  switch (test_case.reader) {
    case ReaderKind::kDcValue: {
      const auto parsed = su::read_dc_value(directory.value.path, test_case.signal);
      status = parsed.status;
      if (parsed.ok()) {
        EXPECT_TRUE(std::isfinite(parsed.value.value));
      }
      break;
    }
    case ReaderKind::kAcResponse: {
      const auto filename = test_case.filename != nullptr ? test_case.filename : "ac.ac";
      const auto parsed = su::read_ac_response(directory.value.path, test_case.signal, filename);
      status = parsed.status;
      if (parsed.ok()) {
        EXPECT_GT(parsed.value.size(), 0U);
        EXPECT_TRUE(parsed.value.shape_consistent());
      }
      break;
    }
    case ReaderKind::kTranWaveform: {
      const auto filename = test_case.filename != nullptr ? test_case.filename : "tran.tran";
      const auto parsed = su::read_tran_waveform(directory.value.path, test_case.signal, filename);
      status = parsed.status;
      if (parsed.ok()) {
        EXPECT_GT(parsed.value.size(), 0U);
        EXPECT_TRUE(parsed.value.shape_consistent());
      }
      break;
    }
  }

  // 解析能力：当前实现必须与矩阵记录一致；backend 改进后此处会翻红，提示更新矩阵。
  EXPECT_EQ(status, test_case.expected_status);

  EXPECT_NO_THROW(session.stop(true));
}
