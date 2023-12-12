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

#include "base/test/scoped_task_environment.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {

using ::cobalt::media::DefaultSbPlayerInterface;
using ::cobalt::media::SbPlayerInterface;
using ::cobalt::media::WebMediaPlayerImpl;
using ::media::ChunkDemuxer;
using ::testing::AtLeast;
using ::testing::NiceMock;


namespace {
SbDecodeTargetGraphicsContextProvider*
MockGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
}  // namespace

class MockWebMediaPlayerClient : public cobalt::media::WebMediaPlayerClient,
                                 public cobalt::media::WebMediaPlayerDelegate {
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
  MOCK_METHOD3(EncryptedMediaInitDataEncountered,
               void(const char* init_data_type, const unsigned char* init_data,
                    unsigned int init_data_length));

  // WebMediaPlayerDelegate methods
  MOCK_METHOD1(RegisterPlayer, void(WebMediaPlayer* player));
  MOCK_METHOD1(UnregisterPlayer, void(WebMediaPlayer* player));
};

class WebMediaPlayerImplTest : public ::testing::Test {
 public:
  WebMediaPlayerImplTest() : sbplayer_interface_(new DefaultSbPlayerInterface) {
    wmpi_ = std::make_unique<WebMediaPlayerImpl>(
        sbplayer_interface_.get(), nullptr,
        base::Bind(MockGetSbDecodeTargetGraphicsContextProvider), &client_,
        &client_, true, false, true,
#if SB_API_VERSION >= 15
        10, 10,
#endif  // SB_API_VERSION >= 15
        nullptr);
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

  std::unique_ptr<SbPlayerInterface> sbplayer_interface_;
  // The WebMediaPlayerImpl instance under test.
  std::unique_ptr<WebMediaPlayerImpl> wmpi_;
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
  EXPECT_CALL(client_, MaxVideoCapabilities()).Times(1);
  wmpi_->LoadMediaSource();
}


}  // namespace media
}  // namespace cobalt
