#pragma once

#include "su/evaluator.hpp"
#include "su/ngspice_session.hpp"
#include "su/result_reader.hpp"
#include "su/task_result.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace su {

enum class SimulatorKind {
  kSpectre,
  kNgspice,
};

struct SimulationOptions {
  SimulatorKind simulator = SimulatorKind::kSpectre;
  std::string netlist_path;
  int workers = 1;
  std::string work_dir_base = "local/runtime/simulations";
  std::string workspace_namespace;
  int timeout_seconds = 60;
  int restart_attempts = 1;
  ResultFormat result_format = ResultFormat::kUnknown;
  NgspiceBuiltinTask ngspice_task = NgspiceBuiltinTask::kRcAc;
};

using SimulationCase = std::map<std::string, double>;

class SimulationResult {
 public:
  explicit SimulationResult(TaskResult task);

  bool ok() const noexcept;
  TaskStatus status() const noexcept;
  std::string status_text() const;
  const std::string& message() const noexcept;
  const std::string& detail() const noexcept;

  const std::string& work_dir() const noexcept;
  ResultFormat result_format() const noexcept;

  ReadResult<ResultDirectory> result_directory() const;
  ReadResult<ScalarResult> read_dc(const std::string& signal_name) const;
  ReadResult<DcSweep> read_dc_sweep(const std::string& sweep_name,
                                    const std::string& signal_name,
                                    const std::string& filename = "dc.dc") const;
  ReadResult<AcResponse> read_ac(const std::string& signal_name,
                                 const std::string& filename = "ac.ac") const;
  ReadResult<TranWaveform> read_tran(const std::string& signal_name,
                                     const std::string& filename = "tran.tran") const;

 private:
  TaskResult task_;
};

class Simulation {
 public:
  explicit Simulation(SimulationOptions options);
  ~Simulation();

  Simulation(const Simulation&) = delete;
  Simulation& operator=(const Simulation&) = delete;
  Simulation(Simulation&&) noexcept;
  Simulation& operator=(Simulation&&) noexcept;

  void add_parameter(std::string name);
  void add_parameter(std::string name, double default_value);

  std::vector<SimulationResult> run(const std::vector<SimulationCase>& cases);

  void cleanup() noexcept;
  const SimulationOptions& options() const noexcept;
  const std::string& workspace_root() const noexcept;

 private:
  struct ParameterDeclaration {
    bool has_default = false;
    double default_value = 0.0;
  };

  Simulation(SimulationOptions options, SessionFactory session_factory);

  std::vector<ParameterState> normalize_cases(const std::vector<SimulationCase>& cases) const;
  Evaluator& evaluator();

  SimulationOptions options_;
  std::string workspace_root_;
  std::map<std::string, ParameterDeclaration> parameters_;
  SessionFactory session_factory_;
  std::unique_ptr<Evaluator> evaluator_;

  friend Simulation make_simulation_for_session_factory(SimulationOptions options,
                                                        SessionFactory session_factory);
};

Simulation make_simulation_for_session_factory(SimulationOptions options,
                                               SessionFactory session_factory);

}  // namespace su
