#include "su/workflow.hpp"

#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace su {
namespace {

constexpr const char* kDefaultSpectreDcSweepFile = "dc.dc";
constexpr const char* kDefaultSpectreAcFile = "ac.ac";
constexpr const char* kDefaultSpectreTranFile = "tran.tran";
constexpr const char* kDefaultNgspiceDcSweepFile = "resistor_divider_dc.out";
constexpr const char* kDefaultNgspiceAcFile = "rc_ac.out";
constexpr const char* kDefaultNgspiceTranFile = "rc_tran.out";

bool finite(double value) {
  return std::isfinite(value);
}

std::string parameter_error_prefix(std::size_t index) {
  std::ostringstream stream;
  stream << "simulation case " << index << ": ";
  return stream.str();
}

void validate_options(const SimulationOptions& options) {
  if (options.netlist_path.empty()) {
    throw std::invalid_argument("netlist_path must not be empty");
  }
  if (options.workers <= 0) {
    throw std::invalid_argument("workers must be positive");
  }
  if (options.work_dir_base.empty()) {
    throw std::invalid_argument("work_dir_base must not be empty");
  }
  if (options.timeout_seconds <= 0) {
    throw std::invalid_argument("timeout_seconds must be positive");
  }
  if (options.restart_attempts < 0) {
    throw std::invalid_argument("restart_attempts must be >= 0");
  }
}

EvaluatorOptions to_evaluator_options(const SimulationOptions& options) {
  EvaluatorOptions evaluator_options;
  evaluator_options.netlist_path = options.netlist_path;
  evaluator_options.num_workers = options.workers;
  evaluator_options.work_dir_base = options.work_dir_base;
  evaluator_options.workspace_namespace = options.workspace_namespace;
  evaluator_options.timeout_seconds = options.timeout_seconds;
  evaluator_options.restart_attempts = options.restart_attempts;
  evaluator_options.result_format = options.result_format;
  return evaluator_options;
}

std::string choose_filename(const std::string& requested, const char* spectre_default,
                            const char* ngspice_default) {
  if (requested.empty() || requested == spectre_default) {
    return ngspice_default;
  }
  return requested;
}

std::string join_path_local(const std::string& left, const std::string& right) {
  return (std::filesystem::path(left) / right).string();
}

template <typename T>
ReadResult<T> task_failure(const TaskResult& task) {
  auto message = task.error_message.empty()
                     ? std::string("simulation task did not succeed")
                     : "simulation task did not succeed: " + task.error_message;
  return ReadResult<T>::failure(ResultStatus::kInvalidInput, std::move(message));
}

template <typename T>
ReadResult<T> directory_failure(const ReadResult<ResultDirectory>& directory) {
  return ReadResult<T>::failure(directory.status, directory.error_message);
}

}  // namespace

SimulationResult::SimulationResult(TaskResult task) : task_(std::move(task)) {}

bool SimulationResult::ok() const noexcept {
  return task_.ok();
}

TaskStatus SimulationResult::status() const noexcept {
  return task_.status;
}

std::string SimulationResult::status_text() const {
  return to_string(task_.status);
}

const std::string& SimulationResult::message() const noexcept {
  return task_.error_message;
}

const std::string& SimulationResult::detail() const noexcept {
  return task_.detail;
}

const std::string& SimulationResult::work_dir() const noexcept {
  return task_.work_dir;
}

ResultFormat SimulationResult::result_format() const noexcept {
  return task_.result_format;
}

ReadResult<ResultDirectory> SimulationResult::result_directory() const {
  if (!task_.ok()) {
    return task_failure<ResultDirectory>(task_);
  }
  if (task_.result_format == ResultFormat::kNspiceWrdata) {
    if (task_.work_dir.empty()) {
      return ReadResult<ResultDirectory>::failure(ResultStatus::kInvalidInput,
                                                  "work_dir must not be empty");
    }
    return ReadResult<ResultDirectory>::success({task_.work_dir, false});
  }
  return find_result_directory(task_.work_dir);
}

ReadResult<ScalarResult> SimulationResult::read_dc(const std::string& signal_name) const {
  if (!task_.ok()) {
    return task_failure<ScalarResult>(task_);
  }
  if (task_.result_format == ResultFormat::kNspiceWrdata) {
    return ReadResult<ScalarResult>::failure(
        ResultStatus::kUnsupportedFormat,
        "Ngspice wrdata scalar DC reading is not supported by the workflow facade");
  }

  const auto directory = result_directory();
  if (!directory.ok()) {
    return directory_failure<ScalarResult>(directory);
  }
  return read_dc_value(directory.value.path, signal_name, task_.result_format);
}

ReadResult<DcSweep> SimulationResult::read_dc_sweep(const std::string& sweep_name,
                                                    const std::string& signal_name,
                                                    const std::string& filename) const {
  if (!task_.ok()) {
    return task_failure<DcSweep>(task_);
  }
  if (task_.result_format == ResultFormat::kNspiceWrdata) {
    const auto data_file = choose_filename(filename, kDefaultSpectreDcSweepFile,
                                           kDefaultNgspiceDcSweepFile);
    return read_ngspice_wrdata_dc_sweep(join_path_local(task_.work_dir, data_file), sweep_name,
                                        signal_name);
  }

  const auto directory = result_directory();
  if (!directory.ok()) {
    return directory_failure<DcSweep>(directory);
  }
  return su::read_dc_sweep(directory.value.path, sweep_name, signal_name, filename,
                           task_.result_format);
}

ReadResult<AcResponse> SimulationResult::read_ac(const std::string& signal_name,
                                                 const std::string& filename) const {
  if (!task_.ok()) {
    return task_failure<AcResponse>(task_);
  }
  if (task_.result_format == ResultFormat::kNspiceWrdata) {
    const auto data_file = choose_filename(filename, kDefaultSpectreAcFile, kDefaultNgspiceAcFile);
    return read_ngspice_wrdata_ac_response(join_path_local(task_.work_dir, data_file),
                                           signal_name);
  }

  const auto directory = result_directory();
  if (!directory.ok()) {
    return directory_failure<AcResponse>(directory);
  }
  return read_ac_response(directory.value.path, signal_name, filename, task_.result_format);
}

ReadResult<TranWaveform> SimulationResult::read_tran(const std::string& signal_name,
                                                     const std::string& filename) const {
  if (!task_.ok()) {
    return task_failure<TranWaveform>(task_);
  }
  if (task_.result_format == ResultFormat::kNspiceWrdata) {
    const auto data_file =
        choose_filename(filename, kDefaultSpectreTranFile, kDefaultNgspiceTranFile);
    return read_ngspice_wrdata_tran_waveform(join_path_local(task_.work_dir, data_file),
                                             signal_name);
  }

  const auto directory = result_directory();
  if (!directory.ok()) {
    return directory_failure<TranWaveform>(directory);
  }
  return read_tran_waveform(directory.value.path, signal_name, filename, task_.result_format);
}

Simulation::Simulation(SimulationOptions options) : Simulation(std::move(options), {}) {}

Simulation::Simulation(SimulationOptions options, SessionFactory session_factory)
    : options_(std::move(options)), session_factory_(std::move(session_factory)) {
  validate_options(options_);
  if (options_.workspace_namespace.empty()) {
    options_.workspace_namespace = generate_workspace_namespace();
  }
  workspace_root_ = join_path(options_.work_dir_base, options_.workspace_namespace);
}

Simulation::~Simulation() = default;

Simulation::Simulation(Simulation&&) noexcept = default;

Simulation& Simulation::operator=(Simulation&&) noexcept = default;

void Simulation::add_parameter(std::string name) {
  if (name.empty()) {
    throw std::invalid_argument("parameter name must not be empty");
  }
  if (parameters_.count(name) != 0U) {
    throw std::invalid_argument("parameter is already declared: " + name);
  }
  parameters_.emplace(std::move(name), ParameterDeclaration{false, 0.0});
}

void Simulation::add_parameter(std::string name, double default_value) {
  if (name.empty()) {
    throw std::invalid_argument("parameter name must not be empty");
  }
  if (!finite(default_value)) {
    throw std::invalid_argument("parameter default value must be finite: " + name);
  }
  if (parameters_.count(name) != 0U) {
    throw std::invalid_argument("parameter is already declared: " + name);
  }
  parameters_.emplace(std::move(name), ParameterDeclaration{true, default_value});
}

std::vector<SimulationResult> Simulation::run(const std::vector<SimulationCase>& cases) {
  if (cases.empty()) {
    return {};
  }

  auto states = normalize_cases(cases);
  auto task_results = evaluator().run(states);

  std::vector<SimulationResult> results;
  results.reserve(task_results.size());
  for (auto& task : task_results) {
    results.emplace_back(std::move(task));
  }
  return results;
}

void Simulation::cleanup() noexcept {
  if (evaluator_) {
    evaluator_->cleanup();
    evaluator_.reset();
  }
}

const SimulationOptions& Simulation::options() const noexcept {
  return options_;
}

const std::string& Simulation::workspace_root() const noexcept {
  return workspace_root_;
}

std::vector<ParameterState> Simulation::normalize_cases(
    const std::vector<SimulationCase>& cases) const {
  std::vector<ParameterState> states;
  states.reserve(cases.size());

  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto& input = cases[index];
    ParameterState state;

    for (const auto& [name, value] : input) {
      const auto declaration = parameters_.find(name);
      if (declaration == parameters_.end()) {
        throw std::invalid_argument(parameter_error_prefix(index) +
                                    "undeclared parameter: " + name);
      }
      if (!finite(value)) {
        throw std::invalid_argument(parameter_error_prefix(index) +
                                    "parameter value must be finite: " + name);
      }
      state.emplace(name, value);
    }

    for (const auto& [name, declaration] : parameters_) {
      if (state.count(name) != 0U) {
        continue;
      }
      if (!declaration.has_default) {
        throw std::invalid_argument(parameter_error_prefix(index) +
                                    "missing required parameter: " + name);
      }
      state.emplace(name, declaration.default_value);
    }

    states.push_back(std::move(state));
  }

  return states;
}

Evaluator& Simulation::evaluator() {
  if (!evaluator_) {
    auto evaluator_options = to_evaluator_options(options_);
    if (session_factory_) {
      evaluator_.reset(new Evaluator(std::move(evaluator_options), session_factory_));
    } else {
      switch (options_.simulator) {
        case SimulatorKind::kSpectre:
          evaluator_.reset(
              new Evaluator(std::move(evaluator_options), make_spectre_session_factory()));
          break;
        case SimulatorKind::kNgspice:
          evaluator_.reset(new Evaluator(std::move(evaluator_options),
                                         make_ngspice_session_factory(options_.ngspice_task)));
          break;
      }
    }
  }
  return *evaluator_;
}

Simulation make_simulation_for_session_factory(SimulationOptions options,
                                               SessionFactory session_factory) {
  if (!session_factory) {
    throw std::invalid_argument("session factory is required");
  }
  return Simulation(std::move(options), std::move(session_factory));
}

}  // namespace su
