// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_
#define MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;
class Decryptor;

// Decryptor-based AudioDecoder implementation that can decrypt and decode
// encrypted audio buffers and return decrypted and decompressed audio frames.
// All public APIs and callbacks are trampolined to the |message_loop_| so
// that no locks are required for thread safety.
class MEDIA_EXPORT DecryptingAudioDecoder : public AudioDecoder {
 public:
  DecryptingAudioDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);

  // AudioDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;

 protected:
  virtual ~DecryptingAudioDecoder();

 private:
  // For a detailed state diagram please see this link: http://goo.gl/8jAok
  // TODO(xhwang): Add a ASCII state diagram in this file after this class
  // stabilizes.
  // TODO(xhwang): Update this diagram for DecryptingAudioDecoder.
  enum State {
    kUninitialized = 0,
    kDecryptorRequested,
    kPendingDecoderInit,
    kIdle,
    kPendingConfigChange,
    kPendingDemuxerRead,
    kPendingDecode,
    kWaitingForKey,
    kDecodeFinished,
  };

  // Carries out the initialization operation scheduled by Initialize().
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    const PipelineStatusCB& status_cb,
                    const StatisticsCB& statistics_cb);

  // Callback for DecryptorHost::RequestDecryptor().
  void SetDecryptor(Decryptor* decryptor);

  // Callback for Decryptor::InitializeAudioDecoder() during initialization.
  void FinishInitialization(bool success);

  // Callback for Decryptor::InitializeAudioDecoder() during config change.
  void FinishConfigChange(bool success);

  // Carries out the buffer reading operation scheduled by Read().
  void DoRead(const ReadCB& read_cb);

  void ReadFromDemuxerStream();

  // Callback for DemuxerStream::Read().
  void DoDecryptAndDecodeBuffer(DemuxerStream::Status status,
                                const scoped_refptr<DecoderBuffer>& buffer);

  void DecodePendingBuffer();

  // Callback for Decryptor::DecryptAndDecodeAudio().
  void DeliverFrame(int buffer_size,
                    Decryptor::Status status,
                    const Decryptor::AudioBuffers& frames);

  // Carries out the frame delivery operation scheduled by DeliverFrame().
  void DoDeliverFrame(int buffer_size,
                      Decryptor::Status status,
                      const Decryptor::AudioBuffers& frames);

  // Callback for the |decryptor_| to notify this object that a new key has been
  // added.
  void OnKeyAdded();

  // Resets decoder and calls |reset_cb_|.
  void DoReset();

  // Sets timestamp and duration for |queued_audio_frames_| to make sure the
  // renderer always receives continuous frames without gaps and overlaps.
  void EnqueueFrames(const Decryptor::AudioBuffers& frames);

  // Converts number of samples to duration.
  base::TimeDelta NumberOfSamplesToDuration(int number_of_samples) const;

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  State state_;

  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  // Callback to request/cancel decryptor creation notification.
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  Decryptor* decryptor_;

  // The buffer returned by the demuxer that needs decrypting/decoding.
  scoped_refptr<media::DecoderBuffer> pending_buffer_to_decode_;

  // Indicates the situation where new key is added during pending decode
  // (in other words, this variable can only be set in state kPendingDecode).
  // If this variable is true and kNoKey is returned then we need to try
  // decrypting/decoding again in case the newly added key is the correct
  // decryption key.
  bool key_added_while_decode_pending_;

  Decryptor::AudioBuffers queued_audio_frames_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  int bytes_per_sample_;

  base::TimeDelta output_timestamp_base_;
  int total_samples_decoded_;

  DISALLOW_COPY_AND_ASSIGN(DecryptingAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_AUDIO_DECODER_H_
