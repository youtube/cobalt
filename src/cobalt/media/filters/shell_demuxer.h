// Copyright 2012 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_FILTERS_SHELL_DEMUXER_H_
#define COBALT_MEDIA_FILTERS_SHELL_DEMUXER_H_

#include <deque>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/threading/thread.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/demuxer.h"
#include "cobalt/media/base/demuxer_stream.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/ranges.h"
#include "cobalt/media/filters/shell_parser.h"

namespace cobalt {
namespace media {

class DecoderBuffer;
class ShellDemuxer;

class ShellDemuxerStream : public DemuxerStream {
 public:
  ShellDemuxerStream(ShellDemuxer* demuxer, Type type);

  // DemuxerStream implementation
  void Read(const ReadCB& read_cb) OVERRIDE;
  AudioDecoderConfig audio_decoder_config() OVERRIDE;
  VideoDecoderConfig video_decoder_config() OVERRIDE;
  Type type() const OVERRIDE;
  void EnableBitstreamConverter() OVERRIDE;
  bool SupportsConfigChanges() OVERRIDE { return false; }
  VideoRotation video_rotation() OVERRIDE { return VIDEO_ROTATION_0; }
  bool enabled() const OVERRIDE { return true; }
  void set_enabled(bool enabled, base::TimeDelta timestamp) OVERRIDE {
    DCHECK(enabled);
  }
  void SetStreamStatusChangeCB(const StreamStatusChangeCB& cb) OVERRIDE {
    NOTREACHED();
  }

  // Functions used by ShellDemuxer
  Ranges<base::TimeDelta> GetBufferedRanges();
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);
  void FlushBuffers();
  void Stop();
  base::TimeDelta GetLastBufferTimestamp() const;
  size_t GetTotalBufferSize() const;

 private:
  // The Ranges object doesn't offer a complement object so we rebuild
  // enqueued ranges from the union of all of the buffers in the queue.
  // Call me whenever _removing_ data from buffer_queue_.
  void RebuildEnqueuedRanges_Locked();

  // non-owning pointer to avoid circular reference
  ShellDemuxer* demuxer_;
  Type type_;

  // Used to protect everything below.
  mutable base::Lock lock_;
  // Keeps track of all time ranges this object has seen since creation.
  // The demuxer uses these ranges to update the pipeline about what data
  // it has demuxed.
  Ranges<base::TimeDelta> buffered_ranges_;
  // The last timestamp of buffer enqueued. This is used in two places:
  //   1. Used with the timestamp of the current frame to calculate the
  //      buffer range.
  //   2. Used by the demuxer to deteminate what type of frame to get next.
  base::TimeDelta last_buffer_timestamp_;
  bool stopped_;

  typedef std::deque<scoped_refptr<DecoderBuffer> > BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCB> ReadQueue;
  ReadQueue read_queue_;

  size_t total_buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellDemuxerStream);
};

class MEDIA_EXPORT ShellDemuxer : public Demuxer {
 public:
  ShellDemuxer(const scoped_refptr<base::MessageLoopProxy>& message_loop,
               DecoderBuffer::Allocator* buffer_allocator,
               DataSource* data_source,
               const scoped_refptr<MediaLog>& media_log);
  ~ShellDemuxer() OVERRIDE;

  // Demuxer implementation.
  std::string GetDisplayName() const OVERRIDE { return "ShellDemuxer"; }
  void Initialize(DemuxerHost* host, const PipelineStatusCB& status_cb,
                  bool enable_text_tracks) OVERRIDE;
  void AbortPendingReads() OVERRIDE {}
  void StartWaitingForSeek(base::TimeDelta seek_time) OVERRIDE {}
  void CancelPendingSeek(base::TimeDelta seek_time) OVERRIDE {}
  void Stop() OVERRIDE;
  void Seek(base::TimeDelta time, const PipelineStatusCB& cb) OVERRIDE;
  DemuxerStream* GetStream(DemuxerStream::Type type) OVERRIDE;
  base::TimeDelta GetStartTime() const OVERRIDE;
  base::Time GetTimelineOffset() const OVERRIDE { return base::Time(); }
  int64_t GetMemoryUsage() const OVERRIDE {
    NOTREACHED();
    return 0;
  }
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta currTime) OVERRIDE {
    NOTREACHED();
  }
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta currTime) OVERRIDE {
    NOTREACHED();
  }

  // TODO: Consider move the following functions to private section.

  // Issues a task to the demuxer to identify the next buffer of provided type
  // in the stream, allocate memory to contain that buffer, download the bytes
  // in to it, and enqueue the data in the appropriate demuxer stream.
  void Request(DemuxerStream::Type type);

  // The DemuxerStream objects ask their parent ShellDemuxer stream class
  // for these configuration data rather than duplicating in the child classes
  const AudioDecoderConfig& AudioConfig();
  const VideoDecoderConfig& VideoConfig();

  // Provide access to ShellDemuxerStream.
  bool MessageLoopBelongsToCurrentThread() const;

 private:
  void ParseConfigDone(const PipelineStatusCB& status_cb,
                       PipelineStatus status);
  void DataSourceStopped(const base::Closure& callback);

  // methods that perform blocking I/O, and are therefore run on the
  // blocking_thread_
  // download enough of the stream to parse the configuration. returns
  // false on error.
  PipelineStatus ParseConfigBlocking(const PipelineStatusCB& status_cb);
  void AllocateBuffer();
  void Download(scoped_refptr<DecoderBuffer> buffer);
  void IssueNextRequest();
  void SeekTask(base::TimeDelta time, const PipelineStatusCB& cb);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  DecoderBuffer::Allocator* buffer_allocator_;
  DemuxerHost* host_;

  // Thread on which all blocking operations are executed.
  base::Thread blocking_thread_;
  DataSource* data_source_;
  scoped_refptr<MediaLog> media_log_;
  scoped_refptr<ShellDataSourceReader> reader_;

  bool stopped_;
  bool flushing_;

  scoped_ptr<ShellDemuxerStream> audio_demuxer_stream_;
  scoped_ptr<ShellDemuxerStream> video_demuxer_stream_;
  scoped_refptr<ShellParser> parser_;

  scoped_refptr<ShellAU> requested_au_;
  bool audio_reached_eos_;
  bool video_reached_eos_;

  base::WeakPtrFactory<ShellDemuxer> weak_ptr_factory_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_FILTERS_SHELL_DEMUXER_H_
