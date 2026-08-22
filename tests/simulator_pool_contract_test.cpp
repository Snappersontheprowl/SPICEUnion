#include "src/pool/simulator_pool.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct FakeSessionState {
  std::atomic<int> starts{0};
  std::atomic<int> stops{0};
  std::atomic<int> runs{0};
  bool fail_start = false;
};

class FakeSession final : public su::SimulatorSession {
 public:
  FakeSession(std::size_t worker_id, std::string work_dir, std::shared_ptr<FakeSessionState> state)
      : worker_id_(worker_id), work_dir_(std::move(work_dir)), state_(std::move(state)) {}

  void start() override {
    state_->starts.fetch_add(1);
    if (state_->fail_start) {
      throw std::runtime_error("fake start failure");
    }
  }

  su::TaskResult run(const su::ParameterState& state, std::chrono::seconds) override {
    state_->runs.fetch_add(1);
    const auto delay_it = state.find("delay_ms");
    if (delay_it != state.end()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay_it->second)));
    }

    const auto throw_it = state.find("throw");
    if (throw_it != state.end() && throw_it->second != 0.0) {
      throw std::runtime_error("fake run exception");
    }

    const auto fail_it = state.find("fail");
    if (fail_it != state.end() && fail_it->second != 0.0) {
      return su::TaskResult::failure(su::TaskStatus::kSimulationFailed, work_dir_,
                                     "fake simulation failure");
    }

    std::string detail;
    const auto tag_it = state.find("tag");
    if (tag_it != state.end()) {
      detail = std::to_string(static_cast<int>(tag_it->second));
    }
    return su::TaskResult::success(work_dir_, detail);
  }

  void stop(bool) noexcept override {
    state_->stops.fetch_add(1);
  }

  std::size_t worker_id() const noexcept override {
    return worker_id_;
  }

  const std::string& work_dir() const noexcept override {
    return work_dir_;
  }

 private:
  std::size_t worker_id_;
  std::string work_dir_;
  std::shared_ptr<FakeSessionState> state_;
};

std::string pool_contract_runtime_root() {
  return std::string(SPICEUNION_PROJECT_ROOT) + "/local/runtime/simulator_pool_contract";
}

su::EvaluatorOptions options(int workers = 2) {
  su::EvaluatorOptions opts;
  opts.netlist_path = "dummy.scs";
  opts.num_workers = workers;
  opts.work_dir_base = pool_contract_runtime_root();
  opts.timeout_seconds = 1;
  return opts;
}

su::SessionFactory factory_with_states(std::vector<std::shared_ptr<FakeSessionState>>* states) {
  return [states](std::size_t worker_id, const su::EvaluatorOptions&,
                  const std::string& work_dir) -> su::SimulatorSessionPtr {
    if (worker_id >= states->size()) {
      states->resize(worker_id + 1);
    }
    if (!(*states)[worker_id]) {
      (*states)[worker_id] = std::make_shared<FakeSessionState>();
    }
    return su::SimulatorSessionPtr(new FakeSession(worker_id, work_dir, (*states)[worker_id]));
  };
}

void construct_pool_with_zero_workers() {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(0), pool_contract_runtime_root(), factory_with_states(&states));
}

void construct_pool_with_null_factory() {
  su::SimulatorPool pool(options(1), pool_contract_runtime_root(), {});
}

void construct_pool_with_null_session() {
  su::SimulatorPool pool(options(1), pool_contract_runtime_root(),
                         [](std::size_t, const su::EvaluatorOptions&,
                            const std::string&) -> su::SimulatorSessionPtr { return {}; });
}

}  // namespace

TEST(SimulatorPoolContractTest, RejectsInvalidConstructionArguments) {
  EXPECT_THROW(construct_pool_with_zero_workers(), std::invalid_argument);
  EXPECT_THROW(construct_pool_with_null_factory(), std::invalid_argument);
  EXPECT_THROW(construct_pool_with_null_session(), std::runtime_error);
}

TEST(SimulatorPoolContractTest, StartAllStartsEveryWorkerAndExposesWorkDirs) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(3), pool_contract_runtime_root(), factory_with_states(&states));

  pool.start_all();

  ASSERT_EQ(states.size(), 3U);
  EXPECT_EQ(states[0]->starts.load(), 1);
  EXPECT_EQ(states[1]->starts.load(), 1);
  EXPECT_EQ(states[2]->starts.load(), 1);

  auto dirs = pool.worker_work_dirs();
  ASSERT_EQ(dirs.size(), 3U);
  EXPECT_NE(dirs[0].find("/worker_0"), std::string::npos);
  EXPECT_NE(dirs[1].find("/worker_1"), std::string::npos);
  EXPECT_NE(dirs[2].find("/worker_2"), std::string::npos);
}

TEST(SimulatorPoolContractTest, StartupFailureStopsCreatedWorkersAndPropagates) {
  std::vector<std::shared_ptr<FakeSessionState>> states(2);
  states[0] = std::make_shared<FakeSessionState>();
  states[1] = std::make_shared<FakeSessionState>();
  states[0]->fail_start = true;

  su::SimulatorPool pool(options(2), pool_contract_runtime_root(), factory_with_states(&states));

  EXPECT_THROW(pool.start_all(), std::runtime_error);
  EXPECT_GE(states[0]->stops.load(), 1);
  EXPECT_GE(states[1]->stops.load(), 1);
}

TEST(SimulatorPoolContractTest, RunBeforeStartRejectsNonEmptyBatch) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(1), pool_contract_runtime_root(), factory_with_states(&states));

  EXPECT_TRUE(pool.evaluate_batch({}).empty());
  EXPECT_THROW(pool.evaluate_batch({su::ParameterState{{"tag", 1.0}}}), std::runtime_error);
}

TEST(SimulatorPoolContractTest, ResultsKeepInputOrderWhenTasksFinishOutOfOrder) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(2), pool_contract_runtime_root(), factory_with_states(&states));
  pool.start_all();

  std::vector<su::ParameterState> inputs = {
      {{"tag", 1.0}, {"delay_ms", 80.0}},
      {{"tag", 2.0}, {"delay_ms", 0.0}},
      {{"tag", 3.0}, {"delay_ms", 10.0}},
  };

  auto results = pool.evaluate_batch(inputs);

  ASSERT_EQ(results.size(), inputs.size());
  EXPECT_EQ(results[0].detail, "1");
  EXPECT_EQ(results[1].detail, "2");
  EXPECT_EQ(results[2].detail, "3");
  EXPECT_TRUE(results[0].ok());
  EXPECT_TRUE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
}

TEST(SimulatorPoolContractTest, SingleTaskFailureDoesNotAffectOtherResults) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(2), pool_contract_runtime_root(), factory_with_states(&states));
  pool.start_all();

  std::vector<su::ParameterState> inputs = {
      {{"tag", 1.0}},
      {{"tag", 2.0}, {"fail", 1.0}},
      {{"tag", 3.0}},
  };

  auto results = pool.evaluate_batch(inputs);

  ASSERT_EQ(results.size(), inputs.size());
  EXPECT_TRUE(results[0].ok());
  EXPECT_FALSE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
  EXPECT_EQ(results[1].status, su::TaskStatus::kSimulationFailed);
}

TEST(SimulatorPoolContractTest, TaskExceptionIsConvertedToTaskResultFailure) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(2), pool_contract_runtime_root(), factory_with_states(&states));
  pool.start_all();

  std::vector<su::ParameterState> inputs = {
      {{"tag", 1.0}},
      {{"tag", 2.0}, {"throw", 1.0}},
      {{"tag", 3.0}},
  };

  auto results = pool.evaluate_batch(inputs);

  ASSERT_EQ(results.size(), inputs.size());
  EXPECT_TRUE(results[0].ok());
  EXPECT_FALSE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
  EXPECT_EQ(results[1].status, su::TaskStatus::kException);
  EXPECT_EQ(results[1].error_message, "fake run exception");
}

TEST(SimulatorPoolContractTest, ShutdownIsRepeatable) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::SimulatorPool pool(options(1), pool_contract_runtime_root(), factory_with_states(&states));
  pool.start_all();

  pool.shutdown_all();
  pool.shutdown_all();

  ASSERT_EQ(states.size(), 1U);
  EXPECT_GE(states[0]->stops.load(), 2);
}
