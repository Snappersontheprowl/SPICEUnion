#include "src/pool/simulator_pool.hpp"

#include "su/evaluator.hpp"

#include "ocp/ordered_concurrent_pool.hpp"

#include <chrono>
#include <exception>
#include <stdexcept>
#include <utility>

namespace su {
namespace {

class SimulatorPoolWorker final : public ocp::Worker<ParameterState, TaskResult> {
 public:
  SimulatorPoolWorker(SimulatorSessionPtr session, std::chrono::seconds timeout)
      : session_(std::move(session)), timeout_(timeout) {}

  void start() override {
    session_->start();
  }

  TaskResult run(const ParameterState& state) override {
    auto result = session_->run(state, timeout_);
    if (result.work_dir.empty()) {
      result.work_dir = session_->work_dir();
    }
    return result;
  }

  void stop() noexcept override {
    session_->stop(true);
  }

 private:
  SimulatorSessionPtr session_;
  std::chrono::seconds timeout_;
};

std::string exception_message(std::exception_ptr error) {
  try {
    if (error) {
      std::rethrow_exception(error);
    }
  } catch (const std::exception& exc) {
    return exc.what();
  } catch (...) {
    return "unknown exception";
  }
  return "unknown exception";
}

}  // namespace

SimulatorPool::SimulatorPool(EvaluatorOptions options, std::string workspace_root,
                             SessionFactory factory)
    : options_(std::move(options)), workspace_root_(std::move(workspace_root)) {
  if (options_.num_workers <= 0) {
    throw std::invalid_argument("num_workers must be positive");
  }
  if (!factory) {
    throw std::invalid_argument("session factory is required");
  }

  worker_work_dirs_.reserve(static_cast<std::size_t>(options_.num_workers));
  for (int index = 0; index < options_.num_workers; ++index) {
    const auto worker_id = static_cast<std::size_t>(index);
    worker_work_dirs_.push_back(join_path(workspace_root_, "worker_" + std::to_string(worker_id)));
  }

  ocp::PoolOptions pool_options;
  pool_options.worker_count = static_cast<std::size_t>(options_.num_workers);

  auto worker_factory = [this, factory = std::move(factory)](std::size_t worker_id) mutable {
    auto session = factory(worker_id, options_, worker_work_dirs_.at(worker_id));
    if (!session) {
      throw std::runtime_error("session factory returned null");
    }
    return std::unique_ptr<ocp::Worker<ParameterState, TaskResult>>(new SimulatorPoolWorker(
        std::move(session), std::chrono::seconds(options_.timeout_seconds)));
  };

  auto failure_handler = [this](std::size_t worker_id, const ParameterState&,
                                std::exception_ptr error) {
    return TaskResult::failure(TaskStatus::kException, worker_work_dirs_.at(worker_id),
                               exception_message(error));
  };

  pool_.reset(new ocp::OrderedConcurrentPool<ParameterState, TaskResult>(
      pool_options, std::move(worker_factory), std::move(failure_handler)));
}

SimulatorPool::~SimulatorPool() = default;

void SimulatorPool::start_all() {
  pool_->start_all();
}

std::vector<TaskResult> SimulatorPool::evaluate_batch(const std::vector<ParameterState>& states) {
  return pool_->run_batch(states);
}

void SimulatorPool::shutdown_all() noexcept {
  pool_->shutdown_all();
}

std::vector<std::string> SimulatorPool::worker_work_dirs() const {
  return worker_work_dirs_;
}

}  // namespace su
