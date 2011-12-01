// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_H_

#include <list>

#include "base/synchronization/lock.h"
#include "media/base/byte_queue.h"
#include "media/base/demuxer.h"
#include "media/webm/webm_cluster_parser.h"

struct AVFormatContext;

namespace media {

class ChunkDemuxerClient;
class ChunkDemuxerStream;
class FFmpegURLProtocol;

// Demuxer implementation that allows chunks of WebM media data to be passed
// from JavaScript to the media stack.
class MEDIA_EXPORT ChunkDemuxer : public Demuxer {
 public:
  explicit ChunkDemuxer(ChunkDemuxerClient* client);
  virtual ~ChunkDemuxer();

  void Init(const PipelineStatusCB& cb);

  // Demuxer implementation.
  virtual void set_host(FilterHost* filter_host) OVERRIDE;
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

  // Parses a buffer that contains an INFO & TRACKS element. This method handles
  // calling & clearing |init_cb_| before it returns.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int ParseInfoAndTracks_Locked(const uint8* data, int size);

  // Generates an AVFormatContext for the INFO & TRACKS elements contained
  // in |data|. Returns NULL if parsing |data| fails.
  AVFormatContext* CreateFormatContext(const uint8* data, int size);

  // Sets up |audio_| & |video_| DemuxerStreams based on the data in
  // |format_context_|. Returns false if no valid audio or video stream were
  // found.
  bool SetupStreams();

  // Parse a cluster add add the buffers to the appropriate DemxuerStream.
  // |data| is expected to point to the beginning of a cluster element.
  //
  // Returns -1 if the parse fails.
  // Returns 0 if more data is needed.
  // Returns the number of bytes parsed on success.
  int ParseCluster_Locked(const uint8* data, int size);

  // Reports an error and puts the demuxer in a state where it won't accept more
  // data.
  void ReportError_Locked(PipelineStatus error);

  base::Lock lock_;
  State state_;

  ChunkDemuxerClient* client_;
  PipelineStatusCB init_cb_;
  PipelineStatusCB seek_cb_;

  scoped_refptr<ChunkDemuxerStream> audio_;
  scoped_refptr<ChunkDemuxerStream> video_;

  // Backing buffer for |url_protocol_|.
  scoped_array<uint8> url_protocol_buffer_;

  // Protocol used by |format_context_|. It must outlive the context object.
  scoped_ptr<FFmpegURLProtocol> url_protocol_;

  // FFmpeg format context for this demuxer. It is created by
  // av_open_input_file() during demuxer initialization and cleaned up with
  // DestroyAVFormatContext() in the destructor.
  AVFormatContext* format_context_;

  int64 buffered_bytes_;

  base::TimeDelta duration_;

  scoped_ptr<WebMClusterParser> cluster_parser_;

  // Should a Seek() call wait for more data before calling the
  // callback.
  bool seek_waits_for_data_;

  ByteQueue byte_queue_;

  DISALLOW_COPY_AND_ASSIGN(ChunkDemuxer);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_H_
