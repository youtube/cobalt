// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/callback.h"

namespace cobalt {
namespace audio {

AudioContext::AudioContext()
    : ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          weak_this_(weak_ptr_factory_.GetWeakPtr())),
      sample_rate_(0.0f),
      current_time_(0.0f),
      audio_lock_(new AudioLock()),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          destination_(new AudioDestinationNode(this))),
      next_callback_id_(0),
      main_message_loop_(base::MessageLoopProxy::current()) {
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

scoped_refptr<AudioBufferSourceNode> AudioContext::CreateBufferSource() {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  return scoped_refptr<AudioBufferSourceNode>(new AudioBufferSourceNode(this));
}

void AudioContext::TraceMembers(script::Tracer* tracer) {
  dom::EventTarget::TraceMembers(tracer);

  tracer->Trace(destination_);
}

void AudioContext::DecodeAudioData(
    script::EnvironmentSettings* settings,
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  scoped_ptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(settings, audio_data, this, success_handler));
  DecodeAudioDataInternal(info.Pass());
}

void AudioContext::DecodeAudioData(
    script::EnvironmentSettings* settings,
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler,
    const DecodeErrorCallbackArg& error_handler) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  scoped_ptr<DecodeCallbackInfo> info(new DecodeCallbackInfo(
      settings, audio_data, this, success_handler, error_handler));
  DecodeAudioDataInternal(info.Pass());
}

void AudioContext::DecodeAudioDataInternal(
    scoped_ptr<DecodeCallbackInfo> info) {
  DCHECK(main_message_loop_->BelongsToCurrentThread());

  const int callback_id = next_callback_id_++;
  CHECK(pending_decode_callbacks_.find(callback_id) ==
        pending_decode_callbacks_.end());
  const scoped_refptr<dom::ArrayBuffer>& audio_data = info->audio_data;
  pending_decode_callbacks_[callback_id] = info.release();

  AsyncAudioDecoder::DecodeFinishCallback decode_callback = base::Bind(
      &AudioContext::DecodeFinish, base::Unretained(this), callback_id);
  audio_decoder_.AsyncDecode(audio_data->data(), audio_data->byte_length(),
                             decode_callback);
}

// Success callback and error callback should be scheduled to run on the main
// thread's event loop.
void AudioContext::DecodeFinish(int callback_id, float sample_rate,
                                int32 number_of_frames,
                                int32 number_of_channels,
                                scoped_array<uint8> channels_data,
                                SampleType sample_type) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AudioContext::DecodeFinish, weak_this_, callback_id,
                   sample_rate, number_of_frames, number_of_channels,
                   base::Passed(&channels_data), sample_type));
    return;
  }

  DecodeCallbacks::iterator info_iterator =
      pending_decode_callbacks_.find(callback_id);
  DCHECK(info_iterator != pending_decode_callbacks_.end());

  scoped_ptr<DecodeCallbackInfo> info(info_iterator->second);
  pending_decode_callbacks_.erase(info_iterator);

  if (channels_data) {
    const scoped_refptr<AudioBuffer>& audio_buffer =
        new AudioBuffer(info->env_settings, sample_rate, number_of_frames,
                        number_of_channels, channels_data.Pass(), sample_type);
    info->success_callback.value().Run(audio_buffer);
  } else if (info->error_callback) {
    info->error_callback.value().value().Run();
  }
}

}  // namespace audio
}  // namespace cobalt
