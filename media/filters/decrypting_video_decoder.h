// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_
#define MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;
class Decryptor;

// Decryptor-based VideoDecoder implementation that can decrypt and decode
// encrypted video buffers and return decrypted and decompressed video frames.
//
// TODO(xhwang): For now, DecryptingVideoDecoder relies on the decryptor to do
// both decryption and video decoding. Add the path to use the decryptor for
// decryption only and use other VideoDecoder implementations within
// DecryptingVideoDecoder for video decoding.
class MEDIA_EXPORT DecryptingVideoDecoder : public VideoDecoder {
 public:
  typedef base::Callback<
      scoped_refptr<base::MessageLoopProxy>()> MessageLoopFactoryCB;
  // |message_loop_factory_cb| provides message loop that the decryption
  // operations run on.
  // |decryptor| performs real decrypting operations.
  // Note that |decryptor| must outlive this instance.
  DecryptingVideoDecoder(const MessageLoopFactoryCB& message_loop_factory_cb,
                         Decryptor* decryptor);

  // VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

 protected:
  virtual ~DecryptingVideoDecoder();

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kDecodeFinished
  };

  // Carries out the initialization operation scheduled by Initialize().
  void DoInitialize(const scoped_refptr<DemuxerStream>& stream,
                    const PipelineStatusCB& status_cb,
                    const StatisticsCB& statistics_cb);

  // Callback for Decryptor::InitializeVideoDecoder().
  void FinishInitialization(bool success);

  // Carries out the initialization finishing operation scheduled by
  // FinishInitialization().
  void DoFinishInitialization(bool success);

  // Carries out the buffer reading operation scheduled by Read().
  void DoRead(const ReadCB& read_cb);

  void ReadFromDemuxerStream();

  // Callback for DemuxerStream::Read().
  void DecryptAndDecodeBuffer(DemuxerStream::Status status,
                              const scoped_refptr<DecoderBuffer>& buffer);

  // Carries out the buffer decrypting/decoding operation scheduled by
  // DecryptAndDecodeBuffer().
  void DoDecryptAndDecodeBuffer(DemuxerStream::Status status,
                                const scoped_refptr<DecoderBuffer>& buffer);

  // Callback for Decryptor::DecryptAndDecodeVideo().
  void DeliverFrame(int buffer_size,
                    Decryptor::Status status,
                    const scoped_refptr<VideoFrame>& frame);

  // Carries out the frame delivery operation scheduled by DeliverFrame().
  void DoDeliverFrame(int buffer_size,
                      Decryptor::Status status,
                      const scoped_refptr<VideoFrame>& frame);

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Free decoder resources and call |stop_cb_|.
  void DoStop();

  // This is !is_null() iff Initialize() hasn't been called.
  MessageLoopFactoryCB message_loop_factory_cb_;

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  DecoderState state_;
  PipelineStatusCB init_cb_;
  StatisticsCB statistics_cb_;
  ReadCB read_cb_;
  base::Closure reset_cb_;
  base::Closure stop_cb_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  Decryptor* const decryptor_;

  DISALLOW_COPY_AND_ASSIGN(DecryptingVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_DECRYPTING_VIDEO_DECODER_H_
