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

#include <memory>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "starboard/memory.h"
#include "third_party/libjpeg-turbo/turbojpeg.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace jpeg_utils {

std::unique_ptr<uint8[]> EncodeRGBAToBuffer(const uint8_t* pixel_data,
                                            int width, int height,
                                            int pitch_in_bytes,
                                            size_t* out_size) {
  TRACE_EVENT0("cobalt::renderer", "jpegEncode::EncodeRGBAToBuffer()");
  unsigned char* jpeg_buffer = NULL;
  unsigned long jpegSize = 0;

  int flags = 0;
  // This can be a value between 1 and 100.
  int quality = 50;

  tjhandle tjInstance = tjInitCompress();
  if (tjInstance == nullptr) return std::unique_ptr<uint8[]>();

  int success = tjCompress2(tjInstance, pixel_data, width, pitch_in_bytes,
                            height, TJPF_RGBA, &jpeg_buffer, &jpegSize,
                            TJSAMP_444, quality, flags);

  tjDestroy(tjInstance);
  tjInstance = NULL;
  if (success < 0) return std::unique_ptr<uint8[]>();

  *out_size = jpegSize;

  // Copy the memory to return to the caller.
  // tjCompress2 allocates the data with malloc, and scoped_array deallocates
  // with delete, so the data has to be copied in.
  std::unique_ptr<uint8[]> out_buffer(new uint8[jpegSize]);
  memcpy(out_buffer.get(), &(jpeg_buffer[0]), jpegSize);
  SbMemoryDeallocate(jpeg_buffer);
  return std::move(out_buffer);
}

}  // namespace jpeg_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
