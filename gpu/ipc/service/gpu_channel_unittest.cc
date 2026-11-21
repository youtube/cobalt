// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/gpu_channel.h"

#include <stdint.h>

#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_test_common.h"

#if BUILDFLAG(IS_COBALT)
#include "base/test/scoped_feature_list.h"
#include "gpu/config/gpu_finch_features.h"
#endif  // BUILDFLAG(IS_COBALT)

namespace gpu {

class GpuChannelTest : public GpuChannelTestCommon {
 public:
  GpuChannelTest() : GpuChannelTestCommon(true /* use_stub_bindings */) {}
  ~GpuChannelTest() override = default;
};

TEST_F(GpuChannelTest, CreateOffscreenCommandBuffer) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId);
  EXPECT_TRUE(stub);
}

TEST_F(GpuChannelTest, IncompatibleStreamIds) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  // Create first context.
  int32_t kRouteId1 =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  int32_t kStreamId1 = 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = kStreamId1;
  init_params->stream_priority = SchedulingPriority::kNormal;
  auto init_params2 = init_params.Clone();

  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId1,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kSuccess);

  CommandBufferStub* stub = channel->LookupCommandBuffer(kRouteId1);
  EXPECT_TRUE(stub);

  // Create second context in same share group but different stream.
  int32_t kRouteId2 = kRouteId1 + 1;
  int32_t kStreamId2 = 2;

  init_params2->share_group_id = kRouteId1;
  init_params2->stream_id = kStreamId2;
  init_params2->stream_priority = SchedulingPriority::kNormal;
  CreateCommandBuffer(*channel, std::move(init_params2), kRouteId2,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kFatalFailure);

  stub = channel->LookupCommandBuffer(kRouteId2);
  EXPECT_FALSE(stub);
}

TEST_F(GpuChannelTest, CreateFailsIfSharedContextIsLost) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Create first context, we will share this one.
  int32_t kSharedRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  {
    SCOPED_TRACE("kSharedRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = MSG_ROUTING_NONE;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kSharedRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kSuccess);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // This context shares with the first one, this should be possible.
  int32_t kFriendlyRouteId = kSharedRouteId + 1;
  {
    SCOPED_TRACE("kFriendlyRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = kSharedRouteId;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kFriendlyRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kSuccess);
  }
  EXPECT_TRUE(channel->LookupCommandBuffer(kFriendlyRouteId));

  // The shared context is lost.
  channel->LookupCommandBuffer(kSharedRouteId)->MarkContextLost();

  // Meanwhile another context is being made pointing to the shared one. This
  // should fail.
  int32_t kAnotherRouteId = kFriendlyRouteId + 1;
  {
    SCOPED_TRACE("kAnotherRouteId");
    auto init_params = mojom::CreateCommandBufferParams::New();
    init_params->share_group_id = kSharedRouteId;
    init_params->stream_id = 0;
    init_params->stream_priority = SchedulingPriority::kNormal;
    init_params->attribs = ContextCreationAttribs();
    init_params->active_url = GURL();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(init_params), kAnotherRouteId,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  }
  EXPECT_FALSE(channel->LookupCommandBuffer(kAnotherRouteId));

  // The lost context is still around though (to verify the failure happened due
  // to the shared context being lost, not due to it being deleted).
  EXPECT_TRUE(channel->LookupCommandBuffer(kSharedRouteId));

  // Destroy the command buffers we initialized before destoying GL.
  channel->DestroyCommandBuffer(kFriendlyRouteId);
  channel->DestroyCommandBuffer(kSharedRouteId);
}

class GpuChannelExitForContextLostTest : public GpuChannelTestCommon {
 public:
  GpuChannelExitForContextLostTest()
      : GpuChannelTestCommon({EXIT_ON_CONTEXT_LOST} /* enabled_workarounds */,
                             true /* use_stub_bindings */) {}
};

TEST_F(GpuChannelExitForContextLostTest,
       CreateFailsDuringLostContextShutdown_1) {
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Put channel manager into shutdown state.
  channel_manager()->OnContextLost(-1 /* context_lost_count */,
                                   false /* synthetic_loss */,
                                   error::ContextLostReason::kUnknown);

  // Calling OnContextLost() above may destroy the gpu channel via post task.
  // Ensure that post task has happened.
  base::RunLoop().RunUntilIdle();

  // If the channel is destroyed, then skip the test.
  if (!channel_manager()->LookupChannel(kClientId))
    return;

  // Try to create a context.
  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  EXPECT_FALSE(channel->LookupCommandBuffer(kRouteId));
}

TEST_F(GpuChannelExitForContextLostTest,
       CreateFailsDuringLostContextShutdown_2) {
  // Put channel manager into shutdown state. Do this before creating a channel,
  // as doing this may destroy any active channels.
  channel_manager()->OnContextLost(-1 /* context_lost_count */,
                                   false /* synthetic_loss */,
                                   error::ContextLostReason::kUnknown);

  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, false);
  ASSERT_TRUE(channel);

  // Try to create a context.
  int32_t kRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;
  auto init_params = mojom::CreateCommandBufferParams::New();
  init_params->share_group_id = MSG_ROUTING_NONE;
  init_params->stream_id = 0;
  init_params->stream_priority = SchedulingPriority::kNormal;
  init_params->attribs = ContextCreationAttribs();
  init_params->active_url = GURL();
  gpu::ContextResult result = gpu::ContextResult::kSuccess;
  gpu::Capabilities capabilities;
  gpu::GLCapabilities gl_capabilities;
  CreateCommandBuffer(*channel, std::move(init_params), kRouteId,
                      GetSharedMemoryRegion(), &result, &capabilities,
                      &gl_capabilities);
  EXPECT_EQ(result, gpu::ContextResult::kTransientFailure);
  EXPECT_FALSE(channel->LookupCommandBuffer(kRouteId));
}

#if BUILDFLAG(IS_COBALT)
TEST_F(GpuChannelTest, CobaltRasterCommandBufferWaitCapsNormalPriority) {
  base::test::ScopedFeatureList enabled_feature{
      features::kCobaltCapRasterWaitPriority};
  int32_t kClientId = 1;
  GpuChannel* channel = CreateChannel(kClientId, true);
  ASSERT_TRUE(channel);

  const int32_t kBaseRouteId =
      static_cast<int32_t>(GpuChannelReservedRoutes::kMaxValue) + 1;

  auto make_raster_params = [](int32_t stream_id, SchedulingPriority priority) {
    auto params = mojom::CreateCommandBufferParams::New();
    params->share_group_id = MSG_ROUTING_NONE;
    params->stream_id = stream_id;
    params->stream_priority = priority;
    params->attribs = ContextCreationAttribs();
    params->attribs.bind_generates_resource = false;
    params->attribs.enable_raster_interface = true;
    params->attribs.enable_gles2_interface = false;
    params->attribs.enable_grcontext = false;
    return params;
  };

  // 1. Background/normal-priority RasterCommandBufferStub stays at kNormal.
  {
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel,
                        make_raster_params(1, SchedulingPriority::kNormal),
                        kBaseRouteId, GetSharedMemoryRegion(), &result,
                        &capabilities, &gl_capabilities);
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    CommandBufferStub* raster_stub = channel->LookupCommandBuffer(kBaseRouteId);
    ASSERT_TRUE(raster_stub);
    EXPECT_TRUE(raster_stub->IsRasterCommandBuffer());
    EXPECT_EQ(SchedulingPriority::kNormal,
              raster_stub->GetWaitSchedulingPriorityForTesting());

    // Disabling the Finch killswitch restores upstream kHigh boost.
    base::test::ScopedFeatureList disabled_feature;
    disabled_feature.InitAndDisableFeature(
        features::kCobaltCapRasterWaitPriority);
    EXPECT_EQ(SchedulingPriority::kHigh,
              raster_stub->GetWaitSchedulingPriorityForTesting());
  }

  // 2. High-priority RasterCommandBufferStub (e.g. media stream) retains kHigh.
  {
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel,
                        make_raster_params(2, SchedulingPriority::kHigh),
                        kBaseRouteId + 1, GetSharedMemoryRegion(), &result,
                        &capabilities, &gl_capabilities);
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    CommandBufferStub* high_raster_stub =
        channel->LookupCommandBuffer(kBaseRouteId + 1);
    ASSERT_TRUE(high_raster_stub);
    EXPECT_TRUE(high_raster_stub->IsRasterCommandBuffer());
    EXPECT_EQ(SchedulingPriority::kHigh,
              high_raster_stub->GetWaitSchedulingPriorityForTesting());
  }

  // 3. GLES2CommandBufferStub retains upstream kHigh boost.
  {
    auto params = mojom::CreateCommandBufferParams::New();
    params->share_group_id = MSG_ROUTING_NONE;
    params->stream_id = 3;
    params->stream_priority = SchedulingPriority::kNormal;
    params->attribs = ContextCreationAttribs();
    gpu::ContextResult result = gpu::ContextResult::kSuccess;
    gpu::Capabilities capabilities;
    gpu::GLCapabilities gl_capabilities;
    CreateCommandBuffer(*channel, std::move(params), kBaseRouteId + 2,
                        GetSharedMemoryRegion(), &result, &capabilities,
                        &gl_capabilities);
    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
    CommandBufferStub* gles2_stub =
        channel->LookupCommandBuffer(kBaseRouteId + 2);
    ASSERT_TRUE(gles2_stub);
    EXPECT_FALSE(gles2_stub->IsRasterCommandBuffer());
    EXPECT_EQ(SchedulingPriority::kHigh,
              gles2_stub->GetWaitSchedulingPriorityForTesting());
  }

  channel->DestroyCommandBuffer(kBaseRouteId + 2);
  channel->DestroyCommandBuffer(kBaseRouteId + 1);
  channel->DestroyCommandBuffer(kBaseRouteId);
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace gpu
