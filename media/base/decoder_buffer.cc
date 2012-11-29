// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/decoder_buffer.h"

#include "base/logging.h"
#include "media/base/decrypt_config.h"
#if !defined(OS_ANDROID) && !defined(__LB_SHELL__)
#include "media/ffmpeg/ffmpeg_common.h"
#endif

#if defined (__LB_LINUX__)
#include <malloc.h>
#endif

namespace media {

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
#if !defined(OS_ANDROID) && !defined(__LB_SHELL__)
  av_free(data_);
#elif defined(__LB_SHELL__)
  free(data_);
#else
  delete[] data_;
#endif
}

void DecoderBuffer::Initialize() {
  DCHECK_GE(buffer_size_, 0);
#if !defined(OS_ANDROID) && !defined(__LB_SHELL__)
  // Why FF_INPUT_BUFFER_PADDING_SIZE?  FFmpeg assumes all input buffers are
  // padded.  Using av_malloc with padding ensures FFmpeg only recieves data
  // padded and aligned to its specifications.
  data_ = reinterpret_cast<uint8*>(
      av_malloc(buffer_size_ + FF_INPUT_BUFFER_PADDING_SIZE));
  memset(data_ + buffer_size_, 0, FF_INPUT_BUFFER_PADDING_SIZE);
#elif defined(__LB_SHELL__)
  handle_ = 0;
  data_size_ = 0;
  buffer_size_ = ((buffer_size_ + kShellMediaBufferAlignment - 1) /
                  kShellMediaBufferAlignment) * kShellMediaBufferAlignment;
  data_ = (uint8*)memalign(kShellMediaBufferAlignment, buffer_size_);
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
#if defined(__LB_SHELL__)
  return data_size_;
#else
  return buffer_size_;
#endif
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

#if defined(__LB_SHELL__)
void DecoderBuffer::SetHandle(uint32 handle) {
  handle_ = handle;
}

uint32 DecoderBuffer::GetHandle() {
  return handle_;
}

int DecoderBuffer::GetBufferSize() {
  return buffer_size_;
}

void DecoderBuffer::SetDataSize(int size) {
  // should never exceed buffer_size_
  DCHECK_LE(size, buffer_size_);
  data_size_ = size;
}


#endif

}  // namespace media
