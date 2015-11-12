/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/audio/audio_context.h"

#include "base/callback.h"

namespace cobalt {
namespace audio {

AudioContext::AudioContext()
    : sample_rate_(0.0f),
      current_time_(0.0f),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          destination_(new AudioDestinationNode(this))),
      main_message_loop_(base::MessageLoopProxy::current()) {
  DCHECK(main_message_loop_);
}

scoped_refptr<AudioBufferSourceNode> AudioContext::CreateBufferSource() {
  return scoped_refptr<AudioBufferSourceNode>(new AudioBufferSourceNode(this));
}

void AudioContext::DecodeAudioData(
    script::EnvironmentSettings* settings,
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler) {
  scoped_refptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(settings, this, success_handler));
  AsyncAudioDecoder::DecodeFinishCallback decode_callback =
      base::Bind(&AudioContext::DecodeFinish, base::Unretained(this), info);
  audio_decoder_.AsyncDecode(audio_data->data(), audio_data->byte_length(),
                             decode_callback);
}

void AudioContext::DecodeAudioData(
    script::EnvironmentSettings* settings,
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler,
    const DecodeErrorCallbackArg& error_handler) {
  scoped_refptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(settings, this, success_handler, error_handler));
  AsyncAudioDecoder::DecodeFinishCallback decode_callback =
      base::Bind(&AudioContext::DecodeFinish, base::Unretained(this), info);
  audio_decoder_.AsyncDecode(audio_data->data(), audio_data->byte_length(),
                             decode_callback);
}

// Success callback and error callback should be scheduled to run on the main
// thread's event loop.
void AudioContext::DecodeFinish(const scoped_refptr<DecodeCallbackInfo>& info,
                                float sample_rate, int32 number_of_frames,
                                int32 number_of_channels,
                                scoped_array<uint8> channels_data) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE, base::Bind(&AudioContext::DecodeFinish, this, info,
                              sample_rate, number_of_frames, number_of_channels,
                              base::Passed(&channels_data)));
    return;
  }

  DCHECK(info);
  if (channels_data) {
    const scoped_refptr<AudioBuffer>& audio_buffer =
        new AudioBuffer(info->env_settings, sample_rate, number_of_frames,
                        number_of_channels, channels_data.Pass());
    info->success_callback.value().Run(audio_buffer);
  } else if (info->error_callback) {
    info->error_callback.value().value().Run();
  }
}

}  // namespace audio
}  // namespace cobalt
