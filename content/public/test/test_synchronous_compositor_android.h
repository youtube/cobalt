// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
#define CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "content/common/content_export.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/android/synchronous_compositor_client.h"

namespace content {

class CONTENT_EXPORT TestSynchronousCompositor : public SynchronousCompositor {
 public:
  explicit TestSynchronousCompositor(const viz::FrameSinkId& frame_sink_id);

  TestSynchronousCompositor(const TestSynchronousCompositor&) = delete;
  TestSynchronousCompositor& operator=(const TestSynchronousCompositor&) =
      delete;

  ~TestSynchronousCompositor() override;

  void SetClient(SynchronousCompositorClient* client);

  scoped_refptr<FrameFuture> DemandDrawHwAsync(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) override;
  void ReturnResources(uint32_t layer_tree_frame_sink_id,
                       std::vector<viz::ReturnedResource> resources) override;
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t layer_tree_frame_sink_id,
      uint32_t sequence_id) override {}
  void DidPresentCompositorFrames(viz::FrameTimingDetailsMap timing_details,
                                  uint32_t frame_token) override {}
  bool DemandDrawSw(SkCanvas* canvas, bool software_canvas) override;
  void SetMemoryPolicy(size_t bytes_limit) override {}
  void DidBecomeActive() override {}
  void DidChangeRootLayerScrollOffset(const gfx::PointF& root_offset) override {
  }
  void SynchronouslyZoomBy(float zoom_delta,
                           const gfx::Point& anchor) override {}
  void OnComputeScroll(base::TimeTicks animate_time) override {}
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void DidInvalidate() override {}
  void WasEvicted() override {}

  void SetHardwareFrame(uint32_t layer_tree_frame_sink_id,
                        std::unique_ptr<viz::CompositorFrame> frame);

  struct ReturnedResources {
    ReturnedResources();
    ReturnedResources(ReturnedResources&&);
    ~ReturnedResources();

    uint32_t layer_tree_frame_sink_id;
    std::vector<viz::ReturnedResource> resources;
  };
  using FrameAckArray = std::vector<ReturnedResources>;
  void SwapReturnedResources(FrameAckArray* array);

 private:
  raw_ptr<SynchronousCompositorClient> client_;
  viz::FrameSinkId frame_sink_id_;
  std::unique_ptr<Frame> hardware_frame_;
  FrameAckArray frame_ack_array_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_SYNCHRONOUS_COMPOSITOR_ANDROID_H_
