// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_
#define MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_

#include <list>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;
class DecryptingDemuxerStream;
class Decryptor;

// AudioDecoderSelector (creates if necessary and) initializes the proper
// AudioDecoder for a given DemuxerStream. If the given DemuxerStream is
// encrypted, a DecryptingDemuxerStream may also be created.
class MEDIA_EXPORT AudioDecoderSelector {
 public:
  typedef std::list<scoped_refptr<AudioDecoder> > AudioDecoderList;

  // Indicates completion of AudioDecoder selection.
  // - First parameter: The initialized AudioDecoder. If it's set to NULL, then
  // AudioDecoder initialization failed.
  // - Second parameter: The initialized DecryptingDemuxerStream. If it's not
  // NULL, then a DecryptingDemuxerStream is created and initialized to do
  // decryption for the initialized AudioDecoder.
  // Note: The caller owns selected AudioDecoder and DecryptingDemuxerStream.
  // The caller should call DecryptingDemuxerStream::Reset() before
  // calling AudioDecoder::Reset() to release any pending decryption or read.
  typedef base::Callback<
      void(const scoped_refptr<AudioDecoder>&,
           const scoped_refptr<DecryptingDemuxerStream>&)> SelectDecoderCB;

  // |set_decryptor_ready_cb| is optional. If |set_decryptor_ready_cb| is null,
  // no decryptor will be available to perform decryption.
  AudioDecoderSelector(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      const AudioDecoderList& decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  ~AudioDecoderSelector();

  // Initializes and selects an AudioDecoder that can decode the |stream|.
  // Selected AudioDecoder (and DecryptingDemuxerStream) is returned via
  // the |select_decoder_cb|.
  void SelectAudioDecoder(const scoped_refptr<DemuxerStream>& stream,
                          const StatisticsCB& statistics_cb,
                          const SelectDecoderCB& select_decoder_cb);

 private:
  void DecryptingAudioDecoderInitDone(PipelineStatus status);
  void DecryptingDemuxerStreamInitDone(PipelineStatus status);
  void InitializeNextDecoder();
  void DecoderInitDone(PipelineStatus status);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  AudioDecoderList decoders_;
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  scoped_refptr<DemuxerStream> input_stream_;
  StatisticsCB statistics_cb_;
  SelectDecoderCB select_decoder_cb_;

  scoped_refptr<AudioDecoder> audio_decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypted_stream_;

  base::WeakPtrFactory<AudioDecoderSelector> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AudioDecoderSelector);
};

}  // namespace media

#endif  // MEDIA_FILTERS_AUDIO_DECODER_SELECTOR_H_
