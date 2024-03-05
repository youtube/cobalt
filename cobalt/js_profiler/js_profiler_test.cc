// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/dom/testing/stub_environment_settings.h"
#include "cobalt/dom/testing/stub_window.h"
#include "cobalt/dom/testing/test_with_javascript.h"
#include "cobalt/js_profiler/profiler.h"
#include "cobalt/js_profiler/profiler_trace_wrapper.h"
#include "cobalt/script/testing/mock_exception_state.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::StrictMock;

namespace cobalt {
namespace js_profiler {

class ProfilerTest : public dom::testing::TestWithJavaScript {
 public:
  ProfilerTest() {}

  void CollectGarbage() {
    window_.web_context()->javascript_engine()->CollectGarbage();
  }

 protected:
  dom::testing::StubWindow window_;
  StrictMock<script::testing::MockExceptionState> exception_state_;
};

TEST_F(ProfilerTest, ProfilerStop) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(10);
  init_options.set_max_buffer_size(1000);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));

  auto promise = profiler_->Stop(window_.environment_settings());
  EXPECT_EQ(profiler_->stopped(), true);
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
}

TEST_F(ProfilerTest, ProfilerAlreadyStopped) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(10);
  init_options.set_max_buffer_size(0);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));

  auto promise = profiler_->Stop(window_.environment_settings());
  EXPECT_EQ(profiler_->stopped(), true);
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
  auto promise2 = profiler_->Stop(window_.environment_settings());
  EXPECT_TRUE(promise2->State() == cobalt::script::PromiseState::kRejected);
}

TEST_F(ProfilerTest, ProfilerZeroSampleInterval) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(0);
  init_options.set_max_buffer_size(0);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));
  EXPECT_EQ(profiler_->sample_interval(), 10);
  auto promise = profiler_->Stop(window_.environment_settings());
  EXPECT_EQ(profiler_->stopped(), true);
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
}

TEST_F(ProfilerTest, ProfilerOutRangeSampleInterval) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(-1);
  init_options.set_max_buffer_size(0);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));
  EXPECT_EQ(profiler_->sample_interval(), 10);
  auto promise = profiler_->Stop(window_.environment_settings());
  EXPECT_EQ(profiler_->stopped(), true);
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kPending);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(promise->State() == cobalt::script::PromiseState::kFulfilled);
}

TEST_F(ProfilerTest, ProfilerJSCode) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("Profiler", &result));
  EXPECT_EQ(result, "function Profiler() { [native code] }");
}

TEST_F(ProfilerTest, ProfilerGroupDisposesOfCpuProfiler) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(10);
  init_options.set_max_buffer_size(1);

  auto profiler_group = ProfilerGroup::From(window_.environment_settings());
  EXPECT_FALSE(profiler_group->active());
  EXPECT_EQ(profiler_group->num_active_profilers(), 0);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));
  EXPECT_EQ(profiler_group->num_active_profilers(), 1);
  EXPECT_TRUE(profiler_group->active());

  auto promise = profiler_->Stop(window_.environment_settings());
  EXPECT_EQ(profiler_->stopped(), true);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(profiler_group->num_active_profilers(), 0);
  EXPECT_FALSE(profiler_group->active());
}

TEST_F(ProfilerTest, ProfilerCanBeCancelled) {
  v8::HandleScope scope(web::get_isolate(window_.environment_settings()));
  ProfilerInitOptions init_options;
  init_options.set_sample_interval(10);
  init_options.set_max_buffer_size(1000);

  scoped_refptr<Profiler> profiler_(new Profiler(
      window_.environment_settings(), init_options, &exception_state_));

  profiler_->Cancel();
  EXPECT_EQ(profiler_->stopped(), true);
}

}  // namespace js_profiler
}  // namespace cobalt
