// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/backend/graphics_system.h"

#include <algorithm>
#include <memory>

#include "base/time/time.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "starboard/common/time.h"
#include "starboard/log.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cobalt {
namespace renderer {
namespace backend {

namespace {

// Number of initializations to use for measuring the reference creation time.
const int kReferenceCount = 5;
}  // namespace

TEST(GraphicsSystemTest, FLAKY_GraphicsSystemCanBeInitializedOften) {
  // Test whether the graphics system can be initialized often without slowing
  // down.
  std::unique_ptr<GraphicsSystem> graphics_system;

  // Treat the first initialization as a 'warm up'.
  graphics_system = CreateDefaultGraphicsSystem();
  graphics_system.reset();

  int64_t start = starboard::CurrentMonotonicTime();
  for (int i = 0; i < kReferenceCount; ++i) {
    graphics_system = CreateDefaultGraphicsSystem();
    graphics_system.reset();
  }
  int64_t time_per_initialization_usec =
      (starboard::CurrentMonotonicTime() - start) / kReferenceCount;
  SB_LOG(INFO) << "Measured duration "
               << time_per_initialization_usec /
                      base::Time::kMicrosecondsPerMillisecond
               << "ms per initialization.";

  // Graphics system initializations should not take more than the maximum of
  // 200ms or three times as long as the time we just measured.
  int64_t maximum_time_usec_per_initialization =
      std::max<int64_t>(3 * time_per_initialization_usec,
                        200 * base::Time::kMicrosecondsPerMillisecond);

  int64_t last = starboard::CurrentMonotonicTime();
  for (int i = 0; i < 20; ++i) {
    graphics_system = CreateDefaultGraphicsSystem();
    graphics_system.reset();
    int64_t now = starboard::CurrentMonotonicTime();
    int64_t elapsed_time_usec = now - last;
    SB_LOG(INFO) << "Test duration "
                 << elapsed_time_usec / base::Time::kMicrosecondsPerMillisecond
                 << "ms.";
    ASSERT_LT(elapsed_time_usec, maximum_time_usec_per_initialization);
    last = now;
  }
}

TEST(GraphicsSystemTest, FLAKY_GraphicsContextCanBeInitializedOften) {
  // Test whether the graphics system and graphics context can be initialized
  // often without slowing down.
  std::unique_ptr<GraphicsSystem> graphics_system;
  std::unique_ptr<GraphicsContext> graphics_context;

  // Treat the first initialization as a 'warm up'.
  graphics_system = CreateDefaultGraphicsSystem();
  graphics_context = graphics_system->CreateGraphicsContext();

  graphics_context.reset();
  graphics_system.reset();

  int64_t start = starboard::CurrentMonotonicTime();
  for (int i = 0; i < kReferenceCount; ++i) {
    graphics_system = CreateDefaultGraphicsSystem();
    graphics_context = graphics_system->CreateGraphicsContext();

    graphics_context.reset();
    graphics_system.reset();
  }
  int64_t time_per_initialization_usec =
      base::Time::kMicrosecondsPerMillisecond +
      (starboard::CurrentMonotonicTime() - start) / kReferenceCount;
  SB_LOG(INFO) << "Measured duration "
               << time_per_initialization_usec /
                      base::Time::kMicrosecondsPerMillisecond
               << "ms per initialization.";

  // Graphics system and context initializations should not take more than the
  // maximum of 200ms or three times as long as the time we just measured.
  int64_t maximum_time_usec_per_initialization =
      std::max<int64_t>(3 * time_per_initialization_usec,
                        200 * base::Time::kMicrosecondsPerMillisecond);

  int64_t last = starboard::CurrentMonotonicTime();
  for (int i = 0; i < 20; ++i) {
    graphics_system = CreateDefaultGraphicsSystem();
    graphics_context = graphics_system->CreateGraphicsContext();

    graphics_context.reset();
    graphics_system.reset();

    int64_t now = starboard::CurrentMonotonicTime();
    int64_t elapsed_time_usec = now - last;
    SB_LOG(INFO) << "Test duration "
                 << elapsed_time_usec / base::Time::kMicrosecondsPerMillisecond
                 << "ms.";
    ASSERT_LT(elapsed_time_usec, maximum_time_usec_per_initialization);
    last = now;
  }
}

}  // namespace backend
}  // namespace renderer
}  // namespace cobalt
