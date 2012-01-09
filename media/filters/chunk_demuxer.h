// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <list>

#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/base/stream_parser.h"

namespace media {

class ChunkDemuxerClient;
class ChunkDemuxerStream;
class FFmpegURLProtocol;

// Demuxer implementation that allows chunks of media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer, public StreamParserHost {
 public:
  explicit ChunkDemuxer(ChunkDemuxerClient* client);
  virtual ~ChunkDemuxer();

  void Init(const PipelineStatusCB& cb);

  // Demuxer implementation.
  virtual void set_host(DemuxerHost* host) OVERRIDE;
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Seek(base::TimeDelta time, const PipelineStatusCB&  cb) OVERRIDE;
  virtual void OnAudioRendererDisabled() OVERRIDE;
  virtual scoped_refptr<DemuxerStream> GetStream(
      DemuxerStream::Type type) OVERRIDE;
  virtual void SetPreload(Preload preload) OVERRIDE;
  virtual base::TimeDelta GetStartTime() const OVERRIDE;
  virtual int GetBitrate() OVERRIDE;
  virtual bool IsLocalSource() OVERRIDE;
  virtual bool IsSeekable() OVERRIDE;

  // Methods used by an external object to control this demuxer.
  void FlushData();

  // Appends media data to the stream. Returns false if this method
  // is called in an invalid state.
  bool AppendData(const uint8* data, size_t length);
  void EndOfStream(PipelineStatus status);
  bool HasEnded();
  void Shutdown();

 private:
  enum State {
    WAITING_FOR_INIT,
    INITIALIZING,
    INITIALIZED,
    ENDED,
    PARSE_ERROR,
    SHUTDOWN,
  };

  void ChangeState_Locked(State new_state);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  void OnStreamParserInitDone(bool success, base::TimeDelta duration);

  // StreamParserHost implementation.
  virtual bool OnNewAudioConfig(const AudioDecoderConfig& config) OVERRIDE;
  virtual bool OnNewVideoConfig(const VideoDecoderConfig& config) OVERRIDE;
  virtual bool OnAudioBuffers(const BufferQueue& buffer) OVERRIDE;
  virtual bool OnVideoBuffers(const BufferQueue& buffer) OVERRIDE;

  base::Lock lock_;
  State state_;

  ChunkDemuxerClient* client_;
  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;

  scoped_refptr<ChunkDemuxerStream> audio_;
  scoped_refptr<ChunkDemuxerStream> video_;

  int64 buffered_bytes_;

  base::TimeDelta duration_;

  scoped_ptr<StreamParser> stream_parser_;

  // Should a Seek() call wait for more data before calling the
  // callback.
  bool seek_waits_for_data_;

  ByteQueue byte_queue_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
