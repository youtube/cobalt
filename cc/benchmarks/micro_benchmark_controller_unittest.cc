// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/benchmarks/micro_benchmark_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/animation/animation_host.h"
#include "cc/benchmarks/micro_benchmark.h"
#include "cc/layers/layer.h"
#include "cc/test/fake_impl_task_runner_provider.h"
#include "cc/test/fake_layer_tree_host.h"
#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/test_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class MicroBenchmarkControllerTest : public testing::Test {
 public:
  void SetUp() override {
    impl_task_runner_provider_ =
        base::WrapUnique(new FakeImplTaskRunnerProvider);
    layer_tree_host_impl_ = std::make_unique<FakeLayerTreeHostImpl>(
        impl_task_runner_provider_.get(), &task_graph_runner_);

    animation_host_ = AnimationHost::CreateForTesting(ThreadInstance::MAIN);
    layer_tree_host_ = FakeLayerTreeHost::Create(
        &layer_tree_host_client_, &task_graph_runner_, animation_host_.get());
    layer_tree_host_->SetRootLayer(Layer::Create());
    layer_tree_host_->InitializeForTesting(
        TaskRunnerProvider::Create(
            base::SingleThreadTaskRunner::GetCurrentDefault(), nullptr),
        std::unique_ptr<Proxy>(new FakeProxy));
  }

  void TearDown() override {
    layer_tree_host_impl_ = nullptr;
    layer_tree_host_ = nullptr;
    impl_task_runner_provider_ = nullptr;
    animation_host_ = nullptr;
  }

  FakeLayerTreeHostClient layer_tree_host_client_;
  TestTaskGraphRunner task_graph_runner_;
  std::unique_ptr<AnimationHost> animation_host_;
  std::unique_ptr<FakeLayerTreeHost> layer_tree_host_;
  std::unique_ptr<FakeLayerTreeHostImpl> layer_tree_host_impl_;
  std::unique_ptr<FakeImplTaskRunnerProvider> impl_task_runner_provider_;
};

void IncrementCallCount(int* count, base::Value::Dict value) {
  ++(*count);
}

TEST_F(MicroBenchmarkControllerTest, ScheduleFail) {
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "non_existant_benchmark", base::Value::Dict(), base::DoNothing());
  EXPECT_EQ(id, 0);
}

TEST_F(MicroBenchmarkControllerTest, CommitScheduled) {
  layer_tree_host_->reset_needs_commit();
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(), base::DoNothing());
  EXPECT_GT(id, 0);
  EXPECT_TRUE(layer_tree_host_->needs_commit());
}

TEST_F(MicroBenchmarkControllerTest, BenchmarkRan) {
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  layer_tree_host_->UpdateLayers();

  EXPECT_EQ(1, run_count);
}

TEST_F(MicroBenchmarkControllerTest, MultipleBenchmarkRan) {
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);
  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  layer_tree_host_->UpdateLayers();

  EXPECT_EQ(2, run_count);

  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);
  id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  layer_tree_host_->UpdateLayers();
  EXPECT_EQ(4, run_count);

  layer_tree_host_->UpdateLayers();
  EXPECT_EQ(4, run_count);
}

TEST_F(MicroBenchmarkControllerTest, BenchmarkImplRan) {
  int run_count = 0;
  base::Value::Dict settings;
  settings.Set("run_benchmark_impl", true);

  // Schedule a main thread benchmark.
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", std::move(settings),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  // Scheduling benchmarks on the impl thread is usually done during
  // LayerTreeHostImpl::FinishCommit().
  for (auto& benchmark : layer_tree_host_->GetMicroBenchmarkController()
                             ->CreateImplBenchmarks()) {
    layer_tree_host_impl_->ScheduleMicroBenchmark(std::move(benchmark));
  }
  layer_tree_host_impl_->CommitComplete();

  // Make sure all posted messages run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, run_count);
}

TEST_F(MicroBenchmarkControllerTest, SendMessage) {
  // Send valid message to invalid benchmark (id = 0)
  base::Value::Dict message;
  message.Set("can_handle", true);
  bool message_handled =
      layer_tree_host_->SendMessageToMicroBenchmark(0, std::move(message));
  EXPECT_FALSE(message_handled);

  // Schedule a benchmark
  int run_count = 0;
  int id = layer_tree_host_->ScheduleMicroBenchmark(
      "unittest_only_benchmark", base::Value::Dict(),
      base::BindOnce(&IncrementCallCount, base::Unretained(&run_count)));
  EXPECT_GT(id, 0);

  // Send valid message to valid benchmark
  message.clear();
  message.Set("can_handle", true);
  message_handled =
      layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(message));
  EXPECT_TRUE(message_handled);

  // Send invalid message to valid benchmark
  message.clear();
  message.Set("can_handle", false);
  message_handled =
      layer_tree_host_->SendMessageToMicroBenchmark(id, std::move(message));
  EXPECT_FALSE(message_handled);
}

}  // namespace
}  // namespace cc
