// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/audio/audio_context.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/dom/dom_settings.h"

namespace cobalt {
namespace audio {

AudioContext::AudioContext(script::EnvironmentSettings* settings)
    : dom::EventTarget(settings),
      global_environment_(
          base::polymorphic_downcast<dom::DOMSettings*>(settings)
              ->global_environment()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      sample_rate_(
          static_cast<float>(SbAudioSinkGetNearestSupportedSampleFrequency(
              kStandardOutputSampleRate))),
      current_time_(0.0f),
      audio_lock_(new AudioLock()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          destination_(new AudioDestinationNode(settings, this))),
      next_callback_id_(0),
      main_message_loop_(base::MessageLoop::current()->task_runner()) {
  DCHECK(main_message_loop_);
}

AudioContext::~AudioContext() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  // Before releasing any |pending_decode_callbacks_|, stop audio decoder
  // explicitly to ensure that all the decoding works are done.
  audio_decoder_.Stop();

  // It is possible that the callbacks in |pending_decode_callbacks_| have not
  // been called when destroying AudioContext.
  for (DecodeCallbacks::iterator it = pending_decode_callbacks_.begin();
       it != pending_decode_callbacks_.end(); ++it) {
    delete it->second;
  }
}

scoped_refptr<AudioBuffer> AudioContext::CreateBuffer(uint32 num_of_channels,
                                                      uint32 length,
                                                      float sample_rate) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  return scoped_refptr<AudioBuffer>(new AudioBuffer(
      sample_rate, std::unique_ptr<AudioBus>(new AudioBus(
                       num_of_channels, length, GetPreferredOutputSampleType(),
                       kStorageTypeInterleaved))));
}

scoped_refptr<AudioBufferSourceNode> AudioContext::CreateBufferSource(
    script::EnvironmentSettings* settings) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  return scoped_refptr<AudioBufferSourceNode>(
      new AudioBufferSourceNode(settings, this));
}

void AudioContext::PreventGarbageCollection() {
  prevent_gc_until_playback_complete_.reset(
      new script::GlobalEnvironment::ScopedPreventGarbageCollection(
          global_environment_, this));
}

void AudioContext::AllowGarbageCollection() {
  prevent_gc_until_playback_complete_.reset();
}

void AudioContext::AddBufferSource(
    const scoped_refptr<AudioBufferSourceNode>& buffer_source) {
  buffer_sources_.insert(buffer_source);
}

void AudioContext::RemoveBufferSource(
    const scoped_refptr<AudioBufferSourceNode>& buffer_source) {
  if (buffer_sources_.find(buffer_source) != buffer_sources_.end()) {
    buffer_sources_.erase(buffer_source);
  }
}

void AudioContext::TraceMembers(script::Tracer* tracer) {
  dom::EventTarget::TraceMembers(tracer);

  tracer->Trace(destination_.get());
  for (const auto& source : buffer_sources_) {
    tracer->Trace(source);
  }
}

void AudioContext::DecodeAudioData(
    const script::Handle<script::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  std::unique_ptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(audio_data, this, success_handler));
  DecodeAudioDataInternal(std::move(info));
}

void AudioContext::DecodeAudioData(
    const script::Handle<script::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler,
    const DecodeErrorCallbackArg& error_handler) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  std::unique_ptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(audio_data, this, success_handler, error_handler));
  DecodeAudioDataInternal(std::move(info));
}

void AudioContext::DecodeAudioDataInternal(
    std::unique_ptr<DecodeCallbackInfo> info) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  const int callback_id = next_callback_id_++;
  CHECK(pending_decode_callbacks_.find(callback_id) ==
        pending_decode_callbacks_.end());
  const std::string& audio_data = info->audio_data;
  pending_decode_callbacks_[callback_id] = info.release();

  AsyncAudioDecoder::DecodeFinishCallback decode_callback = base::Bind(
      &AudioContext::DecodeFinish, base::Unretained(this), callback_id);
  audio_decoder_.AsyncDecode(reinterpret_cast<const uint8*>(audio_data.data()),
                             audio_data.size(), decode_callback);
}

// Success callback and error callback should be scheduled to run on the main
// thread's event loop.
void AudioContext::DecodeFinish(int callback_id, float sample_rate,
                                std::unique_ptr<AudioBus> audio_bus) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AudioContext::DecodeFinish, weak_this_, callback_id,
                   sample_rate, base::Passed(&audio_bus)));
    return;
  }

  DecodeCallbacks::iterator info_iterator =
      pending_decode_callbacks_.find(callback_id);
  DCHECK(info_iterator != pending_decode_callbacks_.end());

  std::unique_ptr<DecodeCallbackInfo> info(info_iterator->second);
  pending_decode_callbacks_.erase(info_iterator);

  if (audio_bus) {
    const scoped_refptr<AudioBuffer>& audio_buffer =
        new AudioBuffer(sample_rate, std::move(audio_bus));
    info->success_callback.value().Run(audio_buffer);
  } else if (info->error_callback) {
    info->error_callback.value().value().Run();
  }
}

}  // namespace audio
}  // namespace cobalt
