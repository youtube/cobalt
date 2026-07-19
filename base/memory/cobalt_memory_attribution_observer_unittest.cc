// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/memory/cobalt_memory_attribution_observer.h"

#include <stdlib.h>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/debug/alias.h"
#include "base/memory/cobalt_memory_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace memory {

class CobaltMemoryAttributionObserverTest : public ::testing::Test {
 public:
  void SetUp() override {
    SetCurrentMemoryContext(MemoryContext::kUnknown);
    // Initialize dispatcher with our observer
    base::allocator::dispatcher::Dispatcher::GetInstance().InitializeForTesting(
        CobaltMemoryAttributionObserver::Get());
  }

  void TearDown() override {
    base::allocator::dispatcher::Dispatcher::GetInstance().ResetForTesting();
    SetCurrentMemoryContext(MemoryContext::kUnknown);
  }
};

TEST_F(CobaltMemoryAttributionObserverTest, AttributesToContext) {
  auto* observer = CobaltMemoryAttributionObserver::Get();
  auto* counters = observer->GetCounters();

  // Get baseline for DOM
  uint64_t initial_dom = counters[static_cast<size_t>(MemoryContext::kDOM)].value.load();

  {
    ScopedMemoryContext scoped_context(MemoryContext::kDOM);
    // Allocate some memory. We use malloc here, which should be intercepted
    // if allocator shim is active.
    void* p = malloc(1024);
    base::debug::Alias(&p);
    free(p);
  }

  uint64_t final_dom = counters[static_cast<size_t>(MemoryContext::kDOM)].value.load();
  // We expect final_dom to be greater than initial_dom by at least 1024 bytes.
  // However, allocator might allocate slightly more or less depending on metadata,
  // but it should be at least >= 1024.
  EXPECT_GE(final_dom, initial_dom + 1024);
}

TEST_F(CobaltMemoryAttributionObserverTest, NestedContexts) {
  auto* observer = CobaltMemoryAttributionObserver::Get();
  auto* counters = observer->GetCounters();

  uint64_t initial_dom = counters[static_cast<size_t>(MemoryContext::kDOM)].value.load();
  uint64_t initial_layout = counters[static_cast<size_t>(MemoryContext::kLayout)].value.load();

  {
    ScopedMemoryContext scoped_dom(MemoryContext::kDOM);
    void* p1 = malloc(1024);
    base::debug::Alias(&p1);
    free(p1);

    {
      ScopedMemoryContext scoped_layout(MemoryContext::kLayout);
      void* p2 = malloc(2048);
      base::debug::Alias(&p2);
      free(p2);
    }

    void* p3 = malloc(512);
    base::debug::Alias(&p3);
    free(p3);
  }

  uint64_t final_dom = counters[static_cast<size_t>(MemoryContext::kDOM)].value.load();
  uint64_t final_layout = counters[static_cast<size_t>(MemoryContext::kLayout)].value.load();

  EXPECT_GE(final_dom, initial_dom + 1024 + 512);
  EXPECT_GE(final_layout, initial_layout + 2048);
}

}  // namespace memory
}  // namespace base
