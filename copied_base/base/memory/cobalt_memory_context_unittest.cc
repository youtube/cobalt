// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/cobalt_memory_context.h"

#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)

namespace base {
namespace memory {

TEST(CobaltMemoryContextTest, BasicSetGet) {
  SetCurrentMemoryContext(MemoryContext::kDOM);
  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);

  SetCurrentMemoryContext(MemoryContext::kMedia);
  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kMedia);

  SetCurrentMemoryContext(MemoryContext::kUnknown);
  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kUnknown);
}

TEST(CobaltMemoryContextTest, ScopedContext) {
  SetCurrentMemoryContext(MemoryContext::kUnknown);

  {
    ScopedMemoryContext scoped(MemoryContext::kDOM);
    EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);

    {
      ScopedMemoryContext nested(MemoryContext::kMedia);
      EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kMedia);
    }

    EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);
  }

  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kUnknown);
}

TEST(CobaltMemoryContextTest, ThreadIsolation) {
  SetCurrentMemoryContext(MemoryContext::kDOM);

  Thread thread("MemoryContextTestThread");
  thread.Start();

  thread.task_runner()->PostTask(
      FROM_HERE, base::BindOnce([]() {
        EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kUnknown);
        SetCurrentMemoryContext(MemoryContext::kMedia);
        EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kMedia);
      }));

  thread.Stop();

  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);
}

TEST(CobaltMemoryContextTest, ContextToString) {
  EXPECT_EQ(ContextToString(MemoryContext::kDOM), "DOM");
  EXPECT_EQ(ContextToString(MemoryContext::kMedia), "Media");
  EXPECT_EQ(ContextToString(MemoryContext::kUnknown), "Unknown");
}

}  // namespace memory
}  // namespace base

#endif  // BUILDFLAG(IS_COBALT)
