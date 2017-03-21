// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_FAKE_DEMUXER_STREAM_H_
#define COBALT_MEDIA_BASE_FAKE_DEMUXER_STREAM_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/base/audio_decoder_config.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/demuxer_stream_provider.h"
#include "cobalt/media/base/video_decoder_config.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

class FakeDemuxerStream : public DemuxerStream {
 public:
  // Constructs an object that outputs |num_configs| different configs in
  // sequence with |num_frames_in_one_config| buffers for each config. The
  // output buffers are encrypted if |is_encrypted| is true.
  FakeDemuxerStream(int num_configs, int num_buffers_in_one_config,
                    bool is_encrypted);
  ~FakeDemuxerStream() OVERRIDE;

  // DemuxerStream implementation.
  void Read(const ReadCB& read_cb) OVERRIDE;
  AudioDecoderConfig audio_decoder_config() OVERRIDE;
  VideoDecoderConfig video_decoder_config() OVERRIDE;
  Type type() const OVERRIDE;
  bool SupportsConfigChanges() OVERRIDE;
  VideoRotation video_rotation() OVERRIDE;
  bool enabled() const OVERRIDE;
  void set_enabled(bool enabled, base::TimeDelta timestamp) OVERRIDE;
  void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) OVERRIDE;

  void Initialize();

  int num_buffers_returned() const { return num_buffers_returned_; }

  // Upon the next read, holds the read callback until SatisfyRead() or Reset()
  // is called.
  void HoldNextRead();

  // Upon the next config change read, holds the read callback until
  // SatisfyRead() or Reset() is called. If there is no config change any more,
  // no read will be held.
  void HoldNextConfigChangeRead();

  // Satisfies the pending read with the next scheduled status and buffer.
  void SatisfyRead();

  // Satisfies pending read request and then holds the following read.
  void SatisfyReadAndHoldNext();

  // Satisfies the pending read (if any) with kAborted and NULL. This call
  // always clears |hold_next_read_|.
  void Reset();

  // Reset() this demuxer stream and set the reading position to the start of
  // the stream.
  void SeekToStart();

  // Sets further read requests to return EOS buffers.
  void SeekToEndOfStream();

  // Sets the splice timestamp for all furture buffers returned via Read().
  void set_splice_timestamp(base::TimeDelta splice_timestamp) {
    splice_timestamp_ = splice_timestamp;
  }

 private:
  void UpdateVideoDecoderConfig();
  void DoRead();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  const int num_configs_;
  const int num_buffers_in_one_config_;
  const bool config_changes_;
  const bool is_encrypted_;

  int num_configs_left_;

  // Number of frames left with the current decoder config.
  int num_buffers_left_in_current_config_;

  int num_buffers_returned_;

  base::TimeDelta current_timestamp_;
  base::TimeDelta duration_;
  base::TimeDelta splice_timestamp_;

  gfx::Size next_coded_size_;
  VideoDecoderConfig video_decoder_config_;

  ReadCB read_cb_;

  int next_read_num_;
  // Zero-based number indicating which read operation should be held. -1 means
  // no read shall be held.
  int read_to_hold_;

  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStream);
};

class FakeDemuxerStreamProvider : public DemuxerStreamProvider {
 public:
  // Note: FakeDemuxerStream currently only supports a fake video DemuxerStream.
  FakeDemuxerStreamProvider(int num_video_configs,
                            int num_video_buffers_in_one_config,
                            bool is_video_encrypted);
  ~FakeDemuxerStreamProvider() OVERRIDE;

  // DemuxerStreamProvider implementation.
  DemuxerStream* GetStream(DemuxerStream::Type type) OVERRIDE;

 private:
  FakeDemuxerStream fake_video_stream_;

  DISALLOW_COPY_AND_ASSIGN(FakeDemuxerStreamProvider);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_FAKE_DEMUXER_STREAM_H_
