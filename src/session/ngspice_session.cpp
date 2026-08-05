#include "su/ngspice_session.hpp"

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

constexpr const char* kDefaultNgspiceOutput = "rc_ac.out";
constexpr const char* kDefaultNgspiceNetlist = "rc_ac.cir";
constexpr const char* kDefaultNgspiceLog = "ngspice.log";

bool make_directories(const std::string& path) {
  if (path.empty()) {
    return false;
  }

  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    return std::filesystem::is_directory(path, error);
  }
  return std::filesystem::create_directories(path, error);
}

bool is_executable_file(const std::string& path) {
  return !path.empty() && ::access(path.c_str(), X_OK) == 0;
}

std::vector<std::string> split_path_list(const char* path_text) {
  std::vector<std::string> paths;
  if (path_text == nullptr) {
    return paths;
  }

  std::string text(path_text);
  std::size_t start = 0;
  while (start <= text.size()) {
    const auto end = text.find(':', start);
    auto part = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (part.empty()) {
      part = ".";
    }
    paths.push_back(std::move(part));
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return paths;
}

std::string join_path_local(const std::string& left, const std::string& right) {
  if (left.empty()) {
    return right;
  }
  if (right.empty()) {
    return left;
  }
  if (left.back() == '/') {
    return left + right;
  }
  return left + "/" + right;
}

std::string find_ngspice_executable() {
  const char* explicit_path = std::getenv("SPICEUNION_NGSPICE");
  if (explicit_path != nullptr && is_executable_file(explicit_path)) {
    return explicit_path;
  }

  const auto paths = split_path_list(std::getenv("PATH"));
  for (const char* name : {"ngspice_con", "ngspice"}) {
    for (const auto& dir : paths) {
      auto candidate = join_path_local(dir, name);
      if (is_executable_file(candidate)) {
        return candidate;
      }
    }
  }
  return {};
}

std::string format_spice_double(double value) {
  std::ostringstream stream;
  stream << std::scientific << std::setprecision(17) << value;
  return stream.str();
}

double state_value_or(const su::ParameterState& state, const std::string& name,
                      double default_value) {
  const auto it = state.find(name);
  return it == state.end() ? default_value : it->second;
}

std::string read_file_excerpt(const std::string& path, std::size_t max_bytes = 4096) {
  std::ifstream input(path);
  if (!input) {
    return {};
  }

  std::ostringstream stream;
  char ch = '\0';
  std::size_t count = 0;
  while (count < max_bytes && input.get(ch)) {
    stream << ch;
    ++count;
  }
  return stream.str();
}

bool write_text_file(const std::string& path, const std::string& text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output << text;
  return static_cast<bool>(output);
}

struct ProcessResult {
  int exit_code = -1;
  bool timed_out = false;
  std::string error_message;
};

ProcessResult run_ngspice_batch(const std::string& executable, const std::string& work_dir,
                                const std::string& netlist_filename, const std::string& log_path,
                                std::chrono::seconds timeout) {
  const int log_fd =
      ::open(log_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, static_cast<mode_t>(0664));
  if (log_fd < 0) {
    return ProcessResult{-1, false,
                         "failed to open ngspice log file: " + std::string(std::strerror(errno))};
  }

  const pid_t pid = ::fork();
  if (pid < 0) {
    const std::string message =
        "failed to fork ngspice process: " + std::string(std::strerror(errno));
    ::close(log_fd);
    return ProcessResult{-1, false, message};
  }

  if (pid == 0) {
    ::dup2(log_fd, STDOUT_FILENO);
    ::dup2(log_fd, STDERR_FILENO);
    ::close(log_fd);
    if (::chdir(work_dir.c_str()) != 0) {
      _exit(126);
    }
    const char* argv[] = {executable.c_str(), "-b", netlist_filename.c_str(), nullptr};
    ::execv(executable.c_str(), const_cast<char* const*>(argv));
    _exit(127);
  }

  ::close(log_fd);

  const auto safe_timeout = timeout.count() > 0 ? timeout : std::chrono::seconds(60);
  const auto deadline = std::chrono::steady_clock::now() + safe_timeout;
  int status = 0;
  while (std::chrono::steady_clock::now() < deadline) {
    const pid_t wait_result = ::waitpid(pid, &status, WNOHANG);
    if (wait_result == pid) {
      if (WIFEXITED(status)) {
        return ProcessResult{WEXITSTATUS(status), false, {}};
      }
      if (WIFSIGNALED(status)) {
        return ProcessResult{128 + WTERMSIG(status), false, "ngspice was terminated by signal"};
      }
      return ProcessResult{-1, false, "ngspice exited with unknown wait status"};
    }
    if (wait_result < 0 && errno != EINTR) {
      return ProcessResult{
          -1, false, "failed while waiting for ngspice: " + std::string(std::strerror(errno))};
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  ::kill(pid, SIGTERM);
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  if (::waitpid(pid, &status, WNOHANG) == 0) {
    ::kill(pid, SIGKILL);
    ::waitpid(pid, &status, 0);
  }
  return ProcessResult{-1, true, "ngspice run timed out"};
}

}  // namespace

namespace su {

NgspiceRcAcConfig ngspice_rc_ac_config_from_state(const ParameterState& state) {
  NgspiceRcAcConfig config;
  config.resistance_ohm = state_value_or(state, "resistance_ohm", config.resistance_ohm);
  config.capacitance_f = state_value_or(state, "capacitance_f", config.capacitance_f);
  config.start_hz = state_value_or(state, "ac_start_hz", config.start_hz);
  config.stop_hz = state_value_or(state, "ac_stop_hz", config.stop_hz);
  config.points_per_decade = static_cast<int>(std::lround(
      state_value_or(state, "points_per_decade", static_cast<double>(config.points_per_decade))));
  return config;
}

std::string render_ngspice_rc_ac_netlist(const NgspiceRcAcConfig& config,
                                         const std::string& output_filename) {
  if (config.resistance_ohm <= 0.0) {
    throw std::invalid_argument("resistance_ohm must be positive");
  }
  if (config.capacitance_f <= 0.0) {
    throw std::invalid_argument("capacitance_f must be positive");
  }
  if (config.start_hz <= 0.0) {
    throw std::invalid_argument("ac_start_hz must be positive");
  }
  if (config.stop_hz <= config.start_hz) {
    throw std::invalid_argument("ac_stop_hz must be greater than ac_start_hz");
  }
  if (config.points_per_decade <= 0) {
    throw std::invalid_argument("points_per_decade must be positive");
  }
  if (output_filename.empty()) {
    throw std::invalid_argument("output_filename must not be empty");
  }

  std::ostringstream netlist;
  netlist << "* SPICEUnion Ngspice RC low-pass AC fixture\n"
          << "Vin in 0 dc 0 ac 1\n"
          << "R1 in out " << format_spice_double(config.resistance_ohm) << "\n"
          << "C1 out 0 " << format_spice_double(config.capacitance_f) << "\n"
          << ".control\n"
          << "set filetype=ascii\n"
          << "ac dec " << config.points_per_decade << " " << format_spice_double(config.start_hz)
          << " " << format_spice_double(config.stop_hz) << "\n"
          << "wrdata " << output_filename << " v(out)\n"
          << "quit\n"
          << ".endc\n"
          << ".end\n";
  return netlist.str();
}

ReadResult<AcResponse> read_ngspice_wrdata_ac_response(const std::string& data_path,
                                                       const std::string& signal_name) {
  if (data_path.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kInvalidInput,
                                           "data_path must not be empty");
  }
  if (signal_name.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kInvalidInput,
                                           "signal_name must not be empty");
  }

  std::ifstream input(data_path);
  if (!input) {
    return ReadResult<AcResponse>::failure(ResultStatus::kFileNotFound,
                                           "ngspice wrdata file was not found: " + data_path);
  }

  AcResponse response;
  response.signal = signal_name;

  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line.empty()) {
      continue;
    }

    std::istringstream parser(line);
    double frequency = 0.0;
    double real = 0.0;
    double imag = 0.0;
    std::string extra;
    if (!(parser >> frequency >> real >> imag) || (parser >> extra)) {
      return ReadResult<AcResponse>::failure(
          ResultStatus::kParseError,
          "failed to parse ngspice wrdata AC line " + std::to_string(line_number));
    }

    response.frequency_hz.push_back(frequency);
    response.real.push_back(real);
    response.imag.push_back(imag);
  }

  if (response.frequency_hz.empty()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kParseError,
                                           "ngspice wrdata AC file has no samples: " + data_path);
  }
  if (!response.shape_consistent()) {
    return ReadResult<AcResponse>::failure(ResultStatus::kParseError,
                                           "ngspice AC vectors have inconsistent lengths");
  }
  return ReadResult<AcResponse>::success(std::move(response));
}

NgspiceSession::NgspiceSession(std::size_t worker_id, EvaluatorOptions options,
                               std::string work_dir)
    : worker_id_(worker_id), options_(std::move(options)), work_dir_(std::move(work_dir)) {}

NgspiceSession::~NgspiceSession() {
  stop(true);
}

void NgspiceSession::start() {
  if (started_) {
    return;
  }
  if (!make_directories(work_dir_)) {
    throw std::runtime_error("failed to create Ngspice worker directory: " + work_dir_);
  }

  ngspice_executable_ = find_ngspice_executable();
  if (ngspice_executable_.empty()) {
    throw std::runtime_error(
        "ngspice executable was not found; set SPICEUNION_NGSPICE or update PATH");
  }
  started_ = true;
}

TaskResult NgspiceSession::run(const ParameterState& state, std::chrono::seconds timeout) {
  if (!started_) {
    start();
  }

  const auto config = ngspice_rc_ac_config_from_state(state);
  const auto netlist_path = join_path_local(work_dir_, kDefaultNgspiceNetlist);
  const auto output_path = join_path_local(work_dir_, kDefaultNgspiceOutput);
  const auto log_path = join_path_local(work_dir_, kDefaultNgspiceLog);

  try {
    const auto netlist = render_ngspice_rc_ac_netlist(config, kDefaultNgspiceOutput);
    if (!write_text_file(netlist_path, netlist)) {
      return TaskResult::failure(TaskStatus::kTransportFailure, work_dir_,
                                 "failed to write Ngspice netlist: " + netlist_path);
    }
  } catch (const std::exception& error) {
    return TaskResult::failure(TaskStatus::kException, work_dir_, error.what());
  }

  const auto process =
      run_ngspice_batch(ngspice_executable_, work_dir_, kDefaultNgspiceNetlist, log_path, timeout);
  if (process.timed_out) {
    return TaskResult::failure(TaskStatus::kTimeout, work_dir_, process.error_message);
  }
  if (process.exit_code == 126 || process.exit_code == 127) {
    return TaskResult::failure(TaskStatus::kStartupFailed, work_dir_,
                               "failed to execute ngspice; log=" + read_file_excerpt(log_path),
                               process.exit_code);
  }
  if (process.exit_code != 0) {
    return TaskResult::failure(
        TaskStatus::kSimulationFailed, work_dir_,
        "ngspice exited with non-zero status; log=" + read_file_excerpt(log_path),
        process.exit_code);
  }

  const auto response = read_ngspice_wrdata_ac_response(output_path, "v(out)");
  if (!response.ok()) {
    return TaskResult::failure(TaskStatus::kSimulationFailed, work_dir_, response.error_message);
  }

  std::ostringstream detail;
  detail << "ngspice_ac_output=" << output_path << ";samples=" << response.value.size();
  return TaskResult::success(work_dir_, detail.str());
}

void NgspiceSession::stop(bool graceful) noexcept {
  (void)graceful;
  started_ = false;
}

}  // namespace su
