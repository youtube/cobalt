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

#include "media/base/decoder_buffer_pool.h"

#include "base/logging.h"
#include "media/base/shell_buffer_factory.h"

namespace media {

DecoderBufferPool::DecoderBufferPool(uint32 sample_size_in_bytes) {
  uint32 buffer_size = kMaxSamplesPerBuffer * sample_size_in_bytes;
  while (decoder_buffers_.size() < kBufferCount) {
    decoder_buffers_.push_back(AllocateFromShellBufferFactory(buffer_size));
    DCHECK(decoder_buffers_.back());
  }
}

scoped_refptr<DecoderBuffer> DecoderBufferPool::Allocate(size_t size_in_bytes) {
  for (DecoderBuffers::iterator iter = decoder_buffers_.begin();
       iter != decoder_buffers_.end(); ++iter) {
    if ((*iter)->HasOneRef()) {
      DCHECK_LE(size_in_bytes, (*iter)->GetAllocatedSize());
      if (size_in_bytes <= (*iter)->GetAllocatedSize()) {
        (*iter)->ShrinkTo(size_in_bytes);
        return *iter;
      }
      break;
    }
  }
  NOTREACHED();
  return AllocateFromShellBufferFactory(size_in_bytes);
}

scoped_refptr<DecoderBuffer> DecoderBufferPool::AllocateFromShellBufferFactory(
    size_t size_in_bytes) {
  return ShellBufferFactory::Instance()->AllocateBufferNow(size_in_bytes,
                                                           false);
}

}  // namespace media
