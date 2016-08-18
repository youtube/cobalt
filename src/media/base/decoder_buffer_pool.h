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

#ifndef MEDIA_BASE_DECODER_BUFFER_POOL_H_
#define MEDIA_BASE_DECODER_BUFFER_POOL_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/decoder_buffer.h"
#include "media/mp4/aac.h"

namespace media {

// This class is currently used by classes inherited from ShellRawAudioDecoder.
// These classes may need to allocate DecoderBuffer from ShellBufferFactory
// during playback.  Our progressive demuxer will try to use up all free space
// of ShellBufferFactory so the allocation made by ShellRawAudioDecoder may
// fail.  This class can pre-allocate DecoderBuffer on playback start to ensure
// that the raw audio decoder can always have buffer to use.
class DecoderBufferPool {
 public:
  static const uint32 kMaxAudioChannels = 8;  // We support 7.1 at most.
  static const uint32 kMaxSamplesPerBuffer =
      mp4::AAC::kFramesPerAccessUnit * kMaxAudioChannels;
  static const size_t kBufferCount = 48;

  DecoderBufferPool(uint32 sample_size_in_bytes);
  scoped_refptr<DecoderBuffer> Allocate(size_t size_in_bytes);

 private:
  typedef std::vector<scoped_refptr<DecoderBuffer> > DecoderBuffers;

  scoped_refptr<DecoderBuffer> AllocateFromShellBufferFactory(
      size_t size_in_bytes);

  DecoderBuffers decoder_buffers_;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_POOL_H_
