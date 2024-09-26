// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/buffer.h"

#include <GLES2/gl2extchromium.h>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "components/exo/frame_sink_resource_manager.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface_tree_host.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/test/surface_tree_host_test_util.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {
namespace {

class BufferTest : public test::ExoTestBase,
                   public testing::WithParamInterface<bool> {
 public:
  BufferTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(kExoReactiveFrameSubmission);
    } else {
      feature_list_.InitAndDisableFeature(kExoReactiveFrameSubmission);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

base::RepeatingClosure CreateReleaseBufferClosure(
    int* release_buffer_call_count,
    base::RepeatingClosure closure) {
  return base::BindLambdaForTesting(
      [release_buffer_call_count, closure = std::move(closure)]() {
        if (release_buffer_call_count) {
          (*release_buffer_call_count)++;
        }
        closure.Run();
      });
}

base::OnceCallback<void(gfx::GpuFenceHandle)> CreateExplicitReleaseCallback(
    int* release_call_count,
    base::RepeatingClosure closure) {
  return base::BindLambdaForTesting(
      [release_call_count,
       closure = std::move(closure)](gfx::GpuFenceHandle release_fence) {
        if (release_call_count) {
          (*release_call_count)++;
        }
        closure.Run();
      });
}

void VerifySyncTokensInCompositorFrame(viz::CompositorFrame* frame) {
  std::vector<GLbyte*> sync_tokens;
  for (auto& resource : frame->resource_list)
    sync_tokens.push_back(resource.mailbox_holder.sync_token.GetData());
  gpu::raster::RasterInterface* ri =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          ->RasterInterface();
  ri->VerifySyncTokensCHROMIUM(sync_tokens.data(), sync_tokens.size());
}

// Instantiate the values of disabling/enabling reactive frame submission in the
// parameterized tests.
INSTANTIATE_TEST_SUITE_P(All, BufferTest, testing::Values(false, true));

TEST_P(BufferTest, ReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  base::RunLoop run_loop_1;
  buffer->set_release_callback(CreateReleaseBufferClosure(
      &release_call_count, run_loop_1.QuitClosure()));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  base::RunLoop run_loop_2;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource,
      gfx::ColorSpace::CreateSRGB(), nullptr,
      CreateExplicitReleaseCallback(&release_resource_count,
                                    run_loop_2.QuitClosure()));
  ASSERT_TRUE(rv);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  run_loop_2.Run();

  ASSERT_EQ(release_call_count, 0);
  // The resource should have been released even if the whole buffer hasn't.
  ASSERT_EQ(release_resource_count, 1);

  buffer->OnDetach();

  run_loop_1.Run();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
}

TEST_P(BufferTest, SolidColorReleaseCallback) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<SolidColorBuffer>(SkColors::kRed, buffer_size);
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  // Set the release callback.
  int release_call_count = 0;
  base::RunLoop run_loop;
  buffer->set_release_callback(
      CreateReleaseBufferClosure(&release_call_count, run_loop.QuitClosure()));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource,
      gfx::ColorSpace::CreateSRGB(), nullptr,
      CreateExplicitReleaseCallback(&release_resource_count,
                                    base::DoNothing()));
  // Solid color buffer is immediately released after commit.
  EXPECT_EQ(release_resource_count, 1);
  EXPECT_FALSE(rv);

  // Release buffer.
  std::vector<viz::ReturnedResource> resources;
  resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                         /*release_fence=*/gfx::GpuFenceHandle(),
                         /*count=*/0, /*lost=*/false);
  frame_sink_holder->ReclaimResources(std::move(resources));

  // We expect that Release() is not called, no matter whether we have a wait
  // here or how long the wait is. An arbitrary time period is added here so
  // that if the event mistakenly happens, it is more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(release_call_count, 0);

  buffer->OnDetach();

  run_loop.Run();
  // Release() should have been called exactly once.
  EXPECT_EQ(release_call_count, 1);
}

TEST_P(BufferTest, IsLost) {
  gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  {
    // Acquire a texture transferable resource for the contents of the buffer.
    viz::TransferableResource resource;
    base::RunLoop run_loop_1;
    bool rv = buffer->ProduceTransferableResource(
        frame_sink_holder->resource_manager(), nullptr, false, &resource,
        gfx::ColorSpace::CreateSRGB(), nullptr,
        CreateExplicitReleaseCallback(nullptr, run_loop_1.QuitClosure()));
    ASSERT_TRUE(rv);

    scoped_refptr<viz::RasterContextProvider> context_provider =
        aura::Env::GetInstance()
            ->context_factory()
            ->SharedMainThreadRasterContextProvider();
    if (context_provider) {
      gpu::raster::RasterInterface* ri = context_provider->RasterInterface();
      ri->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                              GL_INNOCENT_CONTEXT_RESET_ARB);
    }

    // Release buffer.
    std::vector<viz::ReturnedResource> resources;
    resources.emplace_back(resource.id, gpu::SyncToken(),
                           /*release_fence=*/gfx::GpuFenceHandle(),
                           /*count=*/0, /*lost=*/true);
    frame_sink_holder->ReclaimResources(std::move(resources));
    run_loop_1.Run();
  }

  {
    // Producing a new texture transferable resource for the contents of the
    // buffer.
    viz::TransferableResource new_resource;
    base::RunLoop run_loop_2;
    bool rv = buffer->ProduceTransferableResource(
        frame_sink_holder->resource_manager(), nullptr, false, &new_resource,
        gfx::ColorSpace::CreateSRGB(), nullptr,
        CreateExplicitReleaseCallback(nullptr, run_loop_2.QuitClosure()));
    ASSERT_TRUE(rv);
    buffer->OnDetach();

    std::vector<viz::ReturnedResource> resources2;
    resources2.emplace_back(new_resource.id, gpu::SyncToken(),
                            /*release_fence=*/gfx::GpuFenceHandle(),
                            /*count=*/0, /*lost=*/false);
    frame_sink_holder->ReclaimResources(std::move(resources2));
    run_loop_2.Run();
  }
}

// Buffer::Texture::OnLostResources is called when the gpu crashes. This test
// verifies that the Texture is collected properly in such event.
TEST_P(BufferTest, OnLostResources) {
  // Create a Buffer and use it to produce a Texture.
  constexpr gfx::Size buffer_size(256, 256);
  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));
  auto surface_tree_host = std::make_unique<SurfaceTreeHost>("BufferTest");
  LayerTreeFrameSinkHolder* frame_sink_holder =
      surface_tree_host->layer_tree_frame_sink_holder();

  buffer->OnAttach();
  // Acquire a texture transferable resource for the contents of the buffer.
  viz::TransferableResource resource;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource,
      gfx::ColorSpace::CreateSRGB(), nullptr, base::DoNothing());
  ASSERT_TRUE(rv);

  viz::RasterContextProvider* context_provider =
      aura::Env::GetInstance()
          ->context_factory()
          ->SharedMainThreadRasterContextProvider()
          .get();
  static_cast<viz::TestInProcessContextProvider*>(context_provider)
      ->SendOnContextLost();
}

TEST_P(BufferTest, SurfaceTreeHostDestruction) {
  gfx::Size buffer_size(256, 256);

  // We need to setup shell surface and commit the surface, which properly
  // registers frame sink hierarchy and attaches begin frame source. Otherwise
  // OnBeginFrame requests won't be sent.
  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  test::WaitForLastFrameAck(shell_surface.get());

  LayerTreeFrameSinkHolder* frame_sink_holder =
      shell_surface->layer_tree_frame_sink_holder();

  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop;
  auto combined_quit_closure = BarrierClosure(2, run_loop.QuitClosure());

  buffer->set_release_callback(
      CreateReleaseBufferClosure(&release_call_count, combined_quit_closure));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource,
      gfx::ColorSpace::CreateSRGB(), nullptr,
      CreateExplicitReleaseCallback(&release_resource_count,
                                    combined_quit_closure));
  ASSERT_TRUE(rv);

  // Submit frame with resource.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id.source_id =
        viz::BeginFrameArgs::kManualSourceId;
    frame.metadata.begin_frame_ack.frame_id.sequence_number =
        viz::BeginFrameArgs::kStartingFrameNumber;
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = shell_surface->GenerateNextFrameToken();
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame.resource_list.push_back(resource);
    VerifySyncTokensInCompositorFrame(&frame);
    shell_surface->SubmitCompositorFrameForTesting(std::move(frame));
    test::WaitForLastFrameAck(shell_surface.get());
  }

  buffer->OnDetach();

  // We expect that the buffer and resource should not be released yet, no
  // matter whether we have a wait here or how long the wait is. An arbitrary
  // time period is added here so that if the event mistakenly happens, it is
  // more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));
  ASSERT_EQ(release_call_count, 0);
  ASSERT_EQ(release_resource_count, 0);

  shell_surface.reset();
  run_loop.Run();
  ASSERT_EQ(release_call_count, 1);
  ASSERT_EQ(release_resource_count, 1);
}

TEST_P(BufferTest, SurfaceTreeHostLastFrame) {
  gfx::Size buffer_size(256, 256);

  // We need to setup shell surface and commit the surface, which properly
  // registers frame sink hierarchy and attaches begin frame source. Otherwise
  // OnBeginFrame requests won't be sent.
  auto shell_surface =
      test::ShellSurfaceBuilder(buffer_size).BuildShellSurface();
  test::WaitForLastFrameAck(shell_surface.get());

  LayerTreeFrameSinkHolder* frame_sink_holder =
      shell_surface->layer_tree_frame_sink_holder();

  auto buffer = std::make_unique<Buffer>(
      exo_test_helper()->CreateGpuMemoryBuffer(buffer_size));

  // Remove wait time for efficiency.
  buffer->set_wait_for_release_delay_for_testing(base::TimeDelta());

  int release_call_count = 0;

  base::RunLoop run_loop;
  auto combined_quit_closure = BarrierClosure(2, run_loop.QuitClosure());

  buffer->set_release_callback(
      CreateReleaseBufferClosure(&release_call_count, combined_quit_closure));

  buffer->OnAttach();
  viz::TransferableResource resource;
  // Produce a transferable resource for the contents of the buffer.
  int release_resource_count = 0;
  bool rv = buffer->ProduceTransferableResource(
      frame_sink_holder->resource_manager(), nullptr, false, &resource,
      gfx::ColorSpace::CreateSRGB(), nullptr,
      CreateExplicitReleaseCallback(&release_resource_count,
                                    combined_quit_closure));
  ASSERT_TRUE(rv);

  // Submit frame with resource.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = shell_surface->GenerateNextFrameToken();
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    frame.resource_list.push_back(resource);
    VerifySyncTokensInCompositorFrame(&frame);
    shell_surface->SubmitCompositorFrameForTesting(std::move(frame));
    test::WaitForLastFrameAck(shell_surface.get());

    // Try to release buffer in last frame. This can happen during a resize
    // when frame sink id changes.
    std::vector<viz::ReturnedResource> resources;
    resources.emplace_back(resource.id, resource.mailbox_holder.sync_token,
                           /*release_fence=*/gfx::GpuFenceHandle(),
                           /*count=*/0, /*lost=*/false);
    frame_sink_holder->ReclaimResources(std::move(resources));
  }

  buffer->OnDetach();

  // We expect that the buffer and resource should not be released yet, no
  // matter whether we have a wait here or how long the wait is. An arbitrary
  // time period is added here so that if the event mistakenly happens, it is
  // more likely to find out.
  task_environment()->FastForwardBy(base::Seconds(1));
  // Release() should not have been called as resource is used by last frame.
  ASSERT_EQ(release_call_count, 0);
  ASSERT_EQ(release_resource_count, 0);

  // Submit frame without resource. This should cause buffer to be released.
  {
    viz::CompositorFrame frame;
    frame.metadata.begin_frame_ack.frame_id =
        viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                          viz::BeginFrameArgs::kStartingFrameNumber);
    frame.metadata.begin_frame_ack.has_damage = true;
    frame.metadata.frame_token = shell_surface->GenerateNextFrameToken();
    frame.metadata.device_scale_factor = 1;
    auto pass = viz::CompositorRenderPass::Create();
    pass->SetNew(viz::CompositorRenderPassId{1}, gfx::Rect(buffer_size),
                 gfx::Rect(buffer_size), gfx::Transform());
    frame.render_pass_list.push_back(std::move(pass));
    shell_surface->SubmitCompositorFrameForTesting(std::move(frame));
  }

  run_loop.Run();
  // Release() should have been called exactly once.
  ASSERT_EQ(release_call_count, 1);
  ASSERT_EQ(release_resource_count, 1);
}

}  // namespace
}  // namespace exo
