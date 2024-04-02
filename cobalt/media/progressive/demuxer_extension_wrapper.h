// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
//
// Contains classes that wrap the Demuxer Cobalt Extension, providing an
// implementation of a Cobalt demuxer. The main API is DemuxerExtensionWrapper.

#ifndef COBALT_MEDIA_PROGRESSIVE_DEMUXER_EXTENSION_WRAPPER_H_
#define COBALT_MEDIA_PROGRESSIVE_DEMUXER_EXTENSION_WRAPPER_H_

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/threading/thread.h"
#include "cobalt/media/progressive/data_source_reader.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/video_decoder_config.h"
#include "starboard/extension/demuxer.h"

namespace cobalt {
namespace media {

// Represents an audio or video stream. Reads data via the demuxer Cobalt
// Extension.
class DemuxerExtensionStream : public ::media::DemuxerStream {
 public:
  // Represents a video stream.
  explicit DemuxerExtensionStream(
      CobaltExtensionDemuxer* demuxer,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      CobaltExtensionDemuxerVideoDecoderConfig config);
  // Represents an audio stream.
  explicit DemuxerExtensionStream(
      CobaltExtensionDemuxer* demuxer,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      CobaltExtensionDemuxerAudioDecoderConfig config);

  // Disallow copy and assign.
  DemuxerExtensionStream(const DemuxerExtensionStream&) = delete;
  DemuxerExtensionStream& operator=(const DemuxerExtensionStream&) = delete;

  ~DemuxerExtensionStream() = default;

  // Functions used by DemuxerExtensionWrapper.
  ::media::Ranges<base::TimeDelta> GetBufferedRanges();
  void EnqueueBuffer(scoped_refptr<::media::DecoderBuffer> buffer);
  void FlushBuffers();
  void Stop();
  base::TimeDelta GetLastBufferTimestamp() const;
  size_t GetTotalBufferSize() const;

  // DemuxerStream implementation:
  void Read(int max_number_of_buffers_to_read, ReadCB read_cb) override;
  ::media::AudioDecoderConfig audio_decoder_config() override;
  ::media::VideoDecoderConfig video_decoder_config() override;
  Type type() const override;

  void EnableBitstreamConverter() override { NOTIMPLEMENTED(); }

  bool SupportsConfigChanges() override { return false; }

 private:
  typedef std::deque<scoped_refptr<::media::DecoderBuffer>> BufferQueue;
  typedef std::deque<ReadCB> ReadQueue;

  CobaltExtensionDemuxer* demuxer_ = nullptr;  // Not owned.
  base::Optional<::media::VideoDecoderConfig> video_config_;
  base::Optional<::media::AudioDecoderConfig> audio_config_;

  // Protects everything below.
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

  BufferQueue buffer_queue_;
  ReadQueue read_queue_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  size_t total_buffer_size_ = 0;
};

// Wraps a DataSourceReader in an even simpler API, where each read increments
// the read location. This better matches the C data source API.
class PositionalDataSource {
 public:
  explicit PositionalDataSource(scoped_refptr<DataSourceReader> reader);

  // Disallow copy and assign.
  PositionalDataSource(const PositionalDataSource&) = delete;
  PositionalDataSource& operator=(const PositionalDataSource&) = delete;

  ~PositionalDataSource();

  void Stop();

  // Reads up to |bytes_requested|, writing the data into |data|.
  int BlockingRead(uint8_t* data, int bytes_requested);

  // Seeks to |position|.
  void SeekTo(int position);

  // Returns the current read position.
  int64_t GetPosition() const;

  // Returns the size of the file.
  //
  // TODO(b/231744342): investigate whether we need to fix
  // DataSourceReader::FileSize(). In testing, it sometimes returned inaccurate
  // results before a file was fully downloaded. That behavior affects what this
  // function returns.
  int64_t GetSize();

 private:
  scoped_refptr<DataSourceReader> reader_;
  int64_t position_ = 0;
};

// Wraps the demuxer Cobalt Extension in the internal media::Demuxer API.
// Instances should be created via the Create method.
class DemuxerExtensionWrapper : public ::media::Demuxer {
 public:
  // Constructs a new DemuxerExtensionWrapper, returning null on failure. If
  // |data_source| or |task_runner| is null, or if a demuxer cannot be created,
  // this will return null. If |demuxer_api| is null, we will attempt to use the
  // corresponding Cobalt extension.
  static std::unique_ptr<DemuxerExtensionWrapper> Create(
      DataSource* data_source,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const CobaltExtensionDemuxerApi* demuxer_api = nullptr);

  // Disallow copy and assign.
  DemuxerExtensionWrapper(const DemuxerExtensionWrapper&) = delete;
  DemuxerExtensionWrapper& operator=(const DemuxerExtensionWrapper&) = delete;

  ~DemuxerExtensionWrapper() override;

  // Demuxer implementation:
  std::vector<::media::DemuxerStream*> GetAllStreams() override;
  std::string GetDisplayName() const override;
  void Initialize(::media::DemuxerHost* host,
                  ::media::PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time,
            ::media::PipelineStatusCallback status_cb) override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void OnEnabledAudioTracksChanged(
      const std::vector<::media::MediaTrack::Id>& track_ids,
      base::TimeDelta curr_time, TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(
      const std::vector<::media::MediaTrack::Id>& track_ids,
      base::TimeDelta curr_time, TrackChangeCB change_completed_cb) override;

  absl::optional<::media::container_names::MediaContainerName>
  GetContainerForMetrics() const override {
    NOTREACHED();
    return absl::nullopt;
  }

 private:
  // Only a forward declaration here, since the specifics of this class are an
  // implementation detail.
  class H264AnnexBConverter;

  // Arguments must not be null.
  explicit DemuxerExtensionWrapper(
      const CobaltExtensionDemuxerApi* demuxer_api,
      CobaltExtensionDemuxer* demuxer,
      std::unique_ptr<PositionalDataSource> data_source,
      std::unique_ptr<CobaltExtensionDemuxerDataSource> c_data_source,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  void OnInitializeDone(::media::PipelineStatusCallback status_cb,
                        CobaltExtensionDemuxerStatus status);
  void Request(::media::DemuxerStream::Type type);
  bool HasStopped();
  void IssueNextRequest();
  void SeekTask(base::TimeDelta time,
                ::media::PipelineStatusCallback status_cb);

  // Returns the range of buffered data. If both audio and video streams are
  // present, this is the intersection of their buffered ranges; otherwise, it
  // is whatever range of data is buffered.
  ::media::Ranges<base::TimeDelta> GetBufferedRanges();

  const CobaltExtensionDemuxerApi* demuxer_api_ = nullptr;  // Not owned.
  // Owned by this class. Construction/destruction is done via demuxer_api_.
  CobaltExtensionDemuxer* impl_ = nullptr;
  std::unique_ptr<PositionalDataSource> data_source_;
  std::unique_ptr<CobaltExtensionDemuxerDataSource> c_data_source_;
  ::media::DemuxerHost* host_ = nullptr;
  mutable base::Lock lock_for_stopped_;
  // Indicates whether Stop has been called.
  bool stopped_ = false;
  bool video_reached_eos_ = false;
  bool audio_reached_eos_ = false;
  bool flushing_ = false;

  base::Optional<DemuxerExtensionStream> video_stream_;
  base::Optional<DemuxerExtensionStream> audio_stream_;

  std::unique_ptr<H264AnnexBConverter> h264_converter_;

  // Thread for blocking I/O operations.
  base::Thread blocking_thread_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DemuxerExtensionWrapper> weak_factory_{this};
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_DEMUXER_EXTENSION_WRAPPER_H_
