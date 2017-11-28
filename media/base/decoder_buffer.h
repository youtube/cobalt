// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A specialized buffer for interfacing with audio / video decoders.
//
// Specifically ensures that data is aligned and padded as necessary by the
// underlying decoding framework.  On desktop platforms this means memory is
// allocated using FFmpeg with particular alignment and padding requirements.
//
// Also includes decoder specific functionality for decryption.

#ifndef MEDIA_BASE_DECODER_BUFFER_H_
#define MEDIA_BASE_DECODER_BUFFER_H_

#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "media/base/buffers.h"
#include "media/base/decrypt_config.h"

namespace media {

#if defined(__LB_SHELL__) || defined(COBALT)

class DecryptConfig;
class ShellBufferFactory;

class MEDIA_EXPORT DecoderBuffer : public Buffer {
 public:
  // Create a DecoderBuffer indicating we've reached end of stream or an error.
  // GetData() and GetWritableData() return NULL and GetDataSize() returns 0.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer(
      base::TimeDelta timestamp);

  // Buffer implementation.
  const uint8* GetData() const override { return buffer_; }
  // Data size can be less than allocated size after ShrinkTo is called.
  int GetDataSize() const override { return static_cast<int>(size_); }

  int GetAllocatedSize() const { return static_cast<int>(allocated_size_); }
  // This is used by the data that we don't know the exact size before reading.
  void ShrinkTo(int size);
  bool IsKeyframe() const { return is_keyframe_; }

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData() { return buffer_; }

  // Returns a flag indicating whether or not the buffer has been decrypted
  // in-place.  If so, a CDM should avoid decrypting it again after a seek.
  bool IsAlreadyDecrypted() { return is_decrypted_; }
  void SetAlreadyDecrypted(bool value) { is_decrypted_ = value; }

  virtual const DecryptConfig* GetDecryptConfig() const;
  virtual void SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config);

 protected:
  friend class ShellBufferFactory;
  // Should only be called by ShellBufferFactory, consumers should use
  // ShellBufferFactory::AllocateBuffer to make a DecoderBuffer.
  DecoderBuffer(uint8* reusable_buffer, size_t size, bool is_keyframe);
  // For deferred allocation create a shell buffer with buffer_ NULL but a
  // non-zero size. Then we use the SetBuffer() method below to actually
  // set the reusable buffer pointer when it becomes available
  void SetBuffer(uint8* reusable_buffer);

  virtual ~DecoderBuffer();
  uint8* buffer_;
  size_t size_;
  size_t allocated_size_;
  scoped_refptr<ShellBufferFactory> buffer_factory_;
  scoped_ptr<DecryptConfig> decrypt_config_;
  bool is_decrypted_;
  bool is_keyframe_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DecoderBuffer);
};

#else  // defined(__LB_SHELL__) || defined(COBALT)

class MEDIA_EXPORT DecoderBuffer : public Buffer {
 public:
  enum {
    kPaddingSize = 16,
#if defined(ARCH_CPU_ARM_FAMILY)
    kAlignmentSize = 16
#else
    kAlignmentSize = 32
#endif
  };

  // Allocates buffer of size |buffer_size| >= 0.  Buffer will be padded and
  // aligned as necessary.
  explicit DecoderBuffer(int buffer_size);

  // Create a DecoderBuffer whose |data_| is copied from |data|.  Buffer will be
  // padded and aligned as necessary.  |data| must not be NULL and |size| >= 0.
  static scoped_refptr<DecoderBuffer> CopyFrom(const uint8* data, int size);

  // Create a DecoderBuffer indicating we've reached end of stream.  GetData()
  // and GetWritableData() will return NULL and GetDataSize() will return 0.
  static scoped_refptr<DecoderBuffer> CreateEOSBuffer();

  // Buffer implementation.
  virtual const uint8* GetData() const override;
  virtual int GetDataSize() const override;

  // Returns a read-write pointer to the buffer data.
  virtual uint8* GetWritableData();

  virtual const DecryptConfig* GetDecryptConfig() const;
  virtual void SetDecryptConfig(scoped_ptr<DecryptConfig> decrypt_config);

 protected:
  // Allocates a buffer of size |size| >= 0 and copies |data| into it.  Buffer
  // will be padded and aligned as necessary.  If |data| is NULL then |data_| is
  // set to NULL and |buffer_size_| to 0.
  DecoderBuffer(const uint8* data, int size);
  virtual ~DecoderBuffer();

 private:
  int buffer_size_;
  uint8* data_;
  scoped_ptr<DecryptConfig> decrypt_config_;

  // Constructor helper method for memory allocations.
  void Initialize();

  DISALLOW_COPY_AND_ASSIGN(DecoderBuffer);
};

#endif  // defined(__LB_SHELL__) || defined(COBALT)

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
