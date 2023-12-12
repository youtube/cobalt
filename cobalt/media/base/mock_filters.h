// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_MOCK_FILTERS_H_
#define COBALT_MEDIA_BASE_MOCK_FILTERS_H_

#include "cobalt/media/base/sbplayer_pipeline.h"
#include "string"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "vector"

namespace cobalt {
namespace media {

using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::MediaTrack;
using ::media::PipelineStatusCallback;


class MockDemuxer : public Demuxer {
 public:
  MockDemuxer();

  MockDemuxer(const MockDemuxer&) = delete;
  MockDemuxer& operator=(const MockDemuxer&) = delete;

  ~MockDemuxer() override;

  // Demuxer implementation.
  std::string GetDisplayName() const override;

  // DemuxerType GetDemuxerType() const override;

  void Initialize(DemuxerHost* host, PipelineStatusCallback cb) override {
    OnInitialize(host, cb);
  }
  MOCK_METHOD(void, OnInitialize,
              (DemuxerHost * host, PipelineStatusCallback& cb), ());
  MOCK_METHOD(void, StartWaitingForSeek, (base::TimeDelta), (override));
  MOCK_METHOD(void, CancelPendingSeek, (base::TimeDelta), (override));
  void Seek(base::TimeDelta time, PipelineStatusCallback cb) override {
    OnSeek(time, cb);
  }
  MOCK_METHOD(bool, IsSeekable, (), (const override));
  MOCK_METHOD(void, OnSeek, (base::TimeDelta time, PipelineStatusCallback& cb),
              ());
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void, AbortPendingReads, (), (override));
  MOCK_METHOD(std::vector<DemuxerStream*>, GetAllStreams, (), (override));

  MOCK_METHOD(base::TimeDelta, GetStartTime, (), (const, override));
  MOCK_METHOD(base::Time, GetTimelineOffset, (), (const, override));
  MOCK_METHOD(int64_t, GetMemoryUsage, (), (const, override));
  MOCK_METHOD(absl::optional<::media::container_names::MediaContainerName>,
              GetContainerForMetrics, (), (const, override));
  MOCK_METHOD(void, OnEnabledAudioTracksChanged,
              (const std::vector<MediaTrack::Id>&, base::TimeDelta,
               TrackChangeCB),
              (override));
  MOCK_METHOD(void, OnSelectedVideoTrackChanged,
              (const std::vector<MediaTrack::Id>&, base::TimeDelta,
               TrackChangeCB),
              (override));

  // MOCK_METHOD(void, SetPlaybackRate, (double), (override));
};


}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_MOCK_FILTERS_H_
