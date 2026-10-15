// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MEDIA_FILTERS_STARBOARD_SELECTED_TRACK_STREAM_H_
#define MEDIA_FILTERS_STARBOARD_SELECTED_TRACK_STREAM_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "media/base/demuxer_stream.h"

namespace media {

class ChunkDemuxerStream;

// Forwards DemuxerStream reads to the ChunkDemuxerStream of the currently
// selected track when multi-SourceBuffer switching is enabled (see
// SetEnableMultiSourceBufferSwitching). The renderer keeps the DemuxerStream
// proxy it received at initialization while track selection switches the
// underlying stream.
//
// After SwitchTo(), reads are held until the next seek completes, and the first
// read after the seek returns kConfigChanged.
//
// Owned by ChunkDemuxer and destroyed before the streams it forwards to.
// Thread-safe: state is guarded by |lock_|, and read callbacks run on the
// caller's sequence.
class SelectedTrackStream final : public DemuxerStream {
 public:
  SelectedTrackStream(Type type, ChunkDemuxerStream* stream);

  SelectedTrackStream(const SelectedTrackStream&) = delete;
  SelectedTrackStream& operator=(const SelectedTrackStream&) = delete;

  ~SelectedTrackStream() override;

  // DemuxerStream implementation.
  void Read(uint32_t count, ReadCB read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  StreamLiveness liveness() const override;
  bool SupportsConfigChanges() override;

  // Returns the stream that serves the reads.
  ChunkDemuxerStream* stream() const;

  // Makes |stream| serve the reads, starting after the next seek. A read that
  // is pending on the previous stream is held until then, instead of being
  // completed with the end of stream when the previous stream is disabled.
  void SwitchTo(ChunkDemuxerStream* stream);

  // Called when the demuxer aborts the reads of all streams for a seek.
  void AbortReads();

  // Called when the demuxer starts returning data, after a seek.
  void StartReturningData();

  // Completes the held read, and every later read, with the end of stream.
  void Shutdown();

 private:
  void ServeRead_Locked() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const Type type_;
  mutable base::Lock lock_;
  raw_ptr<ChunkDemuxerStream> stream_ GUARDED_BY(lock_);
  // A read held by this object rather than forwarded to |stream_|.
  ReadCB read_cb_ GUARDED_BY(lock_);
  uint32_t read_count_ GUARDED_BY(lock_) = 0;
  // Set by SwitchTo() until the next StartReturningData().
  bool holding_reads_ GUARDED_BY(lock_) = false;
  // Set by SwitchTo() until the next read is served.
  bool config_change_pending_ GUARDED_BY(lock_) = false;
  bool is_shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace media

#endif  // MEDIA_FILTERS_STARBOARD_SELECTED_TRACK_STREAM_H_
