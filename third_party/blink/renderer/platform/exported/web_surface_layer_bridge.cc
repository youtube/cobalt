// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_surface_layer_bridge.h"

#include <memory>
#include "third_party/blink/renderer/platform/graphics/surface_layer_bridge.h"

namespace blink {

std::unique_ptr<WebSurfaceLayerBridge> WebSurfaceLayerBridge::Create(
    viz::FrameSinkId parent_frame_sink_id,
    ContainsVideo contains_video,
    WebSurfaceLayerBridgeObserver* observer,
    cc::UpdateSubmissionStateCB update_submission_state_callback) {
  return std::make_unique<SurfaceLayerBridge>(
      parent_frame_sink_id, contains_video, observer,
      std::move(update_submission_state_callback));
}

WebSurfaceLayerBridge::~WebSurfaceLayerBridge() = default;

}  // namespace blink
