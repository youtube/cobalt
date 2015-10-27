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
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          destination_(new AudioDestinationNode(this))),
      sample_rate_(0.0f),
      current_time_(0.0f) {}

scoped_refptr<AudioBufferSourceNode> AudioContext::CreateBufferSource() {
  return scoped_refptr<AudioBufferSourceNode>(new AudioBufferSourceNode(this));
}

void AudioContext::DecodeAudioData(
    const scoped_refptr<dom::ArrayBuffer>& /*audio_data*/,
    const DecodeSuccessCallbackArg& success_handler) {
  success_callback_.reset(
      new DecodeSuccessCallbackArg::Reference(this, success_handler));

  // TODO(***REMOVED***): Do the actual audio decoding asynchronously.
}

void AudioContext::DecodeAudioData(
    const scoped_refptr<dom::ArrayBuffer>& /*audio_data*/,
    const DecodeSuccessCallbackArg& success_handler,
    const DecodeErrorCallbackArg& error_handler) {
  success_callback_.reset(
      new DecodeSuccessCallbackArg::Reference(this, success_handler));
  error_callback_.reset(
      new DecodeErrorCallbackArg::Reference(this, error_handler));

  // TODO(***REMOVED***): Do the actual audio decoding asynchronously.
}

}  // namespace audio
}  // namespace cobalt
