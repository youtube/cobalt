/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/async_audio_decoder.h"

#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/bindings/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

void AsyncAudioDecoder::DecodeAsync(DOMArrayBuffer* audio_data,
                                    float sample_rate,
                                    V8DecodeSuccessCallback* success_callback,
                                    V8DecodeErrorCallback* error_callback,
                                    ScriptPromiseResolver* resolver,
                                    BaseAudioContext* context,
                                    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      context->GetExecutionContext()->GetTaskRunner(
          blink::TaskType::kInternalMedia);

  worker_pool::PostTask(
      FROM_HERE,
      CrossThreadBindOnce(
          &AsyncAudioDecoder::DecodeOnBackgroundThread,
          WrapCrossThreadPersistent(audio_data), sample_rate,
          MakeCrossThreadHandle(success_callback),
          MakeCrossThreadHandle(error_callback),
          MakeCrossThreadHandle(resolver), WrapCrossThreadPersistent(context),
          std::move(task_runner), exception_state.GetContext()));
}

void AsyncAudioDecoder::DecodeOnBackgroundThread(
    DOMArrayBuffer* audio_data,
    float sample_rate,
    CrossThreadHandle<V8DecodeSuccessCallback> success_callback,
    CrossThreadHandle<V8DecodeErrorCallback> error_callback,
    CrossThreadHandle<ScriptPromiseResolver> resolver,
    BaseAudioContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const ExceptionContext& exception_context) {
  DCHECK(!IsMainThread());
  scoped_refptr<AudioBus> bus = AudioBus::CreateBusFromInMemoryAudioFile(
      audio_data->Data(), audio_data->ByteLength(), false, sample_rate);

  // Decoding is finished, but we need to do the callbacks on the main thread.
  // A reference to `bus` is retained by base::OnceCallback and will be removed
  // after `NotifyComplete()` is done.
  //
  // We also want to avoid notifying the main thread if AudioContext does not
  // exist any more.
  if (context) {
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&AsyncAudioDecoder::NotifyComplete,
                            WrapCrossThreadPersistent(audio_data),
                            MakeUnwrappingCrossThreadHandle(success_callback),
                            MakeUnwrappingCrossThreadHandle(error_callback),
                            WTF::RetainedRef(std::move(bus)),
                            MakeUnwrappingCrossThreadHandle(resolver),
                            WrapCrossThreadPersistent(context),
                            exception_context));
  }
}

void AsyncAudioDecoder::NotifyComplete(
    DOMArrayBuffer*,
    V8DecodeSuccessCallback* success_callback,
    V8DecodeErrorCallback* error_callback,
    AudioBus* audio_bus,
    ScriptPromiseResolver* resolver,
    BaseAudioContext* context,
    const ExceptionContext& exception_context) {
  DCHECK(IsMainThread());

  AudioBuffer* audio_buffer = AudioBuffer::CreateFromAudioBus(audio_bus);

  // If the context is available, let the context finish the notification.
  if (context) {
    context->HandleDecodeAudioData(audio_buffer, resolver, success_callback,
                                   error_callback, exception_context);
  }
}

}  // namespace blink
