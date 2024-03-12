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

#include "cobalt/media/player/web_media_player_impl.h"

#include <utility>

#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "cobalt/media/base/mock_filters.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "starboard/drm.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/chromium/media/base/media_log.h"

namespace cobalt {
namespace media {

using ::cobalt::media::DefaultSbPlayerInterface;
using ::cobalt::media::SbPlayerInterface;
using ::cobalt::media::WebMediaPlayerImpl;
using ::media::ChunkDemuxer;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::StrictMock;


namespace {
SbDecodeTargetGraphicsContextProvider*
FakeGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
// copied from web_media_player_impl.cc
const float kMinRate = 0.0625f;
const float kMaxRate = 16.0f;
}  // namespace

class MockWebMediaPlayerClient : public cobalt::media::WebMediaPlayerClient {
 public:
  MockWebMediaPlayerClient() = default;

  MockWebMediaPlayerClient(const MockWebMediaPlayerClient&) = delete;
  MockWebMediaPlayerClient(const MockWebMediaPlayerClient&&) = delete;
  MockWebMediaPlayerClient& operator=(const MockWebMediaPlayerClient&) = delete;
  MockWebMediaPlayerClient& operator=(const MockWebMediaPlayerClient&&) =
      delete;

  // WebMediaPlayerClient methods
  MOCK_METHOD0(NetworkStateChanged, void());
  MOCK_METHOD1(NetworkError, void(const std::string& message));
  MOCK_METHOD0(ReadyStateChanged, void());
  MOCK_METHOD1(TimeChanged, void(bool eos_played));
  MOCK_METHOD0(DurationChanged, void());
  MOCK_METHOD0(OutputModeChanged, void());
  MOCK_METHOD0(ContentSizeChanged, void());
  MOCK_METHOD0(PlaybackStateChanged, void());
  MOCK_CONST_METHOD0(Volume, float());
  MOCK_METHOD1(SourceOpened, void(ChunkDemuxer* chunk_demuxer));
  MOCK_CONST_METHOD0(SourceURL, std::string());
  MOCK_CONST_METHOD0(MaxVideoCapabilities, std::string());
  MOCK_METHOD0(PreferDecodeToTexture, bool());
  MOCK_METHOD3(EncryptedMediaInitDataEncountered,
               void(const char* init_data_type, const unsigned char* init_data,
                    unsigned int init_data_length));
};

class MockWebMediaPlayerDelegate
    : public cobalt::media::WebMediaPlayerDelegate {
 public:
  MockWebMediaPlayerDelegate() = default;

  MockWebMediaPlayerDelegate(const MockWebMediaPlayerDelegate&) = delete;
  MockWebMediaPlayerDelegate(const MockWebMediaPlayerDelegate&&) = delete;
  MockWebMediaPlayerDelegate& operator=(const MockWebMediaPlayerDelegate&) =
      delete;
  MockWebMediaPlayerDelegate& operator=(const MockWebMediaPlayerDelegate&&) =
      delete;

  // WebMediaPlayerDelegate methods
  MOCK_METHOD1(RegisterPlayer, void(WebMediaPlayer* player));
  MOCK_METHOD1(UnregisterPlayer, void(WebMediaPlayer* player));
};

class WebMediaPlayerImplTest : public ::testing::Test {
 public:
  WebMediaPlayerImplTest()
      : sbplayer_interface_(
            std::make_unique<StrictMock<MockSbPlayerInterface>>()) {
    EXPECT_CALL(delegate_, RegisterPlayer(_));
    EXPECT_CALL(delegate_, UnregisterPlayer(_));

    wmpi_ = std::make_unique<WebMediaPlayerImpl>(
        sbplayer_interface_.get(), nullptr,
        base::Bind(FakeGetSbDecodeTargetGraphicsContextProvider), &client_,
        &delegate_, true, false, true,
#if SB_API_VERSION >= 15
        kSbPlayerWriteDurationLocal, kSbPlayerWriteDurationRemote,
#endif  // SB_API_VERSION >= 15
        &media_log_);
  }

 protected:
  bool HasAudio() { return wmpi_->HasAudio(); }
  bool HasVideo() { return wmpi_->HasVideo(); }
  bool IsPaused() { return wmpi_->IsPaused(); }
  float GetPlaybackRate() { return wmpi_->GetPlaybackRate(); }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // The client interface used by |wmpi_|.
  NiceMock<MockWebMediaPlayerClient> client_;
  NiceMock<MockWebMediaPlayerDelegate> delegate_;

  std::unique_ptr<SbPlayerInterface> sbplayer_interface_;

  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;

  ::media::MediaLog media_log_;
};

TEST_F(WebMediaPlayerImplTest, ConstructAndDestroy) {
  EXPECT_TRUE(IsPaused());
  EXPECT_FALSE(HasAudio());
  EXPECT_FALSE(HasVideo());
  EXPECT_EQ(GetPlaybackRate(), 0);
}

TEST_F(WebMediaPlayerImplTest, LoadMediaSource) {
  EXPECT_CALL(client_, Volume()).Times(1);
  EXPECT_CALL(client_, NetworkStateChanged()).Times(1);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(1);
  EXPECT_CALL(client_, PreferDecodeToTexture()).Times(1);
  EXPECT_CALL(client_, MaxVideoCapabilities()).Times(1);
  wmpi_->LoadMediaSource();
  EXPECT_TRUE(wmpi_->state_.is_media_source);
  EXPECT_TRUE(wmpi_->state_.starting);
}

TEST_F(WebMediaPlayerImplTest, LoadProgressive) {
  EXPECT_CALL(client_, Volume()).Times(1);
  EXPECT_CALL(client_, NetworkStateChanged()).Times(1);
  EXPECT_CALL(client_, ReadyStateChanged()).Times(1);
  EXPECT_CALL(client_, PreferDecodeToTexture()).Times(1);
  EXPECT_CALL(client_, MaxVideoCapabilities()).Times(1);
  GURL url("http://youtube.com/watch?v=w5tbgPJxyGA");
  auto datasource = std::make_unique<NiceMock<MockDataSource>>();
  EXPECT_CALL(*datasource, SetDownloadingStatusCB(_));
  wmpi_->LoadProgressive(url, std::move(datasource));
  EXPECT_TRUE(wmpi_->state_.is_progressive);
  EXPECT_TRUE(wmpi_->state_.starting);
}


TEST_F(WebMediaPlayerImplTest, PlayPause) {
  EXPECT_TRUE(wmpi_->state_.paused);
  wmpi_->Play();
  EXPECT_FALSE(wmpi_->state_.paused);
  wmpi_->Pause();
  EXPECT_TRUE(wmpi_->state_.paused);
}

TEST_F(WebMediaPlayerImplTest, Seek) {
  EXPECT_FALSE(wmpi_->state_.seeking);
  EXPECT_FALSE(wmpi_->state_.pending_seek);
  wmpi_->Seek(2);
  EXPECT_TRUE(wmpi_->state_.seeking);
  EXPECT_FALSE(wmpi_->state_.pending_seek);
  wmpi_->Seek(2);
  EXPECT_TRUE(wmpi_->state_.pending_seek);
  EXPECT_EQ(2, wmpi_->state_.pending_seek_seconds);
}

TEST_F(WebMediaPlayerImplTest, SetRate) {
  wmpi_->SetRate(1);
  EXPECT_EQ(1, wmpi_->state_.playback_rate);
  wmpi_->SetRate(-1);
  EXPECT_EQ(1, wmpi_->state_.playback_rate);
  wmpi_->SetRate(0);
  EXPECT_EQ(0, wmpi_->state_.playback_rate);
  wmpi_->SetRate(kMinRate);
  EXPECT_EQ(kMinRate, wmpi_->state_.playback_rate);
  wmpi_->SetRate(kMinRate / 2);
  EXPECT_EQ(kMinRate, wmpi_->state_.playback_rate);
  wmpi_->SetRate(kMaxRate);
  EXPECT_EQ(kMaxRate, wmpi_->state_.playback_rate);
  wmpi_->SetRate(kMaxRate * 2);
  EXPECT_EQ(kMaxRate, wmpi_->state_.playback_rate);
}

TEST_F(WebMediaPlayerImplTest, SetVolume) {
  wmpi_->SetVolume(1);
  EXPECT_EQ(1, wmpi_->pipeline_->GetVolume());
  wmpi_->SetVolume(0);
  EXPECT_EQ(0, wmpi_->pipeline_->GetVolume());
  wmpi_->SetVolume(-1);
  EXPECT_EQ(0, wmpi_->pipeline_->GetVolume());
  wmpi_->SetVolume(2);
  EXPECT_EQ(0, wmpi_->pipeline_->GetVolume());
}

TEST_F(WebMediaPlayerImplTest, GetNaturalHeightAndWidth) {
  EXPECT_EQ(0, wmpi_->GetNaturalHeight());
  EXPECT_EQ(0, wmpi_->GetNaturalWidth());
}

// // Callback to notify that a DRM system is ready.
// typedef base::Callback<void(SbDrmSystem)> DrmSystemReadyCB;
// Callback to set an DrmSystemReadyCB.
// typedef base::Callback<void(const DrmSystemReadyCB&)> SetDrmSystemReadyCB;

// class MockSetDrmSystemReadyCallback {
// public:
//     MOCK_METHOD1(Run, void(SbDrmSystem));
// };
//
TEST_F(WebMediaPlayerImplTest, SetDRMSystemAndCheckCB) {
  using MockDrmSystemReadyCallback = base::Callback<void(SbDrmSystem)>;
  base::MockCallback<MockDrmSystemReadyCallback> mockSetDrmSystemReadyCb;
  EXPECT_CALL(mockSetDrmSystemReadyCb, Run(_));
  wmpi_->SetDrmSystemReadyCB(mockSetDrmSystemReadyCb.Get());
}

}  // namespace media
}  // namespace cobalt
