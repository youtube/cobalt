// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_

#include <stdint.h>
#include <memory>

#include "media/base/decoder_status.h"
#include "media/base/media_log.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_codec_state.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_webcodecs_error_callback.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_logger.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_trace_names.h"
#include "third_party/blink/renderer/modules/webcodecs/hardware_preference.h"
#include "third_party/blink/renderer/modules/webcodecs/reclaimable_codec.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {
class GpuVideoAcceleratorFactories;
class ScopedDecodeTrace;
}

namespace blink {

template <typename Traits>
class MODULES_EXPORT DecoderTemplate
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<DecoderTemplate<Traits>>,
      public ReclaimableCodec {
 public:
  typedef typename Traits::ConfigType ConfigType;
  typedef typename Traits::MediaConfigType MediaConfigType;
  typedef typename Traits::InputType InputType;
  typedef typename Traits::InitType InitType;
  typedef typename Traits::MediaDecoderType MediaDecoderType;
  typedef typename Traits::MediaOutputType MediaOutputType;
  typedef typename Traits::OutputType OutputType;
  typedef typename Traits::OutputCallbackType OutputCallbackType;

  static const CodecTraceNames* GetTraceNames();

  DecoderTemplate(ScriptState*, const InitType*, ExceptionState&);
  ~DecoderTemplate() override;

  uint32_t decodeQueueSize();
  DEFINE_ATTRIBUTE_EVENT_LISTENER(dequeue, kDequeue)
  void configure(const ConfigType*, ExceptionState&);
  void decode(const InputType*, ExceptionState&);
  ScriptPromise flush(ExceptionState&);
  void reset(ExceptionState&);
  void close(ExceptionState&);
  String state() const { return state_; }

  // EventTarget override.
  ExecutionContext* GetExecutionContext() const override;

  // ExecutionContextLifecycleObserver override.
  void ContextDestroyed() override;

  // ScriptWrappable override.
  bool HasPendingActivity() const override;

  // GarbageCollected override.
  void Trace(Visitor*) const override;

 protected:
  virtual bool IsValidConfig(const ConfigType& config,
                             String* js_error_message) = 0;

  // Convert a configuration to a DecoderConfig. Returns absl::nullopt if the
  // configuration is not supported.
  virtual absl::optional<MediaConfigType> MakeMediaConfig(
      const ConfigType& config,
      String* js_error_message) = 0;

  // Gets the AccelerationPreference from a config.
  // If derived classes do not override this, this will default to kAllow.
  virtual HardwarePreference GetHardwarePreference(const ConfigType& config);

  // Get the low delay preference from a config.
  // If derived classes do not override this, this will default to false.
  virtual bool GetLowDelayPreference(const ConfigType& config);

  // Sets the HardwarePreference on the |decoder_|.
  // The default implementation does nothing and must be overridden by derived
  // classes if needed.
  virtual void SetHardwarePreference(HardwarePreference preference);

  // Virtual for UTs.
  virtual MediaDecoderType* decoder() { return decoder_.get(); }

  // Convert a chunk to a DecoderBuffer. You can assume that the last
  // configuration sent to MakeMediaConfig() is the active configuration for
  // |chunk|. If there is an error in the conversion process, the resulting
  // DecoderBuffer will be null, and |out_status| will contain a description of
  // the error.
  //
  // When |verify_key_frame| is true, clients are expected to verify and set the
  // DecoderBuffer::is_key_frame() value. I.e., they must process the encoded
  // data to ensure the value is actually what the chunk says it is.
  virtual media::DecoderStatus::Or<scoped_refptr<media::DecoderBuffer>>
  MakeInput(const InputType& chunk, bool verify_key_frame) = 0;

  // Convert an output to the WebCodecs type.
  virtual media::DecoderStatus::Or<OutputType*> MakeOutput(
      scoped_refptr<MediaOutputType> output,
      ExecutionContext* context) = 0;

 private:
  struct Request final : public GarbageCollected<Request> {
    enum class Type {
      kConfigure,
      kDecode,
      kFlush,
      kReset,
    };

    void Trace(Visitor*) const;

    // Starts an async trace event.
    void StartTracing();

    // Ends the async trace event associated with |this|.
    void EndTracing(bool shutting_down = false);

    // Get a trace event name from DecoderTemplate::GetTraceNames() and |type|.
    const char* TraceNameFromType();

    Type type;

    // For kConfigure Requests. Prefer absl::optional<> to ensure values are
    // only accessed on the proper request type.
    std::unique_ptr<MediaConfigType> media_config;
    absl::optional<HardwarePreference> hw_pref;
    absl::optional<bool> low_delay;

    // For kDecode Requests.
    scoped_refptr<media::DecoderBuffer> decoder_buffer;

    // For kFlush Requests.
    Member<ScriptPromiseResolver> resolver;

    // For reporting an error at the time when a request is processed.
    media::DecoderStatus status;

    // The value of |reset_generation_| at the time of this request. Used to
    // abort pending requests following a reset().
    uint32_t reset_generation = 0;

    // Used for tracing kDecode requests.
    std::unique_ptr<media::ScopedDecodeTrace> decode_trace;

#if DCHECK_IS_ON()
    // Tracks the state of tracing for debug purposes.
    bool is_tracing;
#endif
  };

  void ProcessRequests();
  bool ProcessConfigureRequest(Request* request);
  void ContinueConfigureWithGpuFactories(
      Request* request,
      media::GpuVideoAcceleratorFactories* factories);
  bool ProcessDecodeRequest(Request* request);
  bool ProcessFlushRequest(Request* request);
  bool ProcessResetRequest(Request* request);
  void ResetAlgorithm();
  void Shutdown(DOMException* ex = nullptr);

  // Called by |decoder_|.
  void OnInitializeDone(media::DecoderStatus status);
  void OnDecodeDone(uint32_t id, media::DecoderStatus);
  void OnFlushDone(media::DecoderStatus);
  void OnResetDone();
  void OnOutput(uint32_t reset_generation, scoped_refptr<MediaOutputType>);

  // Helper function making it easier to check |state_|.
  bool IsClosed();

  // ReclaimableCodec implementation.
  void OnCodecReclaimed(DOMException*) override;

  void TraceQueueSizes() const;

  void ScheduleDequeueEvent();
  void DispatchDequeueEvent(Event* event);
  bool dequeue_event_pending_ = false;

  Member<ScriptState> script_state_;
  Member<OutputCallbackType> output_cb_;
  Member<V8WebCodecsErrorCallback> error_cb_;

  HeapDeque<Member<Request>> requests_;
  uint32_t num_pending_decodes_ = 0;

  // Monotonic increasing generation counter for calls to ResetAlgorithm().
  uint32_t reset_generation_ = 0;

  // Set on Shutdown(), used to generate accurate abort messages.
  bool shutting_down_ = false;
  bool shutting_down_due_to_error_ = false;

  // Which state the codec is in, determining which calls we can receive.
  V8CodecState state_;

  // An in-flight, mutually-exclusive request.
  // Could be a configure, flush, or reset. Decodes go in |pending_decodes_|.
  Member<Request> pending_request_;

  std::unique_ptr<CodecLogger<media::DecoderStatus>> logger_;

  // Empty - GPU factories haven't been retrieved yet.
  // nullptr - We tried to get GPU factories, but acceleration is unavailable.
  absl::optional<media::GpuVideoAcceleratorFactories*> gpu_factories_;

  // Cached config from the last kConfigure request which successfully completed
  // initialization.
  bool low_delay_ = false;
  std::unique_ptr<MediaConfigType> active_config_;

  // TODO(sandersd): Store the last config, flush, and reset so that
  // duplicates can be elided.
  std::unique_ptr<MediaDecoderType> decoder_;
  bool initializing_sync_ = false;

  // TODO(sandersd): Can this just be a HashSet by ptr comparison?
  uint32_t pending_decode_id_ = 0;

  // Used to differentiate Decoders' counters during tracing.
  int trace_counter_id_;

  HeapHashMap<uint32_t, Member<Request>> pending_decodes_;

  // Keyframes are required after configure(), flush(), and reset().
  bool require_key_frame_ = true;

  // Task runner for main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_DECODER_TEMPLATE_H_
