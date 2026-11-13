// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_list.h"

#include <vector>

#include "base/command_line.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class GpuDriverBugListTest : public testing::Test {
 public:
  GpuDriverBugListTest() = default;
  ~GpuDriverBugListTest() override = default;
};

#if BUILDFLAG(IS_ANDROID)
TEST_F(GpuDriverBugListTest, CurrentListForARM) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "MALi_T604";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

TEST_F(GpuDriverBugListTest, CurrentListForImagination) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "Imagination Technologies";
  gpu_info.gl_renderer = "PowerVR SGX 540";
  gpu_info.gl_version = "OpenGL ES 2.0";
  std::set<int> bugs = list->MakeDecision(
      GpuControlList::kOsAndroid, "4.1", gpu_info);
  EXPECT_EQ(1u, bugs.count(USE_CLIENT_SIDE_ARRAYS_FOR_STREAM_BUFFERS));
}

// NVIDIA SHIELD (Tegra) must not use partial swaps on onscreen surfaces, both
// with the native GLES driver and with ANGLE's GLES backend (32-bit SHIELD
// builds always use ANGLE + the passthrough command decoder). b/576231761
TEST_F(GpuDriverBugListTest, NvidiaShieldDisablesPostSubBuffers) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();

  GPUInfo native_info;
  native_info.gl_vendor = "NVIDIA Corporation";
  native_info.gl_renderer = "NVIDIA Tegra X1 (nvgpu)/integrated";
  native_info.gl_version = "OpenGL ES 3.2 NVIDIA 32.5.0";
  std::set<int> bugs =
      list->MakeDecision(GpuControlList::kOsAndroid, "11", native_info);
  EXPECT_EQ(1u, bugs.count(DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES));

  GPUInfo angle_info;
  angle_info.gl_vendor = "Google Inc. (NVIDIA Corporation)";
  angle_info.gl_renderer =
      "ANGLE (NVIDIA Corporation, NVIDIA Tegra X1 (nvgpu)/integrated, "
      "OpenGL ES 3.2 NVIDIA 32.5.0)";
  angle_info.gl_version = "OpenGL ES 2.0.0 (ANGLE 2.1.0)";
  bugs = list->MakeDecision(GpuControlList::kOsAndroid, "11", angle_info);
  EXPECT_EQ(1u, bugs.count(DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES));
}

TEST_F(GpuDriverBugListTest, NonNvidiaAndroidKeepsPostSubBuffers) {
  std::unique_ptr<GpuDriverBugList> list = GpuDriverBugList::Create();
  GPUInfo gpu_info;
  gpu_info.gl_vendor = "ARM";
  gpu_info.gl_renderer = "Mali-G31";
  gpu_info.gl_version = "OpenGL ES 3.2 v1.r20p0";
  std::set<int> bugs =
      list->MakeDecision(GpuControlList::kOsAndroid, "11", gpu_info);
  EXPECT_EQ(0u, bugs.count(DISABLE_POST_SUB_BUFFERS_FOR_ONSCREEN_SURFACES));
}
#endif  // BUILDFLAG(IS_ANDROID)

TEST_F(GpuDriverBugListTest, AppendSingleWorkaround) {
  base::CommandLine command_line(0, nullptr);
  command_line.AppendSwitch(GpuDriverBugWorkaroundTypeToString(
      DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE));
  std::set<int> workarounds;
  workarounds.insert(EXIT_ON_CONTEXT_LOST);
  workarounds.insert(GL_CLEAR_BROKEN);
  EXPECT_EQ(2u, workarounds.size());
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, command_line);
  EXPECT_EQ(3u, workarounds.size());
  EXPECT_EQ(1u, workarounds.count(DISABLE_CHROMIUM_FRAMEBUFFER_MULTISAMPLE));
}

TEST_F(GpuDriverBugListTest, AppendForceGPUWorkaround) {
  base::CommandLine command_line(0, nullptr);
  command_line.AppendSwitch(
      GpuDriverBugWorkaroundTypeToString(FORCE_HIGH_PERFORMANCE_GPU));
  std::set<int> workarounds;
  workarounds.insert(EXIT_ON_CONTEXT_LOST);
  workarounds.insert(FORCE_LOW_POWER_GPU);
  EXPECT_EQ(2u, workarounds.size());
  EXPECT_EQ(1u, workarounds.count(FORCE_LOW_POWER_GPU));
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, command_line);
  EXPECT_EQ(2u, workarounds.size());
  EXPECT_EQ(0u, workarounds.count(FORCE_LOW_POWER_GPU));
  EXPECT_EQ(1u, workarounds.count(FORCE_HIGH_PERFORMANCE_GPU));
}

// Test for invariant "Assume the newly last added entry has the largest ID".
// See GpuControlList::GpuControlList.
// It checks gpu_driver_bug_list.json
TEST_F(GpuDriverBugListTest, TestBlocklistIsValid) {
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  auto max_entry_id = list->max_entry_id();

  std::vector<uint32_t> indices(list->num_entries());
  int current = 0;
  std::generate(indices.begin(), indices.end(),
                [&current] () { return current++; });

  auto entries = list->GetEntryIDsFromIndices(indices);
  auto real_max_entry_id = *std::max_element(entries.begin(), entries.end());
  EXPECT_EQ(real_max_entry_id, max_entry_id);
}

}  // namespace gpu
