// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_
#define MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_

#include <list>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;
class DecryptingDemuxerStream;
class Decryptor;

// VideoDecoderSelector (creates if necessary and) initializes the proper
// VideoDecoder for a given DemuxerStream. If the given DemuxerStream is
// encrypted, a DecryptingDemuxerStream may also be created.
class MEDIA_EXPORT VideoDecoderSelector {
 public:
  typedef std::list<scoped_refptr<VideoDecoder> > VideoDecoderList;

  // Indicates completion of VideoDecoder selection.
  // - First parameter: The initialized VideoDecoder. If it's set to NULL, then
  // VideoDecoder initialization failed.
  // - Second parameter: The initialized DecryptingDemuxerStream. If it's not
  // NULL, then a DecryptingDemuxerStream is created and initialized to do
  // decryption for the initialized VideoDecoder.
  // Note: The caller owns selected VideoDecoder and DecryptingDemuxerStream.
  // The caller should call DecryptingDemuxerStream::Reset() before
  // calling VideoDecoder::Reset() to release any pending decryption or read.
  typedef base::Callback<
      void(const scoped_refptr<VideoDecoder>&,
           const scoped_refptr<DecryptingDemuxerStream>&)> SelectDecoderCB;

  // |set_decryptor_ready_cb| is optional. If |set_decryptor_ready_cb| is null,
  // no decryptor will be available to perform decryption.
  VideoDecoderSelector(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const VideoDecoderList& decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  ~VideoDecoderSelector();

  // Initializes and selects an VideoDecoder that can decode the |stream|.
  // Selected VideoDecoder (and DecryptingDemuxerStream) is returned via
  // the |select_decoder_cb|.
  void SelectVideoDecoder(const scoped_refptr<DemuxerStream>& stream,
                          const StatisticsCB& statistics_cb,
                          const SelectDecoderCB& select_decoder_cb);

 private:
  void DecryptingVideoDecoderInitDone(PipelineStatus status);
  void DecryptingDemuxerStreamInitDone(PipelineStatus status);
  void InitializeNextDecoder();
  void DecoderInitDone(PipelineStatus status);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  VideoDecoderList decoders_;
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  scoped_refptr<DemuxerStream> input_stream_;
  StatisticsCB statistics_cb_;
  SelectDecoderCB select_decoder_cb_;

  scoped_refptr<VideoDecoder> video_decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypted_stream_;

  base::WeakPtrFactory<VideoDecoderSelector> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoDecoderSelector);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_
