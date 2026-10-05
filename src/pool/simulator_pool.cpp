#include "src/pool/simulator_pool.hpp"

#include "su/evaluator.hpp"

#include "ocp/ordered_concurrent_pool.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace su {
namespace {

namespace fs = std::filesystem;

std::string padded_index(std::size_t index) {
  std::ostringstream stream;
  stream << std::setw(6) << std::setfill('0') << index;
  return stream.str();
}

bool is_job_directory_name(const std::string& name) {
  constexpr const char* kPrefix = "case_";
  if (name.rfind(kPrefix, 0) != 0 || name.size() == std::char_traits<char>::length(kPrefix)) {
    return false;
  }
  for (std::size_t i = std::char_traits<char>::length(kPrefix); i < name.size(); ++i) {
    if (name[i] < '0' || name[i] > '9') {
      return false;
    }
  }
  return true;
}

// 把 session 固定 work_dir 里的产物快照到每 job 目录（用于无法按 job 切换产物目录的
// session，例如 Spectre 常驻 interactive 会话）。目标目录本身位于源目录之内时跳过，
// 已存在的 job 目录也跳过，避免把历史 job 的产物复制进新 job。
void snapshot_artifacts(const std::string& source_dir, const std::string& job_dir) {
  std::error_code error;
  const fs::path source(source_dir);
  const fs::path destination(job_dir);
  if (source.empty() || !fs::exists(source, error) || source == destination) {
    return;
  }
  fs::create_directories(destination, error);
  const auto destination_relative = fs::relative(destination, source, error);

  for (auto it = fs::recursive_directory_iterator(source, error);
       !error && it != fs::recursive_directory_iterator(); it.increment(error)) {
    const auto relative = fs::relative(it->path(), source, error);
    if (!destination_relative.empty() && relative == destination_relative) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_directory(error) && relative.parent_path().empty() &&
        is_job_directory_name(it->path().filename().string())) {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(error)) {
      continue;
    }
    const auto target = destination / relative;
    fs::create_directories(target.parent_path(), error);
    fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, error);
  }
}

class SimulatorPoolWorker final : public ocp::Worker<IndexedState, TaskResult> {
 public:
  SimulatorPoolWorker(SimulatorSessionPtr session, std::chrono::seconds timeout,
                      std::string worker_root, std::vector<MetricRequest> metric_requests)
      : session_(std::move(session)),
        timeout_(timeout),
        worker_root_(std::move(worker_root)),
        metric_requests_(std::move(metric_requests)) {}

  void start() override {
    session_->start();
  }

  TaskResult run(const IndexedState& job) override {
    const auto job_dir = join_path(worker_root_, "case_" + padded_index(job.index));
    TaskResult result;
    if (session_->writes_into_job_work_dir()) {
      session_->set_job_work_dir(job_dir);
      result = session_->run(job.state, timeout_);
    } else {
      result = session_->run(job.state, timeout_);
      snapshot_artifacts(session_->work_dir(), job_dir);
    }
    // 调用方始终拿到本次 job 自己的目录，而不是被复用的 worker 目录。
    result.work_dir = job_dir;
    // 指标提取必须发生在 worker 归还池子之前。
    result.metrics = extract_metrics(job_dir, result.result_format, metric_requests_);
    return result;
  }

  void stop() noexcept override {
    session_->stop(true);
  }

 private:
  SimulatorSessionPtr session_;
  std::chrono::seconds timeout_;
  std::string worker_root_;
  std::vector<MetricRequest> metric_requests_;
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
    return std::unique_ptr<ocp::Worker<IndexedState, TaskResult>>(new SimulatorPoolWorker(
        std::move(session), std::chrono::seconds(options_.timeout_seconds),
        worker_work_dirs_.at(worker_id), options_.metrics));
  };

  // worker id 为 std::nullopt 表示这个 job 从未分配到 worker：池子被关停，
  // 或者执行单元创建失败。此时没有对应的 worker 工作目录，回落到 workspace 根目录。
  auto failure_handler = [this](std::optional<std::size_t> worker_id, const IndexedState&,
                                std::exception_ptr error) {
    const std::string& work_dir =
        worker_id.has_value() ? worker_work_dirs_.at(*worker_id) : workspace_root_;
    return TaskResult::failure(TaskStatus::kException, work_dir, exception_message(error));
  };

  pool_.reset(new ocp::OrderedConcurrentPool<IndexedState, TaskResult>(
      pool_options, std::move(worker_factory), std::move(failure_handler)));
}

SimulatorPool::~SimulatorPool() = default;

void SimulatorPool::start_all() {
  pool_->start_all();
}

std::vector<TaskResult> SimulatorPool::evaluate_batch(const std::vector<ParameterState>& states) {
  std::vector<IndexedState> jobs;
  jobs.reserve(states.size());
  for (std::size_t index = 0; index < states.size(); ++index) {
    jobs.push_back(IndexedState{index, states[index]});
  }
  return pool_->run_batch(jobs);
}

void SimulatorPool::shutdown_all() noexcept {
  pool_->shutdown_all();
}

std::vector<std::string> SimulatorPool::worker_work_dirs() const {
  return worker_work_dirs_;
}

}  // namespace su
