// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/stream_parser_buffer_factory.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

namespace media {

class StreamParserBufferFactory::FactoryImpl
    : public base::RefCountedThreadSafe<StreamParserBufferFactory::FactoryImpl> {
 public:
  FactoryImpl() = default;
  FactoryImpl(const FactoryImpl&) = delete;
  FactoryImpl& operator=(const FactoryImpl&) = delete;

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id);

  scoped_refptr<StreamParserBuffer> CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                                    int side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id);

 private:
  friend class base::RefCountedThreadSafe<StreamParserBufferFactory::FactoryImpl>;
  ~FactoryImpl() = default;

  base::Lock lock_;
};

scoped_refptr<StreamParserBuffer> StreamParserBufferFactory::FactoryImpl::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  base::AutoLock auto_lock(lock_);

  if (DecoderBufferFactory::HasAllocator())  {
    auto alloc = DecoderBufferFactory::Allocator::GetInstance();
    int alignment = alloc->GetBufferAlignment();
    int padding = alloc->GetBufferPadding();
    size_t allocated_size = data_size + padding;
    auto data = static_cast<uint8_t*>(alloc->Allocate(allocated_size,
                                                        alignment));
    memset(data, 0, allocated_size);
    auto external_memory = std::make_unique<AllocatedExternalMemory>(data, allocated_size, alloc);
    return StreamParserBuffer::FromExternalMemory(std::move(external_memory), is_key_frame, type, track_id);
  } else {
    return StreamParserBuffer::CopyFrom(data, data_size,
                                        is_key_frame,
                                        type,
                                        track_id);
  }

}

scoped_refptr<StreamParserBuffer> StreamParserBufferFactory::FactoryImpl::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                                    int side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  base::AutoLock auto_lock(lock_);

  if (DecoderBufferFactory::HasAllocator())  {
    auto alloc = DecoderBufferFactory::Allocator::GetInstance();
    int alignment = alloc->GetBufferAlignment();
    int padding = alloc->GetBufferPadding();
    size_t allocated_size = data_size + side_data_size + padding;
    auto data = static_cast<uint8_t*>(alloc->Allocate(allocated_size,
                                                        alignment));
    memset(data, 0, allocated_size);
    auto external_memory = std::make_unique<AllocatedExternalMemory>(data, allocated_size, alloc);
    return StreamParserBuffer::FromExternalMemory(std::move(external_memory), is_key_frame, type, track_id);
  } else {
    return StreamParserBuffer::CopyFrom(data, data_size,
                                        side_data, side_data_size,
                                        is_key_frame,
                                        type,
                                        track_id);
  }

}

StreamParserBufferFactory::StreamParserBufferFactory() : factory_(new FactoryImpl()) {}
StreamParserBufferFactory::~StreamParserBufferFactory() {}

scoped_refptr<StreamParserBuffer> StreamParserBufferFactory::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  return factory_->CopyFrom(data, data_size, is_key_frame, type, track_id);
}

scoped_refptr<StreamParserBuffer> StreamParserBufferFactory::CopyFrom(const uint8_t* data,
                                               size_t data_size,
                                               const uint8_t* side_data,
                                               size_t side_data_size,
                                               bool is_key_frame,
                                               Type type,
                                               TrackId track_id) {
  return factory_->CopyFrom(data, data_size, side_data, side_data_size, is_key_frame, type, track_id);
}

}  // namespace media
