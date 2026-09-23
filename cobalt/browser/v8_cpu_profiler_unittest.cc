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

#include "gin/converter.h"
#include "gin/test/v8_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-profiler.h"
#include "v8/include/v8-script.h"

namespace cobalt {

// V8's sampling CPU profiler backs the JS Self-Profiling API (new Profiler()),
// DevTools CPU profiling and --prof. On POSIX its sampler thread sends SIGPROF
// to the thread running JS, and the SA_SIGINFO handler reads that thread's
// registers from the ucontext_t it receives. Running the real sampler
// in-process means a platform signal layer that hands the handler an invalid
// context fails here as a crash rather than going unnoticed.
using V8CpuProfilerTest = gin::V8Test;

TEST_F(V8CpuProfilerTest, SamplingProfilerCollectsSamplesWithoutCrashing) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  v8::CpuProfiler* profiler = v8::CpuProfiler::New(isolate);
  ASSERT_TRUE(profiler);
  // Sample every 100us so that many SIGPROF deliveries land while the script
  // below is running.
  profiler->SetSamplingInterval(100);

  v8::Local<v8::String> title = gin::StringToV8(isolate, "V8CpuProfilerTest");
  profiler->StartProfiling(
      title, v8::CpuProfilingOptions(v8::kLeafNodeLineNumbers,
                                     v8::CpuProfilingOptions::kNoSampleLimit));

  struct ScopedProfiler {
    v8::CpuProfiler* profiler;
    v8::Local<v8::String> title;
    v8::CpuProfile* profile = nullptr;
    ~ScopedProfiler() {
      if (!profile) {
        profile = profiler->StopProfiling(title);
      }
      if (profile) {
        profile->Delete();
      }
      profiler->Dispose();
    }
  } scoped_profiler{profiler, title};

  // Run JavaScript on this thread for ~200ms so that it is sampled repeatedly.
  constexpr char kBusyLoopScript[] = R"(
    let x = 0;
    const deadline = Date.now() + 200;
    while (Date.now() < deadline) {
      x += Math.sqrt(x + 1);
    }
    x;
  )";
  v8::Local<v8::Script> script;
  ASSERT_TRUE(
      v8::Script::Compile(context, gin::StringToV8(isolate, kBusyLoopScript))
          .ToLocal(&script));
  v8::Local<v8::Value> unused_result;
  ASSERT_TRUE(script->Run(context).ToLocal(&unused_result));

  scoped_profiler.profile = profiler->StopProfiling(title);
  ASSERT_TRUE(scoped_profiler.profile);
  EXPECT_GT(scoped_profiler.profile->GetSamplesCount(), 0)
      << "The sampling profiler produced no samples; SIGPROF was never "
         "handled.";
}

}  // namespace cobalt
