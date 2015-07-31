// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/video_decoder_selector.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "media/base/bind_to_loop.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "media/filters/decrypting_video_decoder.h"

namespace media {

VideoDecoderSelector::VideoDecoderSelector(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const VideoDecoderList& decoders,
    const SetDecryptorReadyCB& set_decryptor_ready_cb)
    : message_loop_(message_loop),
      decoders_(decoders),
      set_decryptor_ready_cb_(set_decryptor_ready_cb),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

VideoDecoderSelector::~VideoDecoderSelector() {}

void VideoDecoderSelector::SelectVideoDecoder(
    const scoped_refptr<DemuxerStream>& stream,
    const StatisticsCB& statistics_cb,
    const SelectDecoderCB& select_decoder_cb) {
  DVLOG(2) << "SelectVideoDecoder()";
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(stream);

  // Make sure |select_decoder_cb| runs on a different execution stack.
  select_decoder_cb_ = BindToCurrentLoop(select_decoder_cb);

  const VideoDecoderConfig& config = stream->video_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream config.";
    base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
    return;
  }

  input_stream_ = stream;
  statistics_cb_ = statistics_cb;

  if (!config.is_encrypted()) {
    if (decoders_.empty()) {
      DLOG(ERROR) << "No video decoder can be used to decode the input stream.";
      base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
      return;
    }

    InitializeNextDecoder();
    return;
  }

  // This could happen if Encrypted Media Extension (EME) is not enabled.
  if (set_decryptor_ready_cb_.is_null()) {
    base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
    return;
  }

  video_decoder_ = new DecryptingVideoDecoder(message_loop_,
                                              set_decryptor_ready_cb_);

  video_decoder_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &VideoDecoderSelector::DecryptingVideoDecoderInitDone,
          weak_ptr_factory_.GetWeakPtr())),
      statistics_cb_);
}

void VideoDecoderSelector::DecryptingVideoDecoderInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status == PIPELINE_OK) {
    decoders_.clear();
    base::ResetAndReturn(&select_decoder_cb_).Run(video_decoder_, NULL);
    return;
  }

  video_decoder_ = NULL;

  if (decoders_.empty()) {
    DLOG(ERROR) << "No video decoder can be used to decode the input stream.";
    base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
    return;
  }

  decrypted_stream_ = new DecryptingDemuxerStream(
      message_loop_, set_decryptor_ready_cb_);

  decrypted_stream_->Initialize(
      input_stream_,
      BindToCurrentLoop(base::Bind(
          &VideoDecoderSelector::DecryptingDemuxerStreamInitDone,
          weak_ptr_factory_.GetWeakPtr())));
}

void VideoDecoderSelector::DecryptingDemuxerStreamInitDone(
    PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    decrypted_stream_ = NULL;
    base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
    return;
  }

  DCHECK(!decrypted_stream_->video_decoder_config().is_encrypted());
  input_stream_ = decrypted_stream_;
  InitializeNextDecoder();
}

void VideoDecoderSelector::InitializeNextDecoder() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  DCHECK(!decoders_.empty());

  video_decoder_ = decoders_.front();
  decoders_.pop_front();
  DCHECK(video_decoder_);
  video_decoder_->Initialize(input_stream_,
                             BindToCurrentLoop(base::Bind(
                                 &VideoDecoderSelector::DecoderInitDone,
                                 weak_ptr_factory_.GetWeakPtr())),
                             statistics_cb_);
}

void VideoDecoderSelector::DecoderInitDone(PipelineStatus status) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  if (status != PIPELINE_OK) {
    if (!decoders_.empty())
      InitializeNextDecoder();
    else
      base::ResetAndReturn(&select_decoder_cb_).Run(NULL, NULL);
    return;
  }

  decoders_.clear();
  base::ResetAndReturn(&select_decoder_cb_).Run(video_decoder_,
                                                decrypted_stream_);
}

}  // namespace media
