// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#if defined(STARBOARD)

#include "cobalt/loader/image/image_decoder_starboard.h"

#include <algorithm>

#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "starboard/decode_target.h"
#include "starboard/image.h"

namespace cobalt {
namespace loader {
namespace image {

ImageDecoderStarboard::ImageDecoderStarboard(
    render_tree::ResourceProvider* resource_provider,
    const base::DebuggerHooks& debugger_hooks, const char* mime_type,
    SbDecodeTargetFormat format)
    : ImageDataDecoder(resource_provider, debugger_hooks),
      mime_type_(mime_type),
      format_(format),
      provider_(resource_provider->GetSbDecodeTargetGraphicsContextProvider()),
      target_(kSbDecodeTargetInvalid) {
  TRACE_EVENT0("cobalt::loader::image",
               "ImageDecoderStarboard::ImageDecoderStarboard()");
}

ImageDecoderStarboard::~ImageDecoderStarboard() {}

size_t ImageDecoderStarboard::DecodeChunkInternal(const uint8* data,
                                                  size_t input_byte) {
  TRACE_EVENT0("cobalt::loader::image",
               "ImageDecoderStarboard::DecodeChunkInternal()");

  buffer_.insert(buffer_.end(), data, data + input_byte);
  return input_byte;
}

scoped_refptr<Image> ImageDecoderStarboard::FinishInternal() {
  TRACE_EVENT0("cobalt::loader::image",
               "ImageDecoderStarboard::FinishInternal()");
  DCHECK(!buffer_.empty());
  DCHECK(SbImageIsDecodeSupported(mime_type_, format_));
  target_ =
      SbImageDecode(provider_, &buffer_[0], static_cast<int>(buffer_.size()),
                    mime_type_, format_);
  if (SbDecodeTargetIsValid(target_)) {
    set_state(kDone);
    return new StaticImage(
        resource_provider()->CreateImageFromSbDecodeTarget(target_));
  }
  set_state(kError);
  return NULL;
}

}  // namespace image
}  // namespace loader
}  // namespace cobalt

#endif  // defined(STARBOARD)
