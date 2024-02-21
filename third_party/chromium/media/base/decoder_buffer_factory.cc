// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer_factory.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace media {

namespace {
DecoderBufferFactory::Allocator* s_allocator = nullptr;
}  // namespace

// static
DecoderBufferFactory::Allocator* DecoderBufferFactory::Allocator::GetInstance() {
  DCHECK(s_allocator);
  return s_allocator;
}

// static
void DecoderBufferFactory::Allocator::Set(Allocator* allocator) {
  s_allocator = allocator;
}

// static
bool DecoderBufferFactory::HasAllocator() {
  if (s_allocator) return true;
  return false;
}

class DecoderBufferFactory::FactoryImpl
    : public base::RefCountedThreadSafe<DecoderBufferFactory::FactoryImpl> {
 public:
  FactoryImpl() = default;
  FactoryImpl(const FactoryImpl&) = delete;
  FactoryImpl& operator=(const FactoryImpl&) = delete;

  scoped_refptr<DecoderBuffer> CopyFrom(const uint8_t* data,
                                               size_t size);

 private:
  friend class base::RefCountedThreadSafe<DecoderBufferFactory::FactoryImpl>;
  ~FactoryImpl() = default;

  base::Lock lock_;

};


scoped_refptr<DecoderBuffer> DecoderBufferFactory::FactoryImpl::CopyFrom(const uint8_t* data,
                                               size_t size) {
  base::AutoLock auto_lock(lock_);

  if (s_allocator)  {
    int alignment = s_allocator->GetBufferAlignment();
    int padding = s_allocator->GetBufferPadding();
    size_t allocated_size = size + padding;
    auto data = static_cast<uint8_t*>(s_allocator->Allocate(allocated_size,
                                                        alignment));
    memset(data, 0, allocated_size);
    auto external_memory = std::make_unique<DecoderBuffer::ExternalMemory>(
        base::make_span(data, allocated_size));
    return DecoderBuffer::FromExternalMemory(std::move(external_memory));
  } else {
    return DecoderBuffer::CopyFrom(data, size);
  }

}

DecoderBufferFactory::DecoderBufferFactory() : factory_(new FactoryImpl()) {}
DecoderBufferFactory::~DecoderBufferFactory() {}

scoped_refptr<DecoderBuffer> DecoderBufferFactory::CopyFrom(const uint8_t* data,
                                               size_t size) {
  return factory_->CopyFrom(data, size);
}

}  // namespace media
