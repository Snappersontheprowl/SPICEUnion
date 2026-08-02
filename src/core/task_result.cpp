#include "su/task_result.hpp"

#include <utility>

namespace su {

const char* to_string(TaskStatus status) noexcept {
  switch (status) {
    case TaskStatus::kSuccess:
      return "success";
    case TaskStatus::kSimulationFailed:
      return "simulation_failed";
    case TaskStatus::kStartupFailed:
      return "startup_failed";
    case TaskStatus::kTimeout:
      return "timeout";
    case TaskStatus::kTransportFailure:
      return "transport_failure";
    case TaskStatus::kException:
      return "exception";
  }
  return "unknown";
}

TaskResult TaskResult::success(std::string work_dir, std::string detail) {
  TaskResult result;
  result.status = TaskStatus::kSuccess;
  result.work_dir = std::move(work_dir);
  result.detail = std::move(detail);
  return result;
}

TaskResult TaskResult::failure(
    TaskStatus status,
    std::string work_dir,
    std::string error_message,
    int error_code) {
  TaskResult result;
  result.status = status;
  result.work_dir = std::move(work_dir);
  result.error_code = error_code;
  result.error_message = std::move(error_message);
  return result;
}

}  // namespace su
