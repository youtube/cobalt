// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_builder.h"

#include "base/files/file_path.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/call_stack_profile_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

using Frame = base::StackSamplingProfiler::Frame;
using Module = base::ModuleCache::Module;

namespace metrics {

namespace {

constexpr CallStackProfileParams kProfileParams = {
    CallStackProfileParams::BROWSER_PROCESS,
    CallStackProfileParams::MAIN_THREAD,
    CallStackProfileParams::PROCESS_STARTUP};

class TestingCallStackProfileBuilder : public CallStackProfileBuilder {
 public:
  TestingCallStackProfileBuilder(
      const CallStackProfileParams& profile_params,
      base::OnceClosure completed_callback = base::OnceClosure());

  ~TestingCallStackProfileBuilder() override;

  const SampledProfile& sampled_profile() { return sampled_profile_; }

 protected:
  // Overridden for testing.
  void PassProfilesToMetricsProvider(SampledProfile sampled_profile) override;

 private:
  // The completed profile.
  SampledProfile sampled_profile_;
};

TestingCallStackProfileBuilder::TestingCallStackProfileBuilder(
    const CallStackProfileParams& profile_params,
    base::OnceClosure completed_callback)
    : CallStackProfileBuilder(profile_params, std::move(completed_callback)) {}

TestingCallStackProfileBuilder::~TestingCallStackProfileBuilder() = default;

void TestingCallStackProfileBuilder::PassProfilesToMetricsProvider(
    SampledProfile sampled_profile) {
  sampled_profile_ = std::move(sampled_profile);
}

}  // namespace

TEST(CallStackProfileBuilderTest, SetProcessMilestone) {
  auto profile_builder =
      std::make_unique<TestingCallStackProfileBuilder>(kProfileParams);

  // The default milestone is 0.
  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(std::vector<Frame>());

  CallStackProfileBuilder::SetProcessMilestone(1);
  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(std::vector<Frame>());

  profile_builder->OnProfileCompleted(base::TimeDelta(), base::TimeDelta());

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_call_stack_profile());
  const CallStackProfile& profile = proto.call_stack_profile();

  ASSERT_EQ(2, profile.deprecated_sample_size());

  uint32_t process_milestones = 0;
  for (int i = 0; i < profile.deprecated_sample(0).process_phase().size(); ++i)
    process_milestones |=
        1U << profile.deprecated_sample(0).process_phase().Get(i);
  EXPECT_EQ(0U, process_milestones);

  process_milestones = 0;
  for (int i = 0; i < profile.deprecated_sample(1).process_phase().size(); ++i)
    process_milestones |=
        1U << profile.deprecated_sample(1).process_phase().Get(i);
  EXPECT_EQ(1U << 1, process_milestones);
}

TEST(CallStackProfileBuilderTest, ProfilingCompleted) {
  // Set up a mock completed callback which will be run once.
  base::MockCallback<base::OnceClosure> mock_closure;
  EXPECT_CALL(mock_closure, Run()).Times(1);

  auto profile_builder = std::make_unique<TestingCallStackProfileBuilder>(
      kProfileParams, mock_closure.Get());

#if defined(OS_WIN)
  uint64_t module_md5 = 0x46C3E4166659AC02ULL;
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  uint64_t module_md5 = 0x554838A8451AC36CULL;
  base::FilePath module_path("/some/path/to/chrome");
#endif

  const uintptr_t module_base_address1 = 0x1000;
  Module module1 = {module_base_address1, "1", module_path};
  Frame frame1 = {module_base_address1 + 0x10, module1};

  const uintptr_t module_base_address2 = 0x1100;
  Module module2 = {module_base_address2, "2", module_path};
  Frame frame2 = {module_base_address2 + 0x10, module2};

  const uintptr_t module_base_address3 = 0x1010;
  Module module3 = {module_base_address3, "3", module_path};
  Frame frame3 = {module_base_address3 + 0x10, module3};

  std::vector<Frame> frames1 = {frame1, frame2};
  std::vector<Frame> frames2 = {frame3};

  profile_builder->OnSampleCompleted(frames1);
  profile_builder->OnSampleCompleted(frames2);
  profile_builder->OnProfileCompleted(base::TimeDelta::FromMilliseconds(500),
                                      base::TimeDelta::FromMilliseconds(100));

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_process());
  ASSERT_EQ(BROWSER_PROCESS, proto.process());
  ASSERT_TRUE(proto.has_thread());
  ASSERT_EQ(MAIN_THREAD, proto.thread());
  ASSERT_TRUE(proto.has_trigger_event());
  ASSERT_EQ(SampledProfile::PROCESS_STARTUP, proto.trigger_event());

  ASSERT_TRUE(proto.has_call_stack_profile());
  const CallStackProfile& profile = proto.call_stack_profile();

  ASSERT_EQ(2, profile.deprecated_sample_size());
  ASSERT_EQ(2, profile.deprecated_sample(0).frame_size());
  ASSERT_TRUE(profile.deprecated_sample(0).frame(0).has_module_id_index());
  EXPECT_EQ(0, profile.deprecated_sample(0).frame(0).module_id_index());
  ASSERT_TRUE(profile.deprecated_sample(0).frame(1).has_module_id_index());
  EXPECT_EQ(1, profile.deprecated_sample(0).frame(1).module_id_index());
  ASSERT_EQ(1, profile.deprecated_sample(1).frame_size());
  ASSERT_TRUE(profile.deprecated_sample(1).frame(0).has_module_id_index());
  EXPECT_EQ(2, profile.deprecated_sample(1).frame(0).module_id_index());

  ASSERT_EQ(3, profile.module_id().size());
  ASSERT_TRUE(profile.module_id(0).has_build_id());
  ASSERT_EQ("1", profile.module_id(0).build_id());
  ASSERT_TRUE(profile.module_id(0).has_name_md5_prefix());
  ASSERT_EQ(module_md5, profile.module_id(0).name_md5_prefix());
  ASSERT_TRUE(profile.module_id(1).has_build_id());
  ASSERT_EQ("2", profile.module_id(1).build_id());
  ASSERT_TRUE(profile.module_id(1).has_name_md5_prefix());
  ASSERT_EQ(module_md5, profile.module_id(1).name_md5_prefix());
  ASSERT_TRUE(profile.module_id(2).has_build_id());
  ASSERT_EQ("3", profile.module_id(2).build_id());
  ASSERT_TRUE(profile.module_id(2).has_name_md5_prefix());
  ASSERT_EQ(module_md5, profile.module_id(2).name_md5_prefix());

  ASSERT_TRUE(profile.has_profile_duration_ms());
  EXPECT_EQ(500, profile.profile_duration_ms());
  ASSERT_TRUE(profile.has_sampling_period_ms());
  EXPECT_EQ(100, profile.sampling_period_ms());
}

TEST(CallStackProfileBuilderTest, SamplesDeduped) {
  auto profile_builder =
      std::make_unique<TestingCallStackProfileBuilder>(kProfileParams);

#if defined(OS_WIN)
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  base::FilePath module_path("/some/path/to/chrome");
#endif

  const uintptr_t module_base_address1 = 0x1000;
  Module module1 = {module_base_address1, "1", module_path};
  Frame frame1 = {module_base_address1 + 0x10, module1};

  const uintptr_t module_base_address2 = 0x1100;
  Module module2 = {module_base_address2, "2", module_path};
  Frame frame2 = {module_base_address2 + 0x10, module2};

  std::vector<Frame> frames = {frame1, frame2};

  // Two samples are completed with the same frames. They also have the same
  // process milestone therefore they are deduped to one.
  CallStackProfileBuilder::SetProcessMilestone(0);

  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(frames);

  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(frames);

  profile_builder->OnProfileCompleted(base::TimeDelta(), base::TimeDelta());

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_process());
  ASSERT_EQ(BROWSER_PROCESS, proto.process());
  ASSERT_TRUE(proto.has_thread());
  ASSERT_EQ(MAIN_THREAD, proto.thread());
  ASSERT_TRUE(proto.has_trigger_event());
  ASSERT_EQ(SampledProfile::PROCESS_STARTUP, proto.trigger_event());

  ASSERT_TRUE(proto.has_call_stack_profile());
  ASSERT_EQ(1, proto.call_stack_profile().deprecated_sample_size());
}

TEST(CallStackProfileBuilderTest, SamplesNotDeduped) {
  auto profile_builder =
      std::make_unique<TestingCallStackProfileBuilder>(kProfileParams);

#if defined(OS_WIN)
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  base::FilePath module_path("/some/path/to/chrome");
#endif

  const uintptr_t module_base_address1 = 0x1000;
  Module module1 = {module_base_address1, "1", module_path};
  Frame frame1 = {module_base_address1 + 0x10, module1};

  const uintptr_t module_base_address2 = 0x1100;
  Module module2 = {module_base_address2, "2", module_path};
  Frame frame2 = {module_base_address2 + 0x10, module2};

  std::vector<Frame> frames = {frame1, frame2};

  // Two samples are completed with the same frames but different process
  // milestones. They are considered as different samples threfore not deduped.
  CallStackProfileBuilder::SetProcessMilestone(2);
  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(frames);

  CallStackProfileBuilder::SetProcessMilestone(4);
  profile_builder->RecordAnnotations();
  profile_builder->OnSampleCompleted(frames);

  profile_builder->OnProfileCompleted(base::TimeDelta(), base::TimeDelta());

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_process());
  ASSERT_EQ(BROWSER_PROCESS, proto.process());
  ASSERT_TRUE(proto.has_thread());
  ASSERT_EQ(MAIN_THREAD, proto.thread());
  ASSERT_TRUE(proto.has_trigger_event());
  ASSERT_EQ(SampledProfile::PROCESS_STARTUP, proto.trigger_event());

  ASSERT_TRUE(proto.has_call_stack_profile());
  ASSERT_EQ(2, proto.call_stack_profile().deprecated_sample_size());
}

TEST(CallStackProfileBuilderTest, Modules) {
  auto profile_builder =
      std::make_unique<TestingCallStackProfileBuilder>(kProfileParams);

  const uintptr_t module_base_address1 = 0x1000;
  Module module1;  // module1 has no information hence invalid.
  Frame frame1 = {module_base_address1 + 0x10, module1};

  const uintptr_t module_base_address2 = 0x1100;
#if defined(OS_WIN)
  uint64_t module_md5 = 0x46C3E4166659AC02ULL;
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  uint64_t module_md5 = 0x554838A8451AC36CULL;
  base::FilePath module_path("/some/path/to/chrome");
#endif
  Module module2 = {module_base_address2, "2", module_path};
  Frame frame2 = {module_base_address2 + 0x10, module2};

  std::vector<Frame> frames = {frame1, frame2};

  profile_builder->OnSampleCompleted(frames);
  profile_builder->OnProfileCompleted(base::TimeDelta(), base::TimeDelta());

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_call_stack_profile());
  const CallStackProfile& profile = proto.call_stack_profile();

  ASSERT_EQ(1, profile.deprecated_sample_size());
  ASSERT_EQ(2, profile.deprecated_sample(0).frame_size());

  ASSERT_FALSE(profile.deprecated_sample(0).frame(0).has_module_id_index());
  ASSERT_FALSE(profile.deprecated_sample(0).frame(0).has_address());

  ASSERT_TRUE(profile.deprecated_sample(0).frame(1).has_module_id_index());
  EXPECT_EQ(0, profile.deprecated_sample(0).frame(1).module_id_index());
  ASSERT_TRUE(profile.deprecated_sample(0).frame(1).has_address());
  EXPECT_EQ(0x10ULL, profile.deprecated_sample(0).frame(1).address());

  ASSERT_EQ(1, profile.module_id().size());
  ASSERT_TRUE(profile.module_id(0).has_build_id());
  ASSERT_EQ("2", profile.module_id(0).build_id());
  ASSERT_TRUE(profile.module_id(0).has_name_md5_prefix());
  ASSERT_EQ(module_md5, profile.module_id(0).name_md5_prefix());
}

TEST(CallStackProfileBuilderTest, DedupModules) {
  auto profile_builder =
      std::make_unique<TestingCallStackProfileBuilder>(kProfileParams);

  const uintptr_t module_base_address = 0x1000;

#if defined(OS_WIN)
  uint64_t module_md5 = 0x46C3E4166659AC02ULL;
  base::FilePath module_path(L"c:\\some\\path\\to\\chrome.exe");
#else
  uint64_t module_md5 = 0x554838A8451AC36CULL;
  base::FilePath module_path("/some/path/to/chrome");
#endif

  Module module1 = {module_base_address, "1", module_path};
  Frame frame1 = {module_base_address + 0x10, module1};

  Module module2 = {module_base_address, "1", module_path};
  Frame frame2 = {module_base_address + 0x20, module2};

  std::vector<Frame> frames = {frame1, frame2};

  profile_builder->OnSampleCompleted(frames);
  profile_builder->OnProfileCompleted(base::TimeDelta(), base::TimeDelta());

  const SampledProfile& proto = profile_builder->sampled_profile();

  ASSERT_TRUE(proto.has_call_stack_profile());
  const CallStackProfile& profile = proto.call_stack_profile();

  ASSERT_EQ(1, profile.deprecated_sample_size());
  ASSERT_EQ(2, profile.deprecated_sample(0).frame_size());

  // Since module1 and module2 have the same base address, they are considered
  // the same module and therefore deduped.
  ASSERT_TRUE(profile.deprecated_sample(0).frame(0).has_module_id_index());
  EXPECT_EQ(0, profile.deprecated_sample(0).frame(0).module_id_index());
  ASSERT_TRUE(profile.deprecated_sample(0).frame(0).has_address());
  EXPECT_EQ(0x10ULL, profile.deprecated_sample(0).frame(0).address());

  ASSERT_TRUE(profile.deprecated_sample(0).frame(1).has_module_id_index());
  EXPECT_EQ(0, profile.deprecated_sample(0).frame(1).module_id_index());
  ASSERT_TRUE(profile.deprecated_sample(0).frame(1).has_address());
  EXPECT_EQ(0x20ULL, profile.deprecated_sample(0).frame(1).address());

  ASSERT_EQ(1, profile.module_id().size());
  ASSERT_TRUE(profile.module_id(0).has_build_id());
  ASSERT_EQ("1", profile.module_id(0).build_id());
  ASSERT_TRUE(profile.module_id(0).has_name_md5_prefix());
  ASSERT_EQ(module_md5, profile.module_id(0).name_md5_prefix());
}

}  // namespace metrics
