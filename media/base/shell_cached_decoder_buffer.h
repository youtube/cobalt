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

#ifndef MEDIA_SHELL_CACHED_DECODER_BUFFER_H_
#define MEDIA_SHELL_CACHED_DECODER_BUFFER_H_

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "media/base/decoder_buffer.h"

namespace media {

// This class can cache the content of |source_buffer| into main memory.  So it
// is possible to store DecoderBuffer in memory space that cannot be accessed by
// the decoder and copy them over to main memory just before they are decoded.
class ShellCachedDecoderBuffer : public media::DecoderBuffer {
 public:
  // Callback to free memory passed in.
  typedef base::Callback<void(void*)> FreeCB;

  ShellCachedDecoderBuffer(
      const scoped_refptr<media::DecoderBuffer>& source_buffer,
      void* destination,
      FreeCB free_cb);
  ~ShellCachedDecoderBuffer();

  const media::DecryptConfig* GetDecryptConfig() const override {
    return source_buffer_->GetDecryptConfig();
  }
  void SetDecryptConfig(scoped_ptr<media::DecryptConfig>) override {
    NOTREACHED();
  }

 private:
  scoped_refptr<media::DecoderBuffer> source_buffer_;
  FreeCB free_cb_;
};

}  // namespace media

#endif  // MEDIA_SHELL_CACHED_DECODER_BUFFER_H_
