#pragma once

#include "su/result.hpp"

#include <string>
#include <vector>

namespace su {

// 用户声明的指标请求：库只负责读取“声明的信号 + 已定义的派生指标”，不定义业务指标。
enum class MetricKind {
  kDcValue,
  kDcSweep,
  kAcResponse,
  kTranWaveform,
};

enum class DerivedMetric {
  kUgbwHz,
  kPhaseMarginDeg,
  kSettlingTimeS,
};

enum class MetricsStatus {
  kOk,
  kSignalNotFound,
  kUnsupportedFormat,
  kReadFailed,
  kInternalError,
};

struct MetricRequest {
  MetricKind kind = MetricKind::kDcValue;
  std::string signal;              // 必填：DC 标量信号 / AC / TRAN 信号
  std::string sweep_signal;        // kDcSweep 专用：扫描轴名（如 "temp" / "vin_dc"）
  std::string filename;            // 可选：空表示按格式约定
  std::vector<DerivedMetric> derived;
  double settling_target = 0.0;    // kSettlingTimeS 用；0 表示取波形末值作为目标
};

struct MetricValue {
  std::string name;                // "V_BGR" / "ugbw_hz" / "settling_time_s" ...
  double value = 0.0;              // SI 单位
  std::string unit;                // "Hz" / "s" / "deg"；未定义时为空
  MetricsStatus status = MetricsStatus::kOk;
  std::string message;
};

struct MetricsOutcome {
  MetricsStatus status = MetricsStatus::kOk;
  std::string message;
  std::vector<MetricValue> values;
};

const char* to_string(MetricKind kind) noexcept;
const char* to_string(DerivedMetric metric) noexcept;
const char* to_string(MetricsStatus status) noexcept;

// 在 job 产物目录中按请求提取指标。必须是“worker 归还池子前”调用。
MetricsOutcome extract_metrics(const std::string& work_dir, ResultFormat declared_format,
                               const std::vector<MetricRequest>& requests);

}  // namespace su
