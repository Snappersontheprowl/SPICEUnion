#include "su/workflow.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
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
};

void write_text(const std::filesystem::path& path, const std::string& text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path);
  output << text;
}

class FakeSession final : public su::SimulatorSession {
 public:
  FakeSession(std::size_t worker_id, std::string work_dir, std::shared_ptr<FakeSessionState> state)
      : worker_id_(worker_id), work_dir_(std::move(work_dir)), state_(std::move(state)) {}

  void start() override {
    state_->starts.fetch_add(1);
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
                                     "fake workflow failure");
    }

    if (state.find("write_ac") != state.end()) {
      write_text(std::filesystem::path(work_dir_) / "rc_ac.out",
                 "1.00000000e+06  9.99960523e-01 -6.28293727e-03\n"
                 "1.12201845e+06  9.99950302e-01 -7.04949950e-03\n");
    }

    std::ostringstream detail;
    const auto tag_it = state.find("tag");
    if (tag_it != state.end()) {
      detail << "tag=" << static_cast<int>(tag_it->second);
    }
    const auto optional_it = state.find("optional");
    if (optional_it != state.end()) {
      if (detail.tellp() > 0) {
        detail << ";";
      }
      detail << "optional=" << optional_it->second;
    }

    auto result = su::TaskResult::success(work_dir_, detail.str());
    result.result_format = su::ResultFormat::kNspiceWrdata;
    return result;
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

std::string workflow_runtime_root() {
  return std::string(SPICEUNION_PROJECT_ROOT) + "/local/runtime/workflow_contract";
}

su::SimulationOptions options(int workers = 2) {
  su::SimulationOptions opts;
  opts.netlist_path = "dummy.scs";
  opts.workers = workers;
  opts.work_dir_base = workflow_runtime_root();
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

su::Simulation make_fake_simulation(std::vector<std::shared_ptr<FakeSessionState>>* states,
                                    int workers = 2) {
  return su::make_simulation_for_session_factory(options(workers), factory_with_states(states));
}

}  // namespace

TEST(SimulationOptionsTest, RejectsInvalidOptions) {
  std::vector<std::shared_ptr<FakeSessionState>> states;

  auto invalid = options();
  invalid.netlist_path.clear();
  EXPECT_THROW(su::make_simulation_for_session_factory(invalid, factory_with_states(&states)),
               std::invalid_argument);

  invalid = options(0);
  EXPECT_THROW(su::make_simulation_for_session_factory(invalid, factory_with_states(&states)),
               std::invalid_argument);

  invalid = options();
  invalid.work_dir_base.clear();
  EXPECT_THROW(su::make_simulation_for_session_factory(invalid, factory_with_states(&states)),
               std::invalid_argument);

  invalid = options();
  invalid.timeout_seconds = 0;
  EXPECT_THROW(su::make_simulation_for_session_factory(invalid, factory_with_states(&states)),
               std::invalid_argument);

  invalid = options();
  invalid.restart_attempts = -1;
  EXPECT_THROW(su::make_simulation_for_session_factory(invalid, factory_with_states(&states)),
               std::invalid_argument);

  EXPECT_THROW(su::make_simulation_for_session_factory(options(), {}), std::invalid_argument);
}

TEST(SimulationParameterContractTest, RejectsEmptyAndDuplicateParameterNames) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);

  EXPECT_THROW(simulation.add_parameter(""), std::invalid_argument);
  simulation.add_parameter("wp");
  EXPECT_THROW(simulation.add_parameter("wp"), std::invalid_argument);
  EXPECT_THROW(simulation.add_parameter("wp", 1.0), std::invalid_argument);
  EXPECT_THROW(simulation.add_parameter("bad_default", std::nan("")), std::invalid_argument);
}

TEST(SimulationRunContractTest, EmptyBatchReturnsEmptyWithoutStartingWorkers) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 2);
  simulation.add_parameter("tag");

  auto results = simulation.run({});

  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(states.empty());
}

TEST(SimulationRunContractTest, RejectsUndeclaredParameterBeforeStartingWorkers) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("wp");

  EXPECT_THROW(simulation.run({su::SimulationCase{{"wp", 1.0}, {"wn", 2.0}}}),
               std::invalid_argument);
  EXPECT_TRUE(states.empty());
}

TEST(SimulationRunContractTest, RejectsMissingRequiredParameterBeforeStartingWorkers) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("wp");
  simulation.add_parameter("wn");

  EXPECT_THROW(simulation.run({su::SimulationCase{{"wp", 1.0}}}), std::invalid_argument);
  EXPECT_TRUE(states.empty());
}

TEST(SimulationRunContractTest, RejectsNonFiniteParameterBeforeStartingWorkers) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("wp");

  EXPECT_THROW(simulation.run({su::SimulationCase{{"wp", std::nan("")}}}),
               std::invalid_argument);
  EXPECT_TRUE(states.empty());
}

TEST(SimulationRunContractTest, FillsDefaultParameters) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("tag");
  simulation.add_parameter("optional", 3.5);

  auto results = simulation.run({su::SimulationCase{{"tag", 7.0}}});

  ASSERT_EQ(results.size(), 1U);
  EXPECT_TRUE(results[0].ok());
  EXPECT_NE(results[0].detail().find("tag=7"), std::string::npos);
  EXPECT_NE(results[0].detail().find("optional=3.5"), std::string::npos);
}

TEST(SimulationRunContractTest, ResultsKeepInputOrderWhenTasksFinishOutOfOrder) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 2);
  simulation.add_parameter("tag");
  simulation.add_parameter("delay_ms", 0.0);

  std::vector<su::SimulationCase> cases = {
      {{"tag", 1.0}, {"delay_ms", 80.0}},
      {{"tag", 2.0}, {"delay_ms", 0.0}},
      {{"tag", 3.0}, {"delay_ms", 10.0}},
  };

  auto results = simulation.run(cases);

  ASSERT_EQ(results.size(), cases.size());
  EXPECT_EQ(results[0].detail(), "tag=1");
  EXPECT_EQ(results[1].detail(), "tag=2");
  EXPECT_EQ(results[2].detail(), "tag=3");
}

TEST(SimulationRunContractTest, SingleTaskFailureIsWrappedWithoutPoisoningOtherResults) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 2);
  simulation.add_parameter("tag");
  simulation.add_parameter("fail", 0.0);

  std::vector<su::SimulationCase> cases = {
      {{"tag", 1.0}},
      {{"tag", 2.0}, {"fail", 1.0}},
      {{"tag", 3.0}},
  };

  auto results = simulation.run(cases);

  ASSERT_EQ(results.size(), cases.size());
  EXPECT_TRUE(results[0].ok());
  EXPECT_FALSE(results[1].ok());
  EXPECT_TRUE(results[2].ok());
  EXPECT_EQ(results[1].status(), su::TaskStatus::kSimulationFailed);
  EXPECT_EQ(results[1].status_text(), "simulation_failed");
  EXPECT_EQ(results[1].message(), "fake workflow failure");
}

TEST(SimulationResultReaderTest, ReadsNgspiceWrdataThroughResultFacade) {
  std::filesystem::remove_all(workflow_runtime_root());
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("write_ac", 0.0);

  auto results = simulation.run({su::SimulationCase{{"write_ac", 1.0}}});

  ASSERT_EQ(results.size(), 1U);
  ASSERT_TRUE(results[0].ok());
  EXPECT_EQ(results[0].result_format(), su::ResultFormat::kNspiceWrdata);

  const auto directory = results[0].result_directory();
  ASSERT_TRUE(directory.ok()) << directory.error_message;
  EXPECT_FALSE(directory.value.raw_directory_found);
  EXPECT_EQ(std::filesystem::path(directory.value.path), std::filesystem::path(results[0].work_dir()));

  const auto response = results[0].read_ac("v(out)");
  ASSERT_TRUE(response.ok()) << response.error_message;
  EXPECT_EQ(response.value.signal, "v(out)");
  ASSERT_EQ(response.value.size(), 2U);
  EXPECT_NEAR(response.value.real[0], 0.999960523, 1.0e-12);
  EXPECT_NEAR(response.value.imag[0], -0.00628293727, 1.0e-14);

  std::filesystem::remove_all(workflow_runtime_root());
}

TEST(SimulationResultReaderTest, FailedTaskRejectsResultReading) {
  std::vector<std::shared_ptr<FakeSessionState>> states;
  auto simulation = make_fake_simulation(&states, 1);
  simulation.add_parameter("fail", 0.0);

  auto results = simulation.run({su::SimulationCase{{"fail", 1.0}}});

  ASSERT_EQ(results.size(), 1U);
  ASSERT_FALSE(results[0].ok());

  const auto response = results[0].read_ac("v(out)");
  EXPECT_FALSE(response.ok());
  EXPECT_EQ(response.status, su::ResultStatus::kInvalidInput);
  EXPECT_NE(response.error_message.find("fake workflow failure"), std::string::npos);
}
