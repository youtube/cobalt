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
#include "media/base/buffers.h"
#include "media/base/decrypt_config.h"

namespace media {

class MEDIA_EXPORT DecoderBuffer : public Buffer {
 public:
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
  virtual const uint8* GetData() const OVERRIDE;
  virtual int GetDataSize() const OVERRIDE;

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

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_H_
