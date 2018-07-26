// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/test/jpeg_utils/jpeg_encode.h"

#include <vector>

#include "base/debug/trace_event.h"
#include "starboard/memory.h"
#include "third_party/libjpeg-turbo/turbojpeg.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace jpeg_utils {

scoped_array<uint8> EncodeRGBAToBuffer(const uint8_t* pixel_data, int width,
                                       int height, int pitch_in_bytes,
                                       size_t* out_size) {
  TRACE_EVENT0("cobalt::renderer", "jpegEncode::EncodeRGBAToBuffer()");
  unsigned char* jpeg_buffer = NULL;
  unsigned long jpegSize = 0;

  int flags = 0;
  // This can be a value between 1 and 100.
  int quality = 50;

  tjhandle tjInstance = tjInitCompress();
  if (tjInstance == nullptr) return scoped_array<uint8>();

  int success = tjCompress2(tjInstance, pixel_data, width, pitch_in_bytes,
                            height, TJPF_RGBA, &jpeg_buffer, &jpegSize,
                            TJSAMP_444, quality, flags);

  tjDestroy(tjInstance);
  tjInstance = NULL;
  if (success < 0) return scoped_array<uint8>();

  *out_size = jpegSize;

  // Copy the memory from the buffer to a scoped_array to return to the caller.
  return scoped_array<uint8>(jpeg_buffer);
}

}  // namespace jpeg_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
