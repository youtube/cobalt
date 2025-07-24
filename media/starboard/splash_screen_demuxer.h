// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_SPLASH_SCREEN_DEMUXER_H_
#define MEDIA_STARBOARD_SPLASH_SCREEN_DEMUXER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/demuxer.h"
#include "media/base/demuxer_stream.h"
#include "mkvparser/mkvparser.h"
#include "mkvparser/mkvreader.h"

namespace media {

class SplashScreenDemuxerStream;

class SplashScreenDemuxer : public Demuxer {
 public:
  SplashScreenDemuxer(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);
  ~SplashScreenDemuxer() override;

  // Demuxer implementation.
  std::vector<DemuxerStream*> GetAllStreams() override;
  std::string GetDisplayName() const override;
  void Initialize(DemuxerHost* host, PipelineStatusCallback status_cb) override;
  void AbortPendingReads() override;
  void StartWaitingForSeek(base::TimeDelta seek_time) override;
  void CancelPendingSeek(base::TimeDelta seek_time) override;
  void Seek(base::TimeDelta time, PipelineStatusCallback status_cb) override;
  void Stop() override;
  base::TimeDelta GetStartTime() const override;
  base::Time GetTimelineOffset() const override;
  int64_t GetMemoryUsage() const override;
  void OnEnabledAudioTracksChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;
  void OnSelectedVideoTrackChanged(const std::vector<MediaTrack::Id>& track_ids,
                                   base::TimeDelta curr_time,
                                   TrackChangeCB change_completed_cb) override;

  DemuxerType GetDemuxerType() const override;
  bool IsSeekable() const override;
  absl::optional<container_names::MediaContainerName> GetContainerForMetrics()
      const override;
  void SetPlaybackRate(double rate) override;

  VideoDecoderConfig video_decoder_config() const;
  void Read(DemuxerStream::Type type, DemuxerStream::ReadCB read_cb);

 private:
  void FindVideoTrack();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  DemuxerHost* host_ = nullptr;

  std::unique_ptr<mkvparser::MkvReader> reader_;
  std::unique_ptr<mkvparser::Segment> segment_;
  const mkvparser::VideoTrack* video_track_ = nullptr;
  const mkvparser::Cluster* current_cluster_ = nullptr;
  const mkvparser::BlockEntry* current_block_entry_ = nullptr;

  std::unique_ptr<SplashScreenDemuxerStream> video_stream_;

  base::WeakPtrFactory<SplashScreenDemuxer> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_STARBOARD_SPLASH_SCREEN_DEMUXER_H_
