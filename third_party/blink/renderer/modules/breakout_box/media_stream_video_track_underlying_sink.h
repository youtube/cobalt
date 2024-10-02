// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/underlying_sink_base.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_video_source.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace gpu {
class GpuMemoryBufferManager;
}  // namespace gpu

namespace blink {

class WebGraphicsContext3DVideoFramePool;
class WritableStreamTransferringOptimizer;

class MODULES_EXPORT MediaStreamVideoTrackUnderlyingSink
    : public UnderlyingSinkBase {
 public:
  explicit MediaStreamVideoTrackUnderlyingSink(
      scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker);

  ~MediaStreamVideoTrackUnderlyingSink() override;

  // UnderlyingSinkBase overrides.
  ScriptPromise start(ScriptState* script_state,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise write(ScriptState* script_state,
                      ScriptValue chunk,
                      WritableStreamDefaultController* controller,
                      ExceptionState& exception_state) override;
  ScriptPromise abort(ScriptState* script_state,
                      ScriptValue reason,
                      ExceptionState& exception_state) override;
  ScriptPromise close(ScriptState* script_state,
                      ExceptionState& exception_state) override;

  std::unique_ptr<WritableStreamTransferringOptimizer>
  GetTransferringOptimizer();

 private:
  void Disconnect();
  // Handles callback from main thread when the GpuMemoryBufferManager is
  // available (also called synchronously if on the main thread).
  void CreateAcceleratedFramePool(gpu::GpuMemoryBufferManager* gmb_manager);
  // Try to convert to an NV12 GpuMemoryBuffer-backed frame if the encoder
  // prefers that format and the BreakoutBoxEagerConversion feature is enabled.
  // Likely to return nullopt for the first few frames, as encoder feedback may
  // not have arrived yet, and initializing the
  // WebGraphicsContext3DVideoFramePool may require a round-trip to the main
  // thread.
  absl::optional<ScriptPromise> MaybeConvertToNV12GMBVideoFrame(
      ScriptState* script_state,
      scoped_refptr<media::VideoFrame> video_frame,
      base::TimeTicks estimated_capture_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Handles callback from WebGraphicsContext3DVideoFramePool::Convert.
  void ConvertDone(Persistent<ScriptPromiseResolver> resolver,
                   scoped_refptr<media::VideoFrame> orig_video_frame,
                   base::TimeTicks estimated_capture_time,
                   scoped_refptr<media::VideoFrame> converted_video_frame);

  const scoped_refptr<PushableMediaStreamVideoSource::Broker> source_broker_;
  bool is_connected_ = false;

  std::unique_ptr<WebGraphicsContext3DVideoFramePool> accelerated_frame_pool_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool accelerated_frame_pool_callback_in_progress_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int convert_to_nv12_gmb_failure_count_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BREAKOUT_BOX_MEDIA_STREAM_VIDEO_TRACK_UNDERLYING_SINK_H_
