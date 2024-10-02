// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "gpu/config/gpu_blocklist.h"
#include "gpu/config/gpu_driver_bug_list_autogen.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/software_rendering_list_autogen.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/re2/src/re2/re2.h"

namespace gpu {

class GpuBlocklistTest : public testing::Test {
 public:
  GpuBlocklistTest() = default;
  ~GpuBlocklistTest() override = default;

  const GPUInfo& gpu_info() const { return gpu_info_; }

  void RunFeatureTest(GpuFeatureType feature_type) {
    const int kFeatureListForEntry1[1] = {feature_type};
    const GpuControlList::Device kDevicesForEntry1[1] = {{0x0640, 0x0}};
    const GpuControlList::Entry kTestEntries[1] = {{
        1,                      // id
        "Test entry",           // description
        1,                      // features size
        kFeatureListForEntry1,  // features
        0,                      // DisabledExtensions size
        nullptr,                // DisabledExtensions
        0,                      // DisabledWebGLExtensions size
        nullptr,                // DisabledWebGLExtensions
        0,                      // CrBugs size
        nullptr,                // CrBugs
        {
            GpuControlList::kOsMacosx,  // os_type
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},                             // os_version
            0x10de,                                // vendor_id
            1,                                     // Devices size
            kDevicesForEntry1,                     // Devices
            GpuControlList::kMultiGpuCategoryAny,  // multi_gpu_category
            GpuControlList::kMultiGpuStyleNone,    // multi_gpu_style
            nullptr,                               // driver info
            nullptr,                               // GL strings
            nullptr,                               // machine model info
            0,                                     // gpu_series size
            nullptr,                               // gpu_series
            {GpuControlList::kUnknown, GpuControlList::kVersionStyleNumerical,
             GpuControlList::kVersionSchemaCommon, nullptr,
             nullptr},  // intel_gpu_generation
            nullptr,    // more conditions
        },
        0,        // exceptions count
        nullptr,  // exceptions
    }};
    GpuControlListData data(1, kTestEntries);
    std::unique_ptr<GpuBlocklist> blocklist = GpuBlocklist::Create(data);
    std::set<int> type =
        blocklist->MakeDecision(GpuBlocklist::kOsMacosx, "10.12.3", gpu_info());
    EXPECT_EQ(1u, type.size());
    EXPECT_EQ(1u, type.count(feature_type));
  }

 protected:
  void SetUp() override {
    gpu_info_.gpu.vendor_id = 0x10de;
    gpu_info_.gpu.device_id = 0x0640;
    gpu_info_.gpu.driver_vendor = "NVIDIA";
    gpu_info_.gpu.driver_version = "1.6.18";
    gpu_info_.machine_model_name = "MacBookPro";
    gpu_info_.machine_model_version = "7.1";
    gpu_info_.gl_vendor = "NVIDIA Corporation";
    gpu_info_.gl_renderer = "NVIDIA GeForce GT 120 OpenGL Engine";
  }

 private:
  GPUInfo gpu_info_;
};

#define GPU_BLOCKLIST_FEATURE_TEST(test_name, feature_type) \
  TEST_F(GpuBlocklistTest, test_name) { RunFeatureTest(feature_type); }

GPU_BLOCKLIST_FEATURE_TEST(Accelerated2DCanvas,
                           GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)

GPU_BLOCKLIST_FEATURE_TEST(AcceleratedWebGL, GPU_FEATURE_TYPE_ACCELERATED_WEBGL)

GPU_BLOCKLIST_FEATURE_TEST(AcceleratedVideoDecode,
                           GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE)

GPU_BLOCKLIST_FEATURE_TEST(AcceleratedVideoEncode,
                           GPU_FEATURE_TYPE_ACCELERATED_VIDEO_ENCODE)

GPU_BLOCKLIST_FEATURE_TEST(GpuRasterization, GPU_FEATURE_TYPE_GPU_RASTERIZATION)

GPU_BLOCKLIST_FEATURE_TEST(WebGL2, GPU_FEATURE_TYPE_ACCELERATED_WEBGL2)

GPU_BLOCKLIST_FEATURE_TEST(GL, GPU_FEATURE_TYPE_ACCELERATED_GL)

GPU_BLOCKLIST_FEATURE_TEST(Vulkan, GPU_FEATURE_TYPE_VULKAN)

GPU_BLOCKLIST_FEATURE_TEST(AcceleratedWebGPU,
                           GPU_FEATURE_TYPE_ACCELERATED_WEBGPU)

GPU_BLOCKLIST_FEATURE_TEST(SkiaGraphite, GPU_FEATURE_TYPE_SKIA_GRAPHITE)

// Test for invariant "Assume the newly last added entry has the largest ID".
// See GpuControlList::GpuControlList.
// It checks software_rendering_list.json
TEST_F(GpuBlocklistTest, TestBlocklistIsValid) {
  std::unique_ptr<GpuBlocklist> list(GpuBlocklist::Create());
  uint32_t max_entry_id = list->max_entry_id();

  std::vector<uint32_t> indices(list->num_entries());
  int current = 0;
  std::generate(indices.begin(), indices.end(),
                [&current]() { return current++; });

  auto entries = list->GetEntryIDsFromIndices(indices);
  auto real_max_entry_id = *std::max_element(entries.begin(), entries.end());
  EXPECT_EQ(real_max_entry_id, max_entry_id);
}

void TestBlockList(const GpuControlList::Entry* entries, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const auto& entry = entries[i];
    if (const auto* gl_strings = entry.conditions.gl_strings) {
      if (gl_strings->gl_vendor) {
        EXPECT_TRUE(RE2(gl_strings->gl_vendor).ok())
            << "gl_vendor=" << gl_strings->gl_vendor;
      }
      if (gl_strings->gl_renderer) {
        EXPECT_TRUE(RE2(gl_strings->gl_renderer).ok())
            << "gl_renderer=" << gl_strings->gl_renderer;
      }
      if (gl_strings->gl_extensions) {
        EXPECT_TRUE(RE2(gl_strings->gl_extensions).ok())
            << "gl_extensions=" << gl_strings->gl_extensions;
      }
      if (gl_strings->gl_version) {
        EXPECT_TRUE(RE2(gl_strings->gl_version).ok())
            << "gl_version=" << gl_strings->gl_version;
      }
    }
    for (size_t j = 0; j < entry.exception_size; ++j) {
      const auto& conditions = entry.exceptions[j];
      if (const auto* gl_strings = conditions.gl_strings) {
        if (gl_strings->gl_vendor) {
          EXPECT_TRUE(RE2(gl_strings->gl_vendor).ok())
              << "gl_vendor=" << gl_strings->gl_vendor;
        }
        if (gl_strings->gl_renderer) {
          EXPECT_TRUE(RE2(gl_strings->gl_renderer).ok())
              << "gl_renderer=" << gl_strings->gl_renderer;
        }
        if (gl_strings->gl_extensions) {
          EXPECT_TRUE(RE2(gl_strings->gl_extensions).ok())
              << "gl_extensions=" << gl_strings->gl_extensions;
        }
        if (gl_strings->gl_version) {
          EXPECT_TRUE(RE2(gl_strings->gl_version).ok())
              << "gl_version=" << gl_strings->gl_version;
        }
      }
    }
  }
}

// It checks software_rendering_list.json
TEST_F(GpuBlocklistTest, VerifyGLStrings) {
  TestBlockList(kSoftwareRenderingListEntries,
                kSoftwareRenderingListEntryCount);
  TestBlockList(kGpuDriverBugListEntries, kGpuDriverBugListEntryCount);
}

}  // namespace gpu
