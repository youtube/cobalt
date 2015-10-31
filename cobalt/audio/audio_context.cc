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
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler) {
  scoped_refptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(this, success_handler));
  AsyncAudioDecoder::DecodeFinishCallback decode_callback =
      base::Bind(&AudioContext::DecodeFinish, base::Unretained(this), info);
  audio_decoder_.AsyncDecode(audio_data, decode_callback);
}

void AudioContext::DecodeAudioData(
    const scoped_refptr<dom::ArrayBuffer>& audio_data,
    const DecodeSuccessCallbackArg& success_handler,
    const DecodeErrorCallbackArg& error_handler) {
  scoped_refptr<DecodeCallbackInfo> info(
      new DecodeCallbackInfo(this, success_handler, error_handler));
  AsyncAudioDecoder::DecodeFinishCallback decode_callback =
      base::Bind(&AudioContext::DecodeFinish, base::Unretained(this), info);
  audio_decoder_.AsyncDecode(audio_data, decode_callback);
}

// Success callback and error callback should be scheduled to run on the main
// thread's event loop.
void AudioContext::DecodeFinish(const scoped_refptr<DecodeCallbackInfo>& info,
                                const scoped_refptr<AudioBuffer>& audio_data) {
  if (!main_message_loop_->BelongsToCurrentThread()) {
    main_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&AudioContext::DecodeFinish, this, info, audio_data));
    return;
  }

  DCHECK(info);
  if (audio_data) {
    info->success_callback.value().Run(audio_data);
  } else if (info->error_callback) {
    info->error_callback.value().value().Run();
  }
}

}  // namespace audio
}  // namespace cobalt
