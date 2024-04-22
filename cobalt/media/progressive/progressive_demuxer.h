// Copyright 2012 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_PROGRESSIVE_PROGRESSIVE_DEMUXER_H_
#define COBALT_MEDIA_PROGRESSIVE_PROGRESSIVE_DEMUXER_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "cobalt/media/progressive/progressive_parser.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_log.h"
#include "media/base/ranges.h"

namespace media {
class DecoderBuffer;
}  // namespace media

namespace cobalt {
namespace media {

class ProgressiveDemuxer;

class ProgressiveDemuxerStream : public ::media::DemuxerStream {
 public:
  typedef ::media::AudioDecoderConfig AudioDecoderConfig;
  typedef ::media::DecoderBuffer DecoderBuffer;
  typedef ::media::VideoDecoderConfig VideoDecoderConfig;

  ProgressiveDemuxerStream(ProgressiveDemuxer* demuxer, Type type);

  // DemuxerStream implementation
  void Read(uint32_t count, ReadCB read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override { return false; }

  // Functions used by ProgressiveDemuxer
  ::media::Ranges<base::TimeDelta> GetBufferedRanges();
  void EnqueueBuffer(scoped_refptr<DecoderBuffer> buffer);
  void FlushBuffers();
  void Stop();
  base::TimeDelta GetLastBufferTimestamp() const;
  size_t GetTotalBufferSize() const;
  size_t GetTotalBufferCount() const;

 private:
  // The Ranges object doesn't offer a complement object so we rebuild
  // enqueued ranges from the union of all of the buffers in the queue.
  // Call me whenever _removing_ data from buffer_queue_.
  void RebuildEnqueuedRanges_Locked();

  // non-owning pointer to avoid circular reference
  ProgressiveDemuxer* demuxer_;
  Type type_;

  // Used to protect everything below.
  mutable base::Lock lock_;
  // Keeps track of all time ranges this object has seen since creation.
  // The demuxer uses these ranges to update the pipeline about what data
  // it has demuxed.
  ::media::Ranges<base::TimeDelta> buffered_ranges_;
  // The last timestamp of buffer enqueued. This is used in two places:
  //   1. Used with the timestamp of the current frame to calculate the
  //      buffer range.
  //   2. Used by the demuxer to deteminate what type of frame to get next.
  base::TimeDelta last_buffer_timestamp_ = ::media::kNoTimestamp;
  bool stopped_ = false;

  typedef std::deque<scoped_refptr<DecoderBuffer>> BufferQueue;
  BufferQueue buffer_queue_;

  typedef std::deque<ReadCB> ReadQueue;
  ReadQueue read_queue_;

  size_t total_buffer_size_ = 0;
  size_t total_buffer_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ProgressiveDemuxerStream);
};

class MEDIA_EXPORT ProgressiveDemuxer : public ::media::Demuxer {
 public:
  typedef ::media::AudioDecoderConfig AudioDecoderConfig;
  typedef ::media::DecoderBuffer DecoderBuffer;
  typedef ::media::DemuxerHost DemuxerHost;
  typedef ::media::DemuxerStream DemuxerStream;
  typedef ::media::MediaLog MediaLog;
  typedef ::media::PipelineStatus PipelineStatus;
  typedef ::media::PipelineStatusCallback PipelineStatusCallback;
  typedef ::media::VideoDecoderConfig VideoDecoderConfig;

  ProgressiveDemuxer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      DataSource* data_source, MediaLog* const media_log);
  ~ProgressiveDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override { return "ProgressiveDemuxer"; }
  ::media::DemuxerType GetDemuxerType() const override {
    // kFFmpegDemuxer is used in Chromium media for progressive demuxing.
    return ::media::DemuxerType::kFFmpegDemuxer;
  }
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override {}
  void StartWaitingForSeek(base::TimeDelta seek_time) override {}
  void CancelPendingSeek(base::TimeDelta seek_time) override {}
  void Stop() override;
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override;
  bool IsSeekable() const override { return true; }
  std::vector<DemuxerStream*> GetAllStreams() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override { return base::Time(); }
  int64_t GetMemoryUsage() const override {
    NOTREACHED();
    return 0;
  }
  absl::optional<::media::container_names::MediaContainerName>
  GetContainerForMetrics() const override {
    NOTREACHED();
    return absl::nullopt;
  }
  void OnEnabledAudioTracksChanged(
      const std::vector<::media::MediaTrack::Id>& track_ids,
      base::TimeDelta currTime, TrackChangeCB change_completed_cb) override {
    NOTREACHED();
  }
  void OnSelectedVideoTrackChanged(
      const std::vector<::media::MediaTrack::Id>& track_ids,
      base::TimeDelta currTime, TrackChangeCB change_completed_cb) override {
    NOTREACHED();
  }
  void SetPlaybackRate(double rate) override { NOTREACHED(); }

  // TODO: Consider move the following functions to private section.

  // Issues a task to the demuxer to identify the next buffer of provided type
  // in the stream, allocate memory to contain that buffer, download the bytes
  // in to it, and enqueue the data in the appropriate demuxer stream.
  void Request(DemuxerStream::Type type);

  // The DemuxerStream objects ask their parent ProgressiveDemuxer stream class
  // for these configuration data rather than duplicating in the child classes
  const AudioDecoderConfig& AudioConfig();
  const VideoDecoderConfig& VideoConfig();

  // Provide access to ProgressiveDemuxerStream.
  bool RunsTasksInCurrentSequence() const {
    return task_runner_->RunsTasksInCurrentSequence();
  }

 private:
  void ParseConfigDone(PipelineStatusCallback status_cb, PipelineStatus status);
  bool HasStopCalled();

  // methods that perform blocking I/O, and are therefore run on the
  // blocking_thread_ download enough of the stream to parse the configuration.
  void ParseConfigBlocking(PipelineStatusCallback status_cb);
  void AllocateBuffer();
  void Download(scoped_refptr<DecoderBuffer> buffer);
  void IssueNextRequest();
  void SeekTask(base::TimeDelta time, PipelineStatusCallback cb);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DemuxerHost* host_;

  // Thread on which all blocking operations are executed.
  base::Thread blocking_thread_;
  DataSource* data_source_;
  MediaLog* media_log_;
  scoped_refptr<DataSourceReader> reader_;

  base::Lock lock_for_stopped_;
  bool stopped_;
  bool flushing_;

  std::unique_ptr<ProgressiveDemuxerStream> audio_demuxer_stream_;
  std::unique_ptr<ProgressiveDemuxerStream> video_demuxer_stream_;
  scoped_refptr<ProgressiveParser> parser_;

  scoped_refptr<AvcAccessUnit> requested_au_;
  bool audio_reached_eos_;
  bool video_reached_eos_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_PROGRESSIVE_DEMUXER_H_
