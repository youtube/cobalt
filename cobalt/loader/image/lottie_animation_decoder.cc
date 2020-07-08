// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/loader/image/lottie_animation_decoder.h"

#include "base/trace_event/trace_event.h"
#include "nb/memory_scope.h"

namespace cobalt {
namespace loader {
namespace image {

LottieAnimationDecoder::LottieAnimationDecoder(
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks)
    : ImageDataDecoder(resource_provider, debugger_hooks) {
  TRACE_EVENT0("cobalt::loader::image",
               "LottieAnimationDecoder::LottieAnimationDecoder()");
  TRACK_MEMORY_SCOPE("Rendering");
}

size_t LottieAnimationDecoder::DecodeChunkInternal(const uint8* data,
                                                   size_t input_byte) {
  TRACE_EVENT0("cobalt::loader::image",
               "LottieAnimationDecoder::DecodeChunkInternal()");
  TRACK_MEMORY_SCOPE("Rendering");
  if (state() == kWaitingForHeader) {
    // TODO: Remove hard coded values.
    lottie_animation_ = new LottieAnimation(resource_provider());
    set_state(kReadLines);
  }

  if (state() == kReadLines) {
    DCHECK(lottie_animation_);
    lottie_animation_->AppendChunk(data, input_byte);
  }

  return input_byte;
}

scoped_refptr<Image> LottieAnimationDecoder::FinishInternal() {
  set_state(kDone);
  lottie_animation_->FinishReadingData();
  return lottie_animation_;
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt
