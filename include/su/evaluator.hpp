#pragma once

#include "su/core.hpp"
#include "su/session.hpp"
#include "su/task_result.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace su {

class SimulatorPool;

using SessionFactory = std::function<SimulatorSessionPtr(
    std::size_t worker_id,
    const EvaluatorOptions& options,
    const std::string& work_dir)>;

class Evaluator {
 public:
  Evaluator(EvaluatorOptions options, SessionFactory session_factory);
  ~Evaluator();

  Evaluator(const Evaluator&) = delete;
  Evaluator& operator=(const Evaluator&) = delete;
  Evaluator(Evaluator&&) noexcept;
  Evaluator& operator=(Evaluator&&) noexcept;

  std::vector<TaskResult> run(const std::vector<ParameterState>& states);
  void cleanup() noexcept;
  void reload_workers();

  const EvaluatorOptions& options() const noexcept { return options_; }
  const std::string& workspace_root() const noexcept { return workspace_root_; }
  std::vector<std::string> worker_work_dirs() const;

 private:
  EvaluatorOptions options_;
  std::string workspace_root_;
  std::unique_ptr<SimulatorPool> pool_;
};

std::string generate_workspace_namespace();
std::string join_path(const std::string& left, const std::string& right);

}  // namespace su
