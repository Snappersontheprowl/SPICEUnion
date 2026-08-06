#pragma once

#include "su/core.hpp"
#include "su/evaluator.hpp"
#include "su/session.hpp"
#include "su/task_result.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ocp {
template <class Job, class Result>
class OrderedConcurrentPool;
}

namespace su {

class SimulatorPool {
 public:
  SimulatorPool(EvaluatorOptions options, std::string workspace_root, SessionFactory factory);
  ~SimulatorPool();

  SimulatorPool(const SimulatorPool&) = delete;
  SimulatorPool& operator=(const SimulatorPool&) = delete;

  void start_all();
  std::vector<TaskResult> evaluate_batch(const std::vector<ParameterState>& states);
  void shutdown_all() noexcept;
  std::vector<std::string> worker_work_dirs() const;

 private:
  EvaluatorOptions options_;
  std::string workspace_root_;
  std::vector<std::string> worker_work_dirs_;
  std::unique_ptr<ocp::OrderedConcurrentPool<ParameterState, TaskResult>> pool_;
};

}  // namespace su
