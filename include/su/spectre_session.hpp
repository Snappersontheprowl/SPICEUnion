#pragma once

#include "su/core.hpp"
#include "su/session.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace su {

class SpectreSession final : public SimulatorSession {
 public:
  SpectreSession(std::size_t worker_id, EvaluatorOptions options, std::string work_dir);
  ~SpectreSession() override;

  SpectreSession(const SpectreSession&) = delete;
  SpectreSession& operator=(const SpectreSession&) = delete;

  void start() override;
  TaskResult run(const ParameterState& state, std::chrono::seconds timeout) override;
  void stop(bool graceful) noexcept override;

  std::size_t worker_id() const noexcept override { return worker_id_; }
  const std::string& work_dir() const noexcept override { return work_dir_; }
  const std::vector<std::string>& recent_output() const noexcept { return recent_output_; }

 private:
  void prepare_workspace() const;
  void launch_process();
  void wait_for_handshake();
  bool write_command(const std::string& command) noexcept;
  bool read_line_with_timeout(int timeout_seconds, std::string* line);
  void remember_output(std::string line);
  std::string recent_output_text() const;
  void discard_process(bool graceful) noexcept;

  std::size_t worker_id_;
  EvaluatorOptions options_;
  std::string work_dir_;
  int stdin_fd_ = -1;
  int stdout_fd_ = -1;
  int child_pid_ = -1;
  bool active_ = false;
  std::vector<std::string> recent_output_;
};

}  // namespace su
