#pragma once

#include "su/core.hpp"
#include "su/evaluator.hpp"
#include "su/session.hpp"
#include "su/task_result.hpp"

#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

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
  struct Worker {
    std::size_t id = 0;
    std::string work_dir;
    SimulatorSessionPtr session;
  };

  std::size_t acquire_worker();
  void release_worker(std::size_t index);
  TaskResult run_one(std::size_t worker_index, const ParameterState& state) noexcept;

  EvaluatorOptions options_;
  std::string workspace_root_;
  std::vector<Worker> workers_;
  mutable std::mutex mutex_;
  std::condition_variable available_;
  std::queue<std::size_t> free_workers_;
  bool started_ = false;
};

}  // namespace su
