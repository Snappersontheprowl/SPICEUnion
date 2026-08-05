#pragma once

#include "su/core.hpp"
#include "su/result.hpp"
#include "su/session.hpp"

#include <chrono>
#include <cstddef>
#include <string>

namespace su {

enum class NgspiceBuiltinTask {
  kRcAc,
  kRcTran,
};

struct NgspiceRcAcConfig {
  double resistance_ohm = 1000.0;
  double capacitance_f = 1.0e-12;
  double start_hz = 1.0e6;
  double stop_hz = 1.0e10;
  int points_per_decade = 20;
};

struct NgspiceRcTranConfig {
  double resistance_ohm = 1000.0;
  double capacitance_f = 1.0e-12;
  double input_voltage_v = 1.0;
  double step_s = 1.0e-11;
  double stop_s = 1.0e-8;
};

NgspiceRcAcConfig ngspice_rc_ac_config_from_state(const ParameterState& state);
NgspiceRcTranConfig ngspice_rc_tran_config_from_state(const ParameterState& state);

std::string render_ngspice_rc_ac_netlist(const NgspiceRcAcConfig& config,
                                         const std::string& output_filename = "rc_ac.out");
std::string render_ngspice_rc_tran_netlist(const NgspiceRcTranConfig& config,
                                           const std::string& output_filename = "rc_tran.out");

ReadResult<AcResponse> read_ngspice_wrdata_ac_response(const std::string& data_path,
                                                       const std::string& signal_name);
ReadResult<TranWaveform> read_ngspice_wrdata_tran_waveform(const std::string& data_path,
                                                           const std::string& signal_name);

class NgspiceSession final : public SimulatorSession {
 public:
  NgspiceSession(std::size_t worker_id, EvaluatorOptions options, std::string work_dir,
                 NgspiceBuiltinTask task = NgspiceBuiltinTask::kRcAc);
  ~NgspiceSession() override;

  void start() override;
  TaskResult run(const ParameterState& state, std::chrono::seconds timeout) override;
  void stop(bool graceful) noexcept override;

  std::size_t worker_id() const noexcept override {
    return worker_id_;
  }

  const std::string& work_dir() const noexcept override {
    return work_dir_;
  }

  const std::string& ngspice_executable() const noexcept {
    return ngspice_executable_;
  }

  NgspiceBuiltinTask task() const noexcept {
    return task_;
  }

 private:
  std::size_t worker_id_ = 0;
  EvaluatorOptions options_;
  std::string work_dir_;
  NgspiceBuiltinTask task_ = NgspiceBuiltinTask::kRcAc;
  std::string ngspice_executable_;
  bool started_ = false;
};

}  // namespace su
