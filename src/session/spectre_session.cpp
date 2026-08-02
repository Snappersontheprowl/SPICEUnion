#include "su/spectre_session.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

bool make_directories(const std::string& path) {
  if (path.empty()) {
    return false;
  }

  std::string current;
  if (path.front() == '/') {
    current = "/";
  }

  std::size_t start = (path.front() == '/') ? 1U : 0U;
  while (start <= path.size()) {
    auto end = path.find('/', start);
    auto part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (!part.empty()) {
      if (!current.empty() && current.back() != '/') {
        current += "/";
      }
      current += part;
      if (::mkdir(current.c_str(), 0775) != 0 && errno != EEXIST) {
        return false;
      }
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return true;
}

bool write_all(int fd, const std::string& text) {
  const char* data = text.data();
  std::size_t remaining = text.size();
  while (remaining > 0) {
    auto written = ::write(fd, data, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    data += written;
    remaining -= static_cast<std::size_t>(written);
  }
  return true;
}

}  // namespace

namespace su {

SpectreSession::SpectreSession(
    std::size_t worker_id,
    EvaluatorOptions options,
    std::string work_dir)
    : worker_id_(worker_id), options_(std::move(options)), work_dir_(std::move(work_dir)) {}

SpectreSession::~SpectreSession() {
  stop(true);
}

void SpectreSession::start() {
  if (active_) {
    return;
  }

  prepare_workspace();
  launch_process();

  try {
    wait_for_handshake();
    if (!write_command("(setq top (sclGetCircuit \"\"))\n")) {
      throw std::runtime_error("failed to initialize Spectre circuit handle");
    }
    active_ = true;
  } catch (...) {
    discard_process(false);
    throw;
  }
}

TaskResult SpectreSession::run(const ParameterState&, std::chrono::seconds) {
  return TaskResult::failure(
      TaskStatus::kSimulationFailed,
      work_dir_,
      "SpectreSession::run is not implemented in M1.1");
}

void SpectreSession::stop(bool graceful) noexcept {
  discard_process(graceful);
}

void SpectreSession::prepare_workspace() const {
  if (!make_directories(work_dir_)) {
    throw std::runtime_error("failed to create Spectre worker directory: " + work_dir_);
  }
}

void SpectreSession::launch_process() {
  int input_pipe[2] = {-1, -1};
  int output_pipe[2] = {-1, -1};

  if (::pipe(input_pipe) != 0) {
    throw std::runtime_error("failed to create Spectre stdin pipe");
  }
  if (::pipe(output_pipe) != 0) {
    ::close(input_pipe[0]);
    ::close(input_pipe[1]);
    throw std::runtime_error("failed to create Spectre stdout pipe");
  }

  auto pid = ::fork();
  if (pid < 0) {
    ::close(input_pipe[0]);
    ::close(input_pipe[1]);
    ::close(output_pipe[0]);
    ::close(output_pipe[1]);
    throw std::runtime_error("failed to fork Spectre process");
  }

  if (pid == 0) {
    ::dup2(input_pipe[0], STDIN_FILENO);
    ::dup2(output_pipe[1], STDOUT_FILENO);
    ::dup2(output_pipe[1], STDERR_FILENO);

    ::close(input_pipe[0]);
    ::close(input_pipe[1]);
    ::close(output_pipe[0]);
    ::close(output_pipe[1]);

    ::chdir(work_dir_.c_str());

    const char* argv[] = {
        "spectre",
        options_.netlist_path.c_str(),
        "+interactive",
        "-64",
        "-o",
        work_dir_.c_str(),
        nullptr,
    };
    ::execvp("spectre", const_cast<char* const*>(argv));
    _exit(127);
  }

  ::close(input_pipe[0]);
  ::close(output_pipe[1]);

  child_pid_ = static_cast<int>(pid);
  stdin_fd_ = input_pipe[1];
  stdout_fd_ = output_pipe[0];
}

void SpectreSession::wait_for_handshake() {
  const int timeout = options_.timeout_seconds > 0 ? options_.timeout_seconds : 60;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout);

  while (std::chrono::steady_clock::now() < deadline) {
    std::string line;
    if (!read_line_with_timeout(1, &line)) {
      int status = 0;
      if (child_pid_ > 0 && ::waitpid(child_pid_, &status, WNOHANG) == child_pid_) {
        child_pid_ = -1;
        throw std::runtime_error(
            "Spectre exited before interactive handshake; recent_output=" + recent_output_text());
      }
      continue;
    }

    remember_output(line);
    if (line.find("Entering Skill interactive front end") != std::string::npos) {
      return;
    }
  }

  throw std::runtime_error(
      "Spectre handshake timed out; recent_output=" + recent_output_text());
}

bool SpectreSession::write_command(const std::string& command) noexcept {
  if (stdin_fd_ < 0) {
    return false;
  }
  return write_all(stdin_fd_, command);
}

bool SpectreSession::read_line_with_timeout(int timeout_seconds, std::string* line) {
  if (stdout_fd_ < 0 || line == nullptr) {
    return false;
  }

  line->clear();
  while (true) {
    fd_set read_set;
    FD_ZERO(&read_set);
    FD_SET(stdout_fd_, &read_set);

    timeval timeout{};
    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    auto ready = ::select(stdout_fd_ + 1, &read_set, nullptr, nullptr, &timeout);
    if (ready < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (ready == 0) {
      return false;
    }

    char ch = '\0';
    auto count = ::read(stdout_fd_, &ch, 1);
    if (count < 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    if (count == 0) {
      return !line->empty();
    }
    if (ch == '\n') {
      return true;
    }
    if (ch != '\r') {
      line->push_back(ch);
    }
  }
}

void SpectreSession::remember_output(std::string line) {
  if (line.empty()) {
    return;
  }
  recent_output_.push_back(std::move(line));
  if (recent_output_.size() > 12) {
    recent_output_.erase(recent_output_.begin());
  }
}

std::string SpectreSession::recent_output_text() const {
  std::ostringstream stream;
  for (std::size_t index = 0; index < recent_output_.size(); ++index) {
    if (index != 0) {
      stream << " | ";
    }
    stream << recent_output_[index];
  }
  auto text = stream.str();
  return text.empty() ? "no output" : text;
}

void SpectreSession::discard_process(bool graceful) noexcept {
  if (graceful && active_ && stdin_fd_ >= 0) {
    write_command("(sclQuit)\n");
  }

  if (child_pid_ > 0) {
    bool exited = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
      int status = 0;
      auto waited = ::waitpid(child_pid_, &status, WNOHANG);
      if (waited == child_pid_) {
        exited = true;
        break;
      }
      if (waited < 0 && errno == ECHILD) {
        exited = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (!exited) {
      ::kill(child_pid_, SIGKILL);
      int status = 0;
      ::waitpid(child_pid_, &status, 0);
    }
  }

  if (stdin_fd_ >= 0) {
    ::close(stdin_fd_);
  }
  if (stdout_fd_ >= 0) {
    ::close(stdout_fd_);
  }

  stdin_fd_ = -1;
  stdout_fd_ = -1;
  child_pid_ = -1;
  active_ = false;
}

}  // namespace su
