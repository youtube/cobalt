// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_FACTORY_H_
#define MEDIA_BASE_DECODER_BUFFER_FACTORY_H_

#include <stddef.h>

#include "media/base/media_export.h"
#include "media/base/decoder_buffer.h"

namespace media {

class MEDIA_EXPORT DecoderBufferFactory {
 public:
  DecoderBufferFactory();
  DecoderBufferFactory(const DecoderBufferFactory&) = delete;
  DecoderBufferFactory& operator=(const DecoderBufferFactory&) = delete;
  ~DecoderBufferFactory();

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
  class FactoryImpl;
  scoped_refptr<FactoryImpl> factory_;
};

struct AllocatedExternalMemory
    : public media::DecoderBuffer::ExternalMemory {
 public:
  AllocatedExternalMemory(uint8_t* data, size_t data_size, DecoderBufferFactory::Allocator *alloc)
      : ExternalMemory(base::make_span(data, data_size)),  alloc_{alloc}, data_{data}, data_size_{data_size} {}

  ~AllocatedExternalMemory() override {
    alloc_->Free(data_, data_size_);
  }
 private:
  DecoderBufferFactory::Allocator *alloc_;
  uint8_t * data_;
  size_t data_size_;
};


}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_FACTORY_H_
