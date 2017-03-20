// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_DECRYPTOR_H_
#define COBALT_MEDIA_BASE_DECRYPTOR_H_

#include <list>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/audio_buffer.h"
#include "cobalt/media/base/media_export.h"

namespace media {

class AudioDecoderConfig;
class DecoderBuffer;
class VideoDecoderConfig;
class VideoFrame;

// Decrypts (and decodes) encrypted buffer.
//
// All methods are called on the (video/audio) decoder thread. Decryptor
// implementations must be thread safe when methods are called this way.
// Depending on the implementation callbacks may be fired synchronously or
// asynchronously.
class MEDIA_EXPORT Decryptor {
 public:
  // TODO(xhwang): Replace kError with kDecryptError and kDecodeError.
  // TODO(xhwang): Replace kNeedMoreData with kNotEnoughData.
  enum Status {
    kSuccess,  // Decryption successfully completed. Decrypted buffer ready.
    kNoKey,    // No key is available to decrypt.
    kNeedMoreData,  // Decoder needs more data to produce a frame.
    kError          // Key is available but an error occurred during decryption.
  };

  // TODO(xhwang): Unify this with DemuxerStream::Type.
  enum StreamType { kAudio, kVideo };

  Decryptor();
  virtual ~Decryptor();

  // Indicates that a new key has been added to the MediaKeys object associated
  // with the Decryptor.
  typedef base::Callback<void()> NewKeyCB;

  // Registers a NewKeyCB which should be called when a new key is added to the
  // decryptor. Only one NewKeyCB can be registered for one |stream_type|.
  // If this function is called multiple times for the same |stream_type|, the
  // previously registered callback will be replaced. In other words,
  // registering a null callback cancels the originally registered callback.
  virtual void RegisterNewKeyCB(StreamType stream_type,
                                const NewKeyCB& key_added_cb) = 0;

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
  // - Only |data|, |data_size| and |timestamp| are set in the returned
  //   DecoderBuffer. The callback handler is responsible for setting other
  //   fields as appropriate.
  typedef base::Callback<void(Status, const scoped_refptr<DecoderBuffer>&)>
      DecryptCB;

  // Decrypts the |encrypted| buffer. The decrypt status and decrypted buffer
  // are returned via the provided callback |decrypt_cb|. The |encrypted| buffer
  // must not be NULL.
  // Decrypt() should not be called until any previous DecryptCB of the same
  // |stream_type| has completed. Thus, only one DecryptCB may be pending at
  // a time for a given |stream_type|.
  virtual void Decrypt(StreamType stream_type,
                       const scoped_refptr<DecoderBuffer>& encrypted,
                       const DecryptCB& decrypt_cb) = 0;

  // Cancels the scheduled decryption operation for |stream_type| and fires the
  // pending DecryptCB immediately with kSuccess and NULL.
  // Decrypt() should not be called again before the pending DecryptCB for the
  // same |stream_type| is fired.
  virtual void CancelDecrypt(StreamType stream_type) = 0;

  // Indicates completion of audio/video decoder initialization.
  //
  // First Parameter: Indicates initialization success.
  // - Set to true if initialization was successful. False if an error occurred.
  typedef base::Callback<void(bool)> DecoderInitCB;

  // Initializes a decoder with the given |config|, executing the |init_cb|
  // upon completion.
  virtual void InitializeAudioDecoder(const AudioDecoderConfig& config,
                                      const DecoderInitCB& init_cb) = 0;
  virtual void InitializeVideoDecoder(const VideoDecoderConfig& config,
                                      const DecoderInitCB& init_cb) = 0;

  // Helper structure for managing multiple decoded audio buffers per input.
  typedef std::list<scoped_refptr<AudioBuffer> > AudioFrames;

  // Indicates completion of audio/video decrypt-and-decode operation.
  //
  // First parameter: The status of the decrypt-and-decode operation.
  // - Set to kSuccess if the encrypted buffer is successfully decrypted and
  //   decoded. In this case, the decoded frame/buffers can be/contain:
  //   1) NULL, which means the operation has been aborted.
  //   2) End-of-stream (EOS) frame, which means that the decoder has hit EOS,
  //      flushed all internal buffers and cannot produce more video frames.
  //   3) Decrypted and decoded video frame or audio buffer.
  // - Set to kNoKey if no decryption key is available to decrypt the encrypted
  //   buffer. In this case the returned frame(s) must be NULL/empty.
  // - Set to kNeedMoreData if more data is needed to produce a video frame. In
  //   this case the returned frame(s) must be NULL/empty.
  // - Set to kError if unexpected error has occurred. In this case the
  //   returned frame(s) must be NULL/empty.
  // Second parameter: The decoded video frame or audio buffers.
  typedef base::Callback<void(Status, const AudioFrames&)> AudioDecodeCB;
  typedef base::Callback<void(Status, const scoped_refptr<VideoFrame>&)>
      VideoDecodeCB;

  // Decrypts and decodes the |encrypted| buffer. The status and the decrypted
  // buffer are returned via the provided callback.
  // The |encrypted| buffer must not be NULL.
  // At end-of-stream, this method should be called repeatedly with
  // end-of-stream DecoderBuffer until no frame/buffer can be produced.
  // These methods can only be called after the corresponding decoder has
  // been successfully initialized.
  virtual void DecryptAndDecodeAudio(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const AudioDecodeCB& audio_decode_cb) = 0;
  virtual void DecryptAndDecodeVideo(
      const scoped_refptr<DecoderBuffer>& encrypted,
      const VideoDecodeCB& video_decode_cb) = 0;

  // Resets the decoder to an initialized clean state, cancels any scheduled
  // decrypt-and-decode operations, and fires any pending
  // AudioDecodeCB/VideoDecodeCB immediately with kError and NULL.
  // This method can only be called after the corresponding decoder has been
  // successfully initialized.
  virtual void ResetDecoder(StreamType stream_type) = 0;

  // Releases decoder resources, deinitializes the decoder, cancels any
  // scheduled initialization or decrypt-and-decode operations, and fires
  // any pending DecoderInitCB/AudioDecodeCB/VideoDecodeCB immediately.
  // DecoderInitCB should be fired with false. AudioDecodeCB/VideoDecodeCB
  // should be fired with kError.
  // This method can be called any time after Initialize{Audio|Video}Decoder()
  // has been called (with the correct stream type).
  // After this operation, the decoder is set to an uninitialized state.
  // The decoder can be reinitialized after it is uninitialized.
  virtual void DeinitializeDecoder(StreamType stream_type) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Decryptor);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_DECRYPTOR_H_
