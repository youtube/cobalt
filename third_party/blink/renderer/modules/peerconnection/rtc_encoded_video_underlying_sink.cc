// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_underlying_sink.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/peerconnection/rtc_encoded_video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_encoded_video_stream_transformer.h"
#include "third_party/webrtc/api/frame_transformer_interface.h"

namespace blink {

using webrtc::TransformableFrameInterface;

RTCEncodedVideoUnderlyingSink::RTCEncodedVideoUnderlyingSink(
    ScriptState* script_state,
    scoped_refptr<blink::RTCEncodedVideoStreamTransformer::Broker>
        transformer_broker)
    : transformer_broker_(std::move(transformer_broker)) {
  DCHECK(transformer_broker_);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::start(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    ExceptionState&) {
  // No extra setup needed.
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::write(
    ScriptState* script_state,
    ScriptValue chunk,
    WritableStreamDefaultController* controller,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RTCEncodedVideoFrame* encoded_frame = V8RTCEncodedVideoFrame::ToWrappable(
      script_state->GetIsolate(), chunk.V8Value());
  if (!encoded_frame) {
    exception_state.ThrowTypeError("Invalid frame");
    return ScriptPromise();
  }

  if (!transformer_broker_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Stream closed");
    return ScriptPromise();
  }

  auto webrtc_frame = encoded_frame->PassWebRtcFrame();
  if (!webrtc_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Empty frame");
    return ScriptPromise();
  }

  transformer_broker_->SendFrameToSink(std::move(webrtc_frame));
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::close(ScriptState* script_state,
                                                   ExceptionState&) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Disconnect from the transformer if the sink is closed.
  if (transformer_broker_)
    transformer_broker_.reset();
  return ScriptPromise::CastUndefined(script_state);
}

ScriptPromise RTCEncodedVideoUnderlyingSink::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // It is not possible to cancel any frames already sent to the WebRTC sink,
  // thus abort() has the same effect as close().
  return close(script_state, exception_state);
}

void RTCEncodedVideoUnderlyingSink::Trace(Visitor* visitor) const {
  UnderlyingSinkBase::Trace(visitor);
}

}  // namespace blink
