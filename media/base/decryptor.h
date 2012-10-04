// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECRYPTOR_H_
#define MEDIA_BASE_DECRYPTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"

namespace media {

class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

// Performs key operations and decrypts (and decodes) encrypted buffer.
//
// Key operations (GenerateKeyRequest(), AddKey() and CancelKeyRequest())
// are called on the renderer thread. Therefore, these calls should be fast
// and nonblocking; key events should be fired asynchronously.
// All other methods are called on the (video/audio) decoder thread.
// Decryptor implementations must be thread safe when methods are called
// following the above model.
// Depending on the implementation callbacks may be fired synchronously or
// asynchronously.
class MEDIA_EXPORT Decryptor {
 public:
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    kServiceError,
    kOutputError,
    kHardwareChangeError,
    kDomainError
  };

  enum Status {
    kSuccess,  // Decryption successfully completed. Decrypted buffer ready.
    kNoKey,  // No key is available to decrypt.
    kNeedMoreData,  // Decoder needs more data to produce a frame.
    kError  // Key is available but an error occurred during decryption.
  };

  Decryptor();
  virtual ~Decryptor();

  // Generates a key request for the |key_system| with |init_data| provided.
  // Returns true if generating key request succeeded, false otherwise.
  // Note: AddKey() and CancelKeyRequest() should only be called after
  // GenerateKeyRequest() returns true.
  virtual bool GenerateKeyRequest(const std::string& key_system,
                                  const uint8* init_data,
                                  int init_data_length) = 0;

  // Adds a |key| to the |key_system|. The |key| is not limited to a decryption
  // key. It can be any data that the key system accepts, such as a license.
  // If multiple calls of this function set different keys for the same
  // key ID, the older key will be replaced by the newer key.
  virtual void AddKey(const std::string& key_system,
                      const uint8* key,
                      int key_length,
                      const uint8* init_data,
                      int init_data_length,
                      const std::string& session_id) = 0;

  // Cancels the key request specified by |session_id|.
  virtual void CancelKeyRequest(const std::string& key_system,
                                const std::string& session_id) = 0;

  // Indicates completion of a decryption operation.
  //
  // First parameter: The status of the decryption operation.
  // - Set to kSuccess if the encrypted buffer is successfully decrypted and
  //   the decrypted buffer is ready to be read.
  // - Set to kNoKey if no decryption key is available to decrypt the encrypted
  //   buffer. In this case the decrypted buffer must be NULL.
  // - Set to kError if unexpected error has occurred. In this case the
  //   decrypted buffer must be NULL.
  // - This parameter should not be set to kNeedMoreData.
  // Second parameter: The decrypted buffer.
  typedef base::Callback<void(Status,
                              const scoped_refptr<DecoderBuffer>&)> DecryptCB;

  // Decrypts the |encrypted| buffer. The decrypt status and decrypted buffer
  // are returned via the provided callback |decrypt_cb|. The |encrypted| buffer
  // must not be NULL.
  // Decrypt() should not be called until any previous DecryptCB has completed.
  // Thus, only one DecryptCB may be pending at a time.
  virtual void Decrypt(const scoped_refptr<DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) = 0;

  // Cancels the scheduled decryption operation and fires the pending DecryptCB
  // immediately with kSuccess and NULL.
  // Decrypt() should not be called again before the pending DecryptCB is fired.
  virtual void CancelDecrypt() = 0;

  // Indicates completion of decoder initialization.
  //
  // First Parameter: Indicates initialization success.
  // - Set to true if initialization was successful. False if an error occurred.
  typedef base::Callback<void(bool)> DecoderInitCB;

  // Initializes a video decoder with the given |config|, executing the
  // |init_cb| upon completion.
  // Note: DecryptAndDecodeVideo(), ResetVideoDecoder() and StopVideoDecoder()
  // can only be called after InitializeVideoDecoder() succeeded.
  virtual void InitializeVideoDecoder(const VideoDecoderConfig& config,
                                      const DecoderInitCB& init_cb) = 0;

  // Indicates completion of video decrypting and decoding operation.
  //
  // First parameter: The status of the decrypting and decoding operation.
  // - Set to kSuccess if the encrypted buffer is successfully decrypted and
  //   decoded. In this case, the decoded video frame can be:
  //   1) NULL, which means the operation has been aborted.
  //   2) End-of-stream (EOS) frame, which means that the decoder has hit EOS,
  //      flushed all internal buffers and cannot produce more video frames.
  //   3) Decrypted and decoded video frame.
  // - Set to kNoKey if no decryption key is available to decrypt the encrypted
  //   buffer. In this case the decoded video frame must be NULL.
  // - Set to kNeedMoreData if more data is needed to produce a video frame. In
  //   this case the decoded video frame must be NULL.
  // - Set to kError if unexpected error has occurred. In this case the
  //   decoded video frame must be NULL.
  // Second parameter: The decoded video frame.
  typedef base::Callback<void(Status,
                              const scoped_refptr<VideoFrame>&)> VideoDecodeCB;

  // Decrypts and decodes the |encrypted| buffer. The status and the decrypted
  // buffer are returned via the provided callback |video_decode_cb|.
  // The |encrypted| buffer must not be NULL.
  // At end-of-stream, this method should be called repeatedly with
  // end-of-stream DecoderBuffer until no video frame can be produced.
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) = 0;

  // Cancels scheduled video decrypt-and-decode operations and fires any pending
  // VideoDecodeCB immediately with kError and NULL frame.
  virtual void CancelDecryptAndDecodeVideo() = 0;

  // Stops decoder and sets it to an uninitialized state.
  // The decoder can be reinitialized by calling InitializeVideoDecoder() after
  // it is stopped.
  virtual void StopVideoDecoder() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Decryptor);
};

}  // namespace media

#endif  // MEDIA_BASE_DECRYPTOR_H_
