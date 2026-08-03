#pragma once

#include <string>

namespace su {

enum class TaskStatus {
  kSuccess,
  kSimulationFailed,
  kStartupFailed,
  kTimeout,
  kTransportFailure,
  kException,
};

const char* to_string(TaskStatus status) noexcept;

struct TaskResult {
  TaskStatus status = TaskStatus::kException;
  std::string work_dir;
  int error_code = 0;
  std::string error_message;
  std::string detail;

  bool ok() const noexcept {
    return status == TaskStatus::kSuccess;
  }

  static TaskResult success(std::string work_dir, std::string detail = {});
  static TaskResult failure(TaskStatus status, std::string work_dir, std::string error_message,
                            int error_code = 0);
};

}  // namespace su
