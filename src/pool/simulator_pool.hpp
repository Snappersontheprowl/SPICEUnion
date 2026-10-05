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

// 池内 job 包装：保留输入下标，用于每 job 产物目录命名与追溯。
struct IndexedState {
  std::size_t index = 0;
  ParameterState state;
};

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
  std::unique_ptr<ocp::OrderedConcurrentPool<IndexedState, TaskResult>> pool_;
};

}  // namespace su
