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

#include "media/base/shell_cached_decoder_buffer.h"

namespace media {

ShellCachedDecoderBuffer::ShellCachedDecoderBuffer(
    const scoped_refptr<media::DecoderBuffer>& source_buffer,
    void* destination,
    FreeCB free_cb)
    : media::DecoderBuffer(NULL, 0, source_buffer->IsKeyframe()),
      source_buffer_(source_buffer),
      free_cb_(free_cb) {
  DCHECK(source_buffer);
  DCHECK(destination);
  DCHECK(!free_cb.is_null());
  DCHECK(!source_buffer->IsEndOfStream());

  SetTimestamp(source_buffer->GetTimestamp());
  SetDuration(source_buffer->GetDuration());

  buffer_ = static_cast<uint8*>(destination);
  memcpy(buffer_, source_buffer_->GetData(), source_buffer_->GetDataSize());
  size_ = source_buffer_->GetDataSize();

  // The buffer is not expandable.
  allocated_size_ = source_buffer_->GetDataSize();
  is_decrypted_ = source_buffer_->IsAlreadyDecrypted();
}

ShellCachedDecoderBuffer::~ShellCachedDecoderBuffer() {
  free_cb_.Run(buffer_);
  // Set the buffer_ to NULL to stop the base class dtor from freeing it.
  buffer_ = NULL;
}

}  // namespace media
