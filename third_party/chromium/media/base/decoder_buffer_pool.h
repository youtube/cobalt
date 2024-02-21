// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_POOL_H_
#define MEDIA_BASE_DECODER_BUFFER_POOL_H_

#include <stddef.h>

#include "media/base/media_export.h"
#include "media/base/decoder_buffer.h"

namespace base {
class TickClock;
}

namespace media {

class MEDIA_EXPORT DecoderBufferPool {
 public:
  DecoderBufferPool();
  DecoderBufferPool(const DecoderBufferPool&) = delete;
  DecoderBufferPool& operator=(const DecoderBufferPool&) = delete;
  ~DecoderBufferPool();

  scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size);

  class Allocator {
   public:
    static Allocator* GetInstance();

    virtual void* Allocate(size_t size, size_t alignment) = 0;
    virtual void Free(void* p, size_t size) = 0;
    virtual int GetBufferAlignment() const = 0;
    virtual int GetBufferPadding() const  = 0;

    protected:
     ~Allocator() {}

     static void Set(Allocator* allocator);
  };

  static bool HasAllocator();

 private:
  class PoolImpl;
  scoped_refptr<PoolImpl> pool_;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_POOL_H_
