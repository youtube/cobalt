// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/raster_implementation.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "gpu/ipc/in_process_gpu_thread_holder.h"
#include "gpu/ipc/raster_in_process_context.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

namespace gpu {

namespace {

constexpr viz::SharedImageFormat kSharedImageFormat =
    viz::SinglePlaneFormat::kRGBA_8888;
constexpr gfx::Size kBufferSize(100, 100);

class RasterInProcessCommandBufferTest : public ::testing::Test {
 public:
  RasterInProcessCommandBufferTest() {
    // Always enable gpu and oop raster, regardless of platform and blocklist.
    auto* gpu_feature_info = gpu_thread_holder_.GetGpuFeatureInfo();
    gpu_feature_info->status_values[gpu::GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
        gpu::kGpuFeatureStatusEnabled;
  }

  std::unique_ptr<RasterInProcessContext> CreateRasterInProcessContext() {
    if (!RasterInProcessContext::SupportedInTest())
      return nullptr;

    ContextCreationAttribs attributes;
    attributes.bind_generates_resource = false;
    attributes.enable_oop_rasterization = true;
    attributes.enable_gles2_interface = false;
    attributes.enable_raster_interface = true;

    auto context = std::make_unique<RasterInProcessContext>();
    auto result = context->Initialize(
        gpu_thread_holder_.GetTaskExecutor(), attributes, SharedMemoryLimits(),
        /*gr_shader_cache=*/nullptr, /*activity_flags=*/nullptr);
    DCHECK_EQ(result, ContextResult::kSuccess);
    return context;
  }

  void SetUp() override {
    if (!RasterInProcessContext::SupportedInTest())
      return;
    gpu_thread_holder_.GetGpuPreferences()->texture_target_exception_list =
        CreateBufferUsageAndFormatExceptionList();
    context_ = CreateRasterInProcessContext();
    ri_ = context_->GetImplementation();
  }

  void TearDown() override { context_.reset(); }

 protected:
  InProcessGpuThreadHolder gpu_thread_holder_;
  raw_ptr<raster::RasterInterface> ri_;  // not owned
  std::unique_ptr<RasterInProcessContext> context_;
};

}  // namespace

TEST_F(RasterInProcessCommandBufferTest, AllowedBetweenBeginEndRasterCHROMIUM) {
  if (!RasterInProcessContext::SupportedInTest())
    return;

  // Check for GPU and driver support
  if (!context_->GetCapabilities().supports_oop_raster) {
    return;
  }

  // Create shared image and allocate storage.
  auto* sii = context_->GetSharedImageInterface();
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t flags = gpu::SHARED_IMAGE_USAGE_RASTER |
                   gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION;
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      kSharedImageFormat, kBufferSize, color_space, kTopLeft_GrSurfaceOrigin,
      kPremul_SkAlphaType, flags, "TestLabel", kNullSurfaceHandle);
  ri_->WaitSyncTokenCHROMIUM(sii->GenUnverifiedSyncToken().GetConstData());

  // Call BeginRasterCHROMIUM.
  ri_->BeginRasterCHROMIUM(
      /*sk_color_4f=*/{0, 0, 0, 0}, /*needs_clear=*/true,
      /*msaa_sample_count=*/0, gpu::raster::kNoMSAA,
      /*can_use_lcd_text=*/false, /*visible=*/true, color_space, mailbox.name);
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());

  // Should flag an error this command is not allowed between a Begin and
  // EndRasterCHROMIUM.
  GLuint id;
  ri_->GenQueriesEXT(1, &id);
  EXPECT_EQ(static_cast<GLenum>(GL_INVALID_OPERATION), ri_->GetError());

  // Confirm that we skip over without error.
  ri_->EndRasterCHROMIUM();
  EXPECT_EQ(static_cast<GLenum>(GL_NO_ERROR), ri_->GetError());
}

}  // namespace gpu
