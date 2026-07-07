// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/memory/cobalt_memory_task_observer.h"

#include "base/location.h"
#include "base/memory/cobalt_memory_context.h"
#include "base/pending_task.h"
#include "base/task/current_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

class CobaltMemoryTaskObserverTest : public testing::Test {
 public:
  CobaltMemoryTaskObserverTest() {
    base::memory::SetCurrentMemoryContext(
        base::memory::MemoryContext::kUnknown);
  }

  void TearDown() override {
    base::memory::SetCurrentMemoryContext(
        base::memory::MemoryContext::kUnknown);
  }

 protected:
  CobaltMemoryTaskObserver observer_;
};

TEST_F(CobaltMemoryTaskObserverTest, MapsGraphicsContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "cobalt/src/cc/trees/layer_tree_host.cc", 100,
      reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kUnknown);

  observer_.WillProcessTask(pending_task, false);

  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kGraphics);

  observer_.DidProcessTask(pending_task);

  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kUnknown);
}

TEST_F(CobaltMemoryTaskObserverTest, MapsScriptContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "v8/src/api/api.cc", 100, reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  observer_.WillProcessTask(pending_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kScript);
  observer_.DidProcessTask(pending_task);
}

TEST_F(CobaltMemoryTaskObserverTest, MapsNetworkContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "net/url_request/url_request.cc", 100,
      reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  observer_.WillProcessTask(pending_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kNetwork);
  observer_.DidProcessTask(pending_task);
}

TEST_F(CobaltMemoryTaskObserverTest, MapsMediaContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "media/base/decoder.cc", 100, reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  observer_.WillProcessTask(pending_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kMedia);
  observer_.DidProcessTask(pending_task);
}

TEST_F(CobaltMemoryTaskObserverTest, MapsIPCContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "mojo/public/cpp/bindings/receiver.cc", 100,
      reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  observer_.WillProcessTask(pending_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kPlatformIPC);
  observer_.DidProcessTask(pending_task);
}

TEST_F(CobaltMemoryTaskObserverTest, MapsUnknownContextCorrectly) {
  base::Location location = base::Location::CreateForTesting(
      "TestFunc", "unknown/path/file.cc", 100, reinterpret_cast<void*>(1));
  base::PendingTask pending_task(location, base::OnceClosure());

  observer_.WillProcessTask(pending_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kUnknown);
  observer_.DidProcessTask(pending_task);
}

TEST_F(CobaltMemoryTaskObserverTest, SupportsNestedTasks) {
  base::Location graphics_location = base::Location::CreateForTesting(
      "TestFunc", "cobalt/src/cc/trees/layer_tree_host.cc", 100,
      reinterpret_cast<void*>(1));
  base::PendingTask graphics_task(graphics_location, base::OnceClosure());

  base::Location script_location = base::Location::CreateForTesting(
      "TestFunc", "cobalt/src/v8/src/api/api.cc", 100,
      reinterpret_cast<void*>(1));
  base::PendingTask script_task(script_location, base::OnceClosure());

  observer_.WillProcessTask(graphics_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kGraphics);

  // Nested task
  observer_.WillProcessTask(script_task, false);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kScript);

  observer_.DidProcessTask(script_task);
  // Restores original context
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kGraphics);

  observer_.DidProcessTask(graphics_task);
  EXPECT_EQ(base::memory::GetCurrentMemoryContext(),
            base::memory::MemoryContext::kUnknown);
}

}  // namespace memory
}  // namespace cobalt
