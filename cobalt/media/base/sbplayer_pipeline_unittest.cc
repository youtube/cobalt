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

#include "cobalt/media/base/sbplayer_pipeline.h"

#include "base/test/scoped_task_environment.h"
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {

using ::cobalt::media::DefaultSbPlayerInterface;
using ::cobalt::media::SbPlayerInterface;
using ::media::Demuxer;
using ::media::DemuxerHost;
using ::media::DemuxerStream;
using ::media::MediaTrack;
using ::media::PipelineStatusCallback;
using ::testing::AtLeast;
using ::testing::NiceMock;
using ::testing::StrictMock;


namespace {
SbDecodeTargetGraphicsContextProvider*
MockGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
}  // namespace


class SbPlayerPipelineTest : public ::testing::Test {
 public:
  SbPlayerPipelineTest()
      : demuxer_(std::make_unique<StrictMock<MockDemuxer>>()),
        sbplayer_interface_(new DefaultSbPlayerInterface),
        decode_target_provider_(new DecodeTargetProvider()) {
    pipeline_ = new SbPlayerPipeline(
        sbplayer_interface_.get(), nullptr,
        task_environment_.GetMainThreadTaskRunner(),
        base::Bind(MockGetSbDecodeTargetGraphicsContextProvider), true, false,
        true,
#if SB_API_VERSION >= 15
        10, 10,
#endif  // SB_API_VERSION >= 15
        nullptr, &media_metrics_provider_, decode_target_provider_.get());
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_;

  scoped_refptr<DecodeTargetProvider> decode_target_provider_;
  scoped_refptr<Pipeline> pipeline_;

  std::unique_ptr<StrictMock<MockDemuxer>> demuxer_;

  std::unique_ptr<SbPlayerInterface> sbplayer_interface_;

  MediaMetricsProvider media_metrics_provider_;
};


TEST_F(SbPlayerPipelineTest, ConstructAndDestroy) {
  LOG(INFO) << "YO THOR _ SB PKAYER PIPELINE TEST\n";
}


}  // namespace media
}  // namespace cobalt
