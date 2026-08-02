#include "su/evaluator.hpp"

#include "src/pool/simulator_pool.hpp"

#include <atomic>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <utility>

namespace su {

Evaluator::Evaluator(EvaluatorOptions options, SessionFactory session_factory)
    : options_(std::move(options)) {
  if (options_.workspace_namespace.empty()) {
    options_.workspace_namespace = generate_workspace_namespace();
  }
  workspace_root_ = join_path(options_.work_dir_base, options_.workspace_namespace);
  pool_.reset(new SimulatorPool(options_, workspace_root_, std::move(session_factory)));
  pool_->start_all();
}

Evaluator::~Evaluator() {
  cleanup();
}

Evaluator::Evaluator(Evaluator&&) noexcept = default;

Evaluator& Evaluator::operator=(Evaluator&&) noexcept = default;

std::vector<TaskResult> Evaluator::run(const std::vector<ParameterState>& states) {
  if (states.empty()) {
    return {};
  }
  if (!pool_) {
    throw std::runtime_error("evaluator has no simulator pool");
  }
  return pool_->evaluate_batch(states);
}

void Evaluator::cleanup() noexcept {
  if (pool_) {
    pool_->shutdown_all();
  }
}

void Evaluator::reload_workers() {
  if (!pool_) {
    throw std::runtime_error("evaluator has no simulator pool");
  }
  pool_->shutdown_all();
  pool_->start_all();
}

std::vector<std::string> Evaluator::worker_work_dirs() const {
  if (!pool_) {
    return {};
  }
  return pool_->worker_work_dirs();
}

std::string generate_workspace_namespace() {
  static std::atomic<unsigned long> counter{0};
  std::ostringstream stream;
  stream << "eval_" << static_cast<long>(::getpid()) << "_" << counter.fetch_add(1);
  return stream.str();
}

std::string join_path(const std::string& left, const std::string& right) {
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

}  // namespace su
