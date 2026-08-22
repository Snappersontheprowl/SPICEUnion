#include "su/evaluator.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
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

    const auto fail_it = state.find("fail");
    if (fail_it != state.end() && fail_it->second != 0.0) {
      return su::TaskResult::failure(su::TaskStatus::kSimulationFailed, work_dir_,
                                     "fake task failure");
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

su::EvaluatorOptions options(int workers = 2) {
  su::EvaluatorOptions opts;
  opts.netlist_path = "dummy.scs";
  opts.num_workers = workers;
  opts.work_dir_base =
      std::string(SPICEUNION_PROJECT_ROOT) + "/local/runtime/evaluator_contract";
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

}  // namespace

TEST(EvaluatorContractTest, EmptyInputReturnsEmptyResults) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::Evaluator evaluator(options(1), factory_with_states(&states));

  auto results = evaluator.run({});

  EXPECT_TRUE(results.empty());
  ASSERT_EQ(states.size(), 1U);
  EXPECT_EQ(states[0]->runs.load(), 0);
}

TEST(EvaluatorContractTest, ResultsKeepInputOrderWhenTasksFinishOutOfOrder) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::Evaluator evaluator(options(2), factory_with_states(&states));

  std::vector<su::ParameterState> inputs = {
      {{"tag", 1.0}, {"delay_ms", 80.0}},
      {{"tag", 2.0}, {"delay_ms", 0.0}},
      {{"tag", 3.0}, {"delay_ms", 10.0}},
  };

  auto results = evaluator.run(inputs);

  ASSERT_EQ(results.size(), inputs.size());
  EXPECT_TRUE(results[0].ok());
  EXPECT_TRUE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
  EXPECT_EQ(results[0].detail, "1");
  EXPECT_EQ(results[1].detail, "2");
  EXPECT_EQ(results[2].detail, "3");
}

TEST(EvaluatorContractTest, SingleTaskFailureDoesNotAffectOtherResults) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::Evaluator evaluator(options(2), factory_with_states(&states));

  std::vector<su::ParameterState> inputs = {
      {{"tag", 1.0}},
      {{"tag", 2.0}, {"fail", 1.0}},
      {{"tag", 3.0}},
  };

  auto results = evaluator.run(inputs);

  ASSERT_EQ(results.size(), inputs.size());
  EXPECT_TRUE(results[0].ok());
  EXPECT_FALSE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
  EXPECT_EQ(results[1].status, su::TaskStatus::kSimulationFailed);
}

TEST(EvaluatorContractTest, StartupFailureStopsCreatedWorkersAndPropagates) {
  std::vector<std::shared_ptr<FakeSessionState>> states(2);
  states[0] = std::make_shared<FakeSessionState>();
  states[1] = std::make_shared<FakeSessionState>();
  states[0]->fail_start = true;

  EXPECT_THROW(
      { su::Evaluator evaluator(options(2), factory_with_states(&states)); }, std::runtime_error);

  EXPECT_GE(states[0]->stops.load(), 1);
  EXPECT_GE(states[1]->stops.load(), 1);
}

TEST(EvaluatorContractTest, AutoNamespacesAvoidWorkerDirectoryCollisions) {
  std::vector<std::shared_ptr<FakeSessionState>> first_states;
  std::vector<std::shared_ptr<FakeSessionState>> second_states;

  su::Evaluator first(options(1), factory_with_states(&first_states));
  su::Evaluator second(options(1), factory_with_states(&second_states));

  auto first_dirs = first.worker_work_dirs();
  auto second_dirs = second.worker_work_dirs();

  ASSERT_EQ(first_dirs.size(), 1U);
  ASSERT_EQ(second_dirs.size(), 1U);
  EXPECT_NE(first_dirs[0], second_dirs[0]);
}

TEST(EvaluatorContractTest, ExplicitNamespaceIsReflectedInWorkerDirectories) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto opts = options(2);
  opts.workspace_namespace = "named_run";

  su::Evaluator evaluator(opts, factory_with_states(&states));

  auto dirs = evaluator.worker_work_dirs();
  ASSERT_EQ(dirs.size(), 2U);
  EXPECT_NE(dirs[0].find("/named_run/worker_0"), std::string::npos);
  EXPECT_NE(dirs[1].find("/named_run/worker_1"), std::string::npos);
}

TEST(EvaluatorContractTest, CleanupIsRepeatable) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  su::Evaluator evaluator(options(1), factory_with_states(&states));

  evaluator.cleanup();
  evaluator.cleanup();

  ASSERT_EQ(states.size(), 1U);
  EXPECT_GE(states[0]->stops.load(), 2);
}
