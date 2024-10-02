// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_sender_sink_optimizer.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

RtcEncodedVideoSenderSinkOptimizer::RtcEncodedVideoSenderSinkOptimizer(
    UnderlyingSinkSetter set_underlying_sink,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker> transformer)
    : set_underlying_sink_(std::move(set_underlying_sink)),
      transformer_(std::move(transformer)) {}

UnderlyingSinkBase*
RtcEncodedVideoSenderSinkOptimizer::PerformInProcessOptimization(
    ScriptState* script_state) {
  auto* new_sink = MakeGarbageCollected<RTCEncodedVideoUnderlyingSink>(
      script_state, std::move(transformer_),
      webrtc::TransformableFrameInterface::Direction::kSender);

  std::move(set_underlying_sink_).Run(WrapCrossThreadPersistent(new_sink));

  return new_sink;
}

}  // namespace blink
