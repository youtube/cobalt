// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"

#if defined(__LB_SHELL__) || defined(COBALT)
#include "base/debug/trace_event.h"
#include "media/base/shell_buffer_factory.h"
#endif  // defined(__LB_SHELL__) || defined(COBALT)

#if !defined(OS_ANDROID)
#include "base/memory/aligned_memory.h"
#endif

namespace media {

#if defined(__LB_SHELL__) || defined(COBALT)

// static
scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer(
    base::TimeDelta timestamp) {
  scoped_refptr<DecoderBuffer> eos =
      scoped_refptr<DecoderBuffer>(new DecoderBuffer(NULL, 0, true));
  eos->SetTimestamp(timestamp);
  return eos;
}

void DecoderBuffer::ShrinkTo(int size) {
  CHECK_LE(size, GetAllocatedSize());
  size_ = size;
}

DecoderBuffer::DecoderBuffer(uint8* reusable_buffer,
                             size_t size,
                             bool is_keyframe)
    : Buffer(kNoTimestamp(), kInfiniteDuration()),
      buffer_(reusable_buffer),
      size_(size),
      allocated_size_(size),
      is_decrypted_(false),
      is_keyframe_(is_keyframe) {
  if (buffer_) {
    // Retain a reference to the buffer factory, to ensure that we do not
    // outlive it.
    buffer_factory_ = ShellBufferFactory::Instance();
  }
}

DecoderBuffer::~DecoderBuffer() {
  // recycle our buffer
  if (buffer_) {
    TRACE_EVENT1("media_stack", "DecoderBuffer::~DecoderBuffer()",
                 "timestamp", GetTimestamp().InMicroseconds());
    DCHECK_NE(buffer_factory_, (ShellBufferFactory*)NULL);
    buffer_factory_->Reclaim(buffer_);
  }
}

const DecryptConfig* DecoderBuffer::GetDecryptConfig() const {
  DCHECK(!IsEndOfStream());
  return decrypt_config_.get();
}

void DecoderBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  DCHECK(!IsEndOfStream());
  decrypt_config_ = decrypt_config.Pass();
}

void DecoderBuffer::SetBuffer(uint8* reusable_buffer) {
  buffer_ = reusable_buffer;
  if (buffer_) {
    // Retain a reference to the buffer factory, to ensure that we do not
    // outlive it.
    buffer_factory_ = ShellBufferFactory::Instance();
  }
}

#else  // defined(__LB_SHELL__) || defined(COBALT)

DecoderBuffer::DecoderBuffer(int buffer_size)
    : Buffer(base::TimeDelta(), base::TimeDelta()),
      buffer_size_(buffer_size) {
  Initialize();
}

DecoderBuffer::DecoderBuffer(const uint8* data, int buffer_size)
    : Buffer(base::TimeDelta(), base::TimeDelta()),
      buffer_size_(buffer_size) {
  // Prevent invalid allocations.  Also used to create end of stream buffers.
  if (!data) {
    buffer_size_ = 0;
    data_ = NULL;
    return;
  }

  Initialize();
  memcpy(data_, data, buffer_size_);
}

DecoderBuffer::~DecoderBuffer() {
#if !defined(OS_ANDROID)
  base::AlignedFree(data_);
#else
  delete[] data_;
#endif
}

void DecoderBuffer::Initialize() {
  DCHECK_GE(buffer_size_, 0);
#if !defined(OS_ANDROID)
  data_ = reinterpret_cast<uint8*>(
      base::AlignedAlloc(buffer_size_ + kPaddingSize, kAlignmentSize));
  memset(data_ + buffer_size_, 0, kPaddingSize);
#else
  data_ = new uint8[buffer_size_];
#endif
}

scoped_refptr<DecoderBuffer> DecoderBuffer::CopyFrom(const uint8* data,
                                                     int data_size) {
  DCHECK(data);
  return make_scoped_refptr(new DecoderBuffer(data, data_size));
}

scoped_refptr<DecoderBuffer> DecoderBuffer::CreateEOSBuffer() {
  return make_scoped_refptr(new DecoderBuffer(NULL, 0));
}

const uint8* DecoderBuffer::GetData() const {
  return data_;
}

int DecoderBuffer::GetDataSize() const {
  return buffer_size_;
}

uint8* DecoderBuffer::GetWritableData() {
  return data_;
}

const DecryptConfig* DecoderBuffer::GetDecryptConfig() const {
  return decrypt_config_.get();
}

void DecoderBuffer::SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config) {
  decrypt_config_ = decrypt_config.Pass();
}

#endif  // defined(__LB_SHELL__) || defined(COBALT)

}  // namespace media
