#include "src/pool/simulator_pool.hpp"

#include "su/evaluator.hpp"

#include <chrono>
#include <exception>
#include <future>
#include <stdexcept>
#include <utility>

namespace su {

SimulatorPool::SimulatorPool(EvaluatorOptions options, std::string workspace_root,
                             SessionFactory factory)
    : options_(std::move(options)), workspace_root_(std::move(workspace_root)) {
  if (options_.num_workers <= 0) {
    throw std::invalid_argument("num_workers must be positive");
  }
  if (!factory) {
    throw std::invalid_argument("session factory is required");
  }

  workers_.reserve(static_cast<std::size_t>(options_.num_workers));
  for (int index = 0; index < options_.num_workers; ++index) {
    const auto worker_id = static_cast<std::size_t>(index);
    auto work_dir = join_path(workspace_root_, "worker_" + std::to_string(worker_id));
    auto session = factory(worker_id, options_, work_dir);
    if (!session) {
      throw std::runtime_error("session factory returned null");
    }
    workers_.push_back(Worker{worker_id, std::move(work_dir), std::move(session)});
  }
}

SimulatorPool::~SimulatorPool() {
  shutdown_all();
}

void SimulatorPool::start_all() {
  std::vector<std::future<void>> futures;
  futures.reserve(workers_.size());

  for (auto& worker : workers_) {
    auto* worker_ptr = &worker;
    futures.push_back(
        std::async(std::launch::async, [worker_ptr]() { worker_ptr->session->start(); }));
  }

  std::exception_ptr first_error;
  for (auto& future : futures) {
    try {
      future.get();
    } catch (...) {
      if (!first_error) {
        first_error = std::current_exception();
      }
    }
  }

  if (first_error) {
    shutdown_all();
    std::rethrow_exception(first_error);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!free_workers_.empty()) {
      free_workers_.pop();
    }
    for (std::size_t index = 0; index < workers_.size(); ++index) {
      free_workers_.push(index);
    }
    started_ = true;
  }
  available_.notify_all();
}

std::vector<TaskResult> SimulatorPool::evaluate_batch(const std::vector<ParameterState>& states) {
  if (states.empty()) {
    return {};
  }
  if (!started_) {
    throw std::runtime_error("simulator pool has not been started");
  }

  std::vector<TaskResult> results(states.size());
  std::vector<std::future<void>> futures;
  futures.reserve(states.size());

  for (std::size_t index = 0; index < states.size(); ++index) {
    futures.push_back(std::async(std::launch::async, [this, &states, &results, index]() {
      const auto worker_index = acquire_worker();
      try {
        results[index] = run_one(worker_index, states[index]);
      } catch (const std::exception& exc) {
        results[index] = TaskResult::failure(TaskStatus::kException,
                                             workers_[worker_index].work_dir, exc.what());
      } catch (...) {
        results[index] = TaskResult::failure(TaskStatus::kException,
                                             workers_[worker_index].work_dir, "unknown exception");
      }
      release_worker(worker_index);
    }));
  }

  for (auto& future : futures) {
    future.get();
  }

  return results;
}

void SimulatorPool::shutdown_all() noexcept {
  for (auto& worker : workers_) {
    worker.session->stop(true);
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!free_workers_.empty()) {
      free_workers_.pop();
    }
    started_ = false;
  }
  available_.notify_all();
}

std::vector<std::string> SimulatorPool::worker_work_dirs() const {
  std::vector<std::string> result;
  result.reserve(workers_.size());
  for (const auto& worker : workers_) {
    result.push_back(worker.work_dir);
  }
  return result;
}

std::size_t SimulatorPool::acquire_worker() {
  std::unique_lock<std::mutex> lock(mutex_);
  available_.wait(lock, [this]() { return !free_workers_.empty(); });
  auto index = free_workers_.front();
  free_workers_.pop();
  return index;
}

void SimulatorPool::release_worker(std::size_t index) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    free_workers_.push(index);
  }
  available_.notify_one();
}

TaskResult SimulatorPool::run_one(std::size_t worker_index, const ParameterState& state) noexcept {
  auto& worker = workers_[worker_index];
  try {
    auto result = worker.session->run(state, std::chrono::seconds(options_.timeout_seconds));
    if (result.work_dir.empty()) {
      result.work_dir = worker.work_dir;
    }
    return result;
  } catch (const std::exception& exc) {
    return TaskResult::failure(TaskStatus::kException, worker.work_dir, exc.what());
  } catch (...) {
    return TaskResult::failure(TaskStatus::kException, worker.work_dir, "unknown exception");
  }
}

}  // namespace su
