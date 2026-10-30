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

#include "media/starboard/url_player_demuxer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/eme_constants.h"
#include "media/base/media_util.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/pipeline_status.h"
#include "media/base/ranges.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/demuxer_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace media {

namespace {

using ::testing::_;
using ::testing::NiceMock;
using ::testing::SaveArg;
using ::testing::StrictMock;

const char kHlsUrl[] = "https://example.com/master.m3u8";

class UrlPlayerDemuxerTest : public ::testing::Test {
 protected:
  UrlPlayerDemuxerTest()
      : demuxer_(task_environment_.GetMainThreadTaskRunner(), GURL(kHlsUrl)) {}

  DemuxerStream* GetStream(DemuxerStream::Type type) {
    for (DemuxerStream* stream : demuxer_.GetAllStreams()) {
      if (stream->type() == type) {
        return stream;
      }
    }
    return nullptr;
  }

  void InitializeDemuxer() {
    demuxer_.Initialize(&host_, base::DoNothing());
    base::RunLoop().RunUntilIdle();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  StrictMock<MockDemuxerHost> host_;
  UrlPlayerDemuxer demuxer_;
};

TEST_F(UrlPlayerDemuxerTest, IsUrlPlayerDemuxer) {
  EXPECT_EQ(demuxer_.GetDemuxerType(), DemuxerType::kUrlPlayerDemuxer);
  ASSERT_EQ(demuxer_.AsUrlPlayerMediaResource(), &demuxer_);
  EXPECT_EQ(demuxer_.AsUrlPlayerMediaResource()->GetMediaUrl(), GURL(kHlsUrl));
}

TEST_F(UrlPlayerDemuxerTest, HasAudioAndVideoStreams) {
  EXPECT_TRUE(GetStream(DemuxerStream::AUDIO));
  EXPECT_TRUE(GetStream(DemuxerStream::VIDEO));
}

TEST_F(UrlPlayerDemuxerTest, StreamsAreUnencrypted) {
  ASSERT_TRUE(GetStream(DemuxerStream::AUDIO));
  ASSERT_TRUE(GetStream(DemuxerStream::VIDEO));
  EXPECT_FALSE(
      GetStream(DemuxerStream::AUDIO)->audio_decoder_config().is_encrypted());
  EXPECT_FALSE(
      GetStream(DemuxerStream::VIDEO)->video_decoder_config().is_encrypted());
}

TEST_F(UrlPlayerDemuxerTest, PlatformDurationChangeReachesHost) {
  InitializeDemuxer();
  EXPECT_CALL(host_, SetDuration(base::Seconds(20)));
  demuxer_.OnPlatformDurationChange(base::Seconds(20));
}

TEST_F(UrlPlayerDemuxerTest, PlatformBufferedRangesReachHost) {
  InitializeDemuxer();
  Ranges<base::TimeDelta> ranges;
  EXPECT_CALL(host_, OnBufferedTimeRangesChanged(_))
      .WillOnce(SaveArg<0>(&ranges));
  demuxer_.OnPlatformBufferedRangesChange(base::Seconds(5), base::Seconds(10));

  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges.start(0), base::Seconds(5));
  EXPECT_EQ(ranges.end(0), base::Seconds(15));
}

TEST_F(UrlPlayerDemuxerTest, PlatformEncryptedInitDataRunsCallback) {
  const std::vector<uint8_t> init_data = {1, 2, 3};
  base::MockRepeatingCallback<void(EmeInitDataType,
                                   const std::vector<uint8_t>&)>
      encrypted_cb;
  demuxer_.SetEncryptedMediaInitDataCB(encrypted_cb.Get());
  EXPECT_CALL(encrypted_cb, Run(EmeInitDataType::CENC, init_data));
  demuxer_.OnPlatformEncryptedMediaInitData(EmeInitDataType::CENC, init_data);
}

class MockDemuxerManagerClient : public DemuxerManager::Client {
 public:
  MOCK_METHOD2(OnEncryptedMediaInitData,
               void(EmeInitDataType, const std::vector<uint8_t>&));
  MOCK_METHOD1(OnChunkDemuxerOpened, void(ChunkDemuxer*));
  MOCK_METHOD0(OnProgress, void());
  MOCK_METHOD1(OnError, void(PipelineStatus));
  MOCK_METHOD0(StopForDemuxerReset, void());
  MOCK_METHOD0(RestartForHls, void());
  MOCK_CONST_METHOD0(IsSecurityOriginCryptographic, bool());
#if BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)
  MOCK_METHOD1(AddMediaTrack, void(const MediaTrack&));
  MOCK_METHOD1(RemoveMediaTrack, void(const MediaTrack&));
#endif  // BUILDFLAG(ENABLE_FFMPEG) || BUILDFLAG(ENABLE_HLS_DEMUXER)
#if BUILDFLAG(ENABLE_HLS_DEMUXER)
  MOCK_METHOD0(GetHlsDataSourceProvider,
               base::SequenceBound<HlsDataSourceProvider>());
#endif  // BUILDFLAG(ENABLE_HLS_DEMUXER)
  MOCK_METHOD0(CouldPlayIfEnoughData, bool());
  MOCK_METHOD1(MakeDemuxerThreadDumper, void(Demuxer*));
  MOCK_CONST_METHOD0(CurrentTime, double());
  MOCK_METHOD1(UpdateLoadedUrl, void(const GURL&));
  MOCK_METHOD1(DemuxerRequestsSeek, void(base::TimeDelta));
};

class UrlPlayerDemuxerManagerTest : public ::testing::Test {
 protected:
  UrlPlayerDemuxerManagerTest()
      : demuxer_manager_(&client_,
                         task_environment_.GetMainThreadTaskRunner(),
                         &media_log_,
                         /*enable_instant_source_buffer_gc=*/false,
                         std::make_unique<UrlPlayerDemuxer>(
                             task_environment_.GetMainThreadTaskRunner(),
                             GURL(kHlsUrl))) {}

  // Creates the demuxer through DemuxerManager and stores it in `demuxer_`.
  void CreateDemuxer() {
    PipelineStatus status = demuxer_manager_.CreateDemuxer(
        /*load_media_source=*/false, DataSource::AUTO,
        /*needs_first_frame=*/false,
        base::BindOnce(
            [](raw_ptr<Demuxer>* out_demuxer, Demuxer* demuxer,
               Pipeline::StartType type, bool is_streaming, bool is_static) {
              *out_demuxer = demuxer;
              return PipelineStatus(PIPELINE_OK);
            },
            &demuxer_),
        {});
    EXPECT_TRUE(status.is_ok());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  NullMediaLog media_log_;
  NiceMock<MockDemuxerManagerClient> client_;
  DemuxerManager demuxer_manager_;
  raw_ptr<Demuxer> demuxer_ = nullptr;
};

TEST_F(UrlPlayerDemuxerManagerTest, EncryptedInitDataReachesClient) {
  CreateDemuxer();
  const std::vector<uint8_t> init_data = {1, 2, 3};
  ASSERT_TRUE(demuxer_);
  ASSERT_EQ(demuxer_->GetDemuxerType(), DemuxerType::kUrlPlayerDemuxer);

  EXPECT_CALL(client_,
              OnEncryptedMediaInitData(EmeInitDataType::CENC, init_data));
  static_cast<UrlPlayerDemuxer*>(demuxer_.get())
      ->OnPlatformEncryptedMediaInitData(EmeInitDataType::CENC, init_data);
  base::RunLoop().RunUntilIdle();
}

}  // namespace

}  // namespace media
