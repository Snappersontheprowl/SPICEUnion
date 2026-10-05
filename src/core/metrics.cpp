#include "su/metrics.hpp"

#include "su/ngspice_session.hpp"
#include "su/result_reader.hpp"

#include <utility>

namespace su {
namespace {

std::string join_path_local(const std::string& directory, const std::string& filename) {
  if (directory.empty()) {
    return filename;
  }
  if (directory.back() == '/') {
    return directory + filename;
  }
  return directory + "/" + filename;
}

MetricsStatus map_read_status(ResultStatus status) {
  switch (status) {
    case ResultStatus::kOk:
      return MetricsStatus::kOk;
    case ResultStatus::kSignalNotFound:
      return MetricsStatus::kSignalNotFound;
    case ResultStatus::kUnsupportedFormat:
      return MetricsStatus::kUnsupportedFormat;
    case ResultStatus::kDirectoryNotFound:
    case ResultStatus::kFileNotFound:
    case ResultStatus::kParseError:
    case ResultStatus::kInvalidInput:
    case ResultStatus::kArtifactsNotRetained:
      return MetricsStatus::kReadFailed;
  }
  return MetricsStatus::kReadFailed;
}

MetricValue failed_metric(const std::string& name, MetricsStatus status,
                          const std::string& message) {
  MetricValue value;
  value.name = name;
  value.status = status;
  value.message = message;
  return value;
}

void merge_status(MetricsOutcome& outcome, MetricsStatus status, const std::string& message) {
  if (status != MetricsStatus::kOk && outcome.status == MetricsStatus::kOk) {
    outcome.status = status;
    outcome.message = message;
  }
}

// Ngspice 内置任务的 wrdata 输出文件名约定（用户未显式指定 filename 时使用）。
std::string default_filename(MetricKind kind, ResultFormat format, const char* spectre_default) {
  if (format == ResultFormat::kNspiceWrdata) {
    switch (kind) {
      case MetricKind::kDcSweep:
        return "resistor_divider_dc.out";
      case MetricKind::kAcResponse:
        return "rc_ac.out";
      case MetricKind::kTranWaveform:
        return "rc_tran.out";
      case MetricKind::kDcValue:
        break;
    }
  }
  return spectre_default;
}

}  // namespace

const char* to_string(MetricKind kind) noexcept {
  switch (kind) {
    case MetricKind::kDcValue:
      return "dc";
    case MetricKind::kDcSweep:
      return "dc_sweep";
    case MetricKind::kAcResponse:
      return "ac";
    case MetricKind::kTranWaveform:
      return "tran";
  }
  return "unknown";
}

const char* to_string(DerivedMetric metric) noexcept {
  switch (metric) {
    case DerivedMetric::kUgbwHz:
      return "ugbw";
    case DerivedMetric::kPhaseMarginDeg:
      return "phase_margin";
    case DerivedMetric::kSettlingTimeS:
      return "settling_time";
  }
  return "unknown";
}

const char* to_string(MetricsStatus status) noexcept {
  switch (status) {
    case MetricsStatus::kOk:
      return "ok";
    case MetricsStatus::kSignalNotFound:
      return "signal_not_found";
    case MetricsStatus::kUnsupportedFormat:
      return "unsupported_format";
    case MetricsStatus::kReadFailed:
      return "read_failed";
    case MetricsStatus::kInternalError:
      return "internal_error";
  }
  return "unknown";
}

MetricsOutcome extract_metrics(const std::string& work_dir, ResultFormat declared_format,
                               const std::vector<MetricRequest>& requests) {
  MetricsOutcome outcome;
  if (requests.empty()) {
    return outcome;
  }

  auto directory = find_result_directory(work_dir);
  const std::string result_dir = directory.ok() ? directory.value.path : work_dir;

  for (const auto& request : requests) {
    if (request.signal.empty()) {
      outcome.values.push_back(
          failed_metric("", MetricsStatus::kReadFailed, "metric request requires a signal"));
      merge_status(outcome, MetricsStatus::kReadFailed, "metric request requires a signal");
      continue;
    }

    switch (request.kind) {
      case MetricKind::kDcValue: {
        if (declared_format == ResultFormat::kNspiceWrdata) {
          const std::string message =
              "Ngspice wrdata scalar DC metric is not supported by the extractor";
          outcome.values.push_back(failed_metric(request.signal, MetricsStatus::kReadFailed,
                                                 message));
          merge_status(outcome, MetricsStatus::kReadFailed, message);
          break;
        }
        const auto read = read_dc_value(result_dir, request.signal, declared_format);
        if (!read.ok()) {
          outcome.values.push_back(failed_metric(request.signal, map_read_status(read.status),
                                                 read.error_message));
          merge_status(outcome, map_read_status(read.status), read.error_message);
          break;
        }
        MetricValue value;
        value.name = read.value.signal.empty() ? request.signal : read.value.signal;
        value.value = read.value.value;
        outcome.values.push_back(std::move(value));
        break;
      }
      case MetricKind::kDcSweep: {
        if (request.sweep_signal.empty()) {
          const std::string message = "dc_sweep request requires sweep_signal";
          outcome.values.push_back(failed_metric(request.signal, MetricsStatus::kReadFailed,
                                                 message));
          merge_status(outcome, MetricsStatus::kReadFailed, message);
          break;
        }
        const std::string sweep_file =
            request.filename.empty()
                ? default_filename(request.kind, declared_format, "dc.dc")
                : request.filename;
        const auto read =
            declared_format == ResultFormat::kNspiceWrdata
                ? read_ngspice_wrdata_dc_sweep(join_path_local(result_dir, sweep_file),
                                               request.sweep_signal, request.signal)
                : read_dc_sweep(result_dir, request.sweep_signal, request.signal, sweep_file,
                                declared_format);
        if (!read.ok()) {
          outcome.values.push_back(failed_metric(request.signal, map_read_status(read.status),
                                                 read.error_message));
          merge_status(outcome, map_read_status(read.status), read.error_message);
          break;
        }
        const auto& sweep = read.value;
        MetricValue first;
        first.name = sweep.signal + "_first";
        first.value = sweep.sweep_values.empty() ? 0.0 : sweep.sweep_values.front();
        MetricValue last;
        last.name = sweep.signal + "_last";
        last.value = sweep.sweep_values.empty() ? 0.0 : sweep.sweep_values.back();
        MetricValue points;
        points.name = sweep.signal + "_points";
        points.value = static_cast<double>(sweep.sweep_values.size());
        outcome.values.push_back(std::move(first));
        outcome.values.push_back(std::move(last));
        outcome.values.push_back(std::move(points));
        break;
      }
      case MetricKind::kAcResponse: {
        if (request.derived.empty()) {
          const std::string message = "ac request requires a derived metric";
          outcome.values.push_back(failed_metric(request.signal, MetricsStatus::kReadFailed,
                                                 message));
          merge_status(outcome, MetricsStatus::kReadFailed, message);
          break;
        }
        const std::string ac_file =
            request.filename.empty()
                ? default_filename(request.kind, declared_format, "ac.ac")
                : request.filename;
        const auto read =
            declared_format == ResultFormat::kNspiceWrdata
                ? read_ngspice_wrdata_ac_response(join_path_local(result_dir, ac_file),
                                                  request.signal)
                : read_ac_response(result_dir, request.signal, ac_file, declared_format);
        if (!read.ok()) {
          outcome.values.push_back(failed_metric(request.signal, map_read_status(read.status),
                                                 read.error_message));
          merge_status(outcome, map_read_status(read.status), read.error_message);
          break;
        }
        const auto derived = derive_ac_view(read.value);
        if (!derived.ok()) {
          outcome.values.push_back(failed_metric(request.signal,
                                                 map_read_status(derived.status),
                                                 derived.error_message));
          merge_status(outcome, map_read_status(derived.status), derived.error_message);
          break;
        }
        const auto metrics = calculate_ugbw_and_phase_margin(derived.value);
        if (!metrics.ok()) {
          outcome.values.push_back(failed_metric(request.signal,
                                                 map_read_status(metrics.status),
                                                 metrics.error_message));
          merge_status(outcome, map_read_status(metrics.status), metrics.error_message);
          break;
        }
        for (const auto metric : request.derived) {
          MetricValue value;
          if (metric == DerivedMetric::kUgbwHz) {
            value.name = "ugbw_hz";
            value.value = metrics.value.unity_gain_bandwidth_hz;
            value.unit = "Hz";
            outcome.values.push_back(std::move(value));
          } else if (metric == DerivedMetric::kPhaseMarginDeg) {
            value.name = "phase_margin_deg";
            value.value = metrics.value.phase_margin_deg;
            value.unit = "deg";
            outcome.values.push_back(std::move(value));
          } else {
            const std::string message = "settling_time is not an AC derived metric";
            outcome.values.push_back(
                failed_metric("settling_time_s", MetricsStatus::kReadFailed, message));
            merge_status(outcome, MetricsStatus::kReadFailed, message);
          }
        }
        break;
      }
      case MetricKind::kTranWaveform: {
        const std::string tran_file =
            request.filename.empty()
                ? default_filename(request.kind, declared_format, "tran.tran")
                : request.filename;
        const auto read =
            declared_format == ResultFormat::kNspiceWrdata
                ? read_ngspice_wrdata_tran_waveform(join_path_local(result_dir, tran_file),
                                                    request.signal)
                : read_tran_waveform(result_dir, request.signal, tran_file, declared_format);
        if (!read.ok()) {
          outcome.values.push_back(failed_metric(request.signal, map_read_status(read.status),
                                                 read.error_message));
          merge_status(outcome, map_read_status(read.status), read.error_message);
          break;
        }
        const auto& waveform = read.value;
        for (const auto metric : request.derived) {
          if (metric == DerivedMetric::kSettlingTimeS) {
            const double target = request.settling_target != 0.0
                                      ? request.settling_target
                                      : (waveform.value.empty() ? 0.0 : waveform.value.back());
            const auto settled = calculate_settling_time(waveform, target);
            if (!settled.ok()) {
              outcome.values.push_back(failed_metric("settling_time_s",
                                                     map_read_status(settled.status),
                                                     settled.error_message));
              merge_status(outcome, map_read_status(settled.status), settled.error_message);
              break;
            }
            MetricValue value;
            value.name = "settling_time_s";
            value.value = settled.value.settling_time_s;
            value.unit = "s";
            outcome.values.push_back(std::move(value));
          } else {
            const std::string message = "ugbw/phase_margin are AC derived metrics";
            outcome.values.push_back(
                failed_metric("", MetricsStatus::kReadFailed, message));
            merge_status(outcome, MetricsStatus::kReadFailed, message);
          }
        }
        break;
      }
    }
  }
  return outcome;
}

}  // namespace su
