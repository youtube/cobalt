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

#include "base/test/scoped_task_environment.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "cobalt/media/player/web_media_player_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
SbDecodeTargetGraphicsContextProvider*
MockGetSbDecodeTargetGraphicsContextProvider() {
  return NULL;
}
}  // namespace

class WebMediaPlayerTest : public ::testing::Test,
                           private cobalt::media::WebMediaPlayerClient,
                           private cobalt::media::WebMediaPlayerDelegate {
 public:
  typedef ::media::ChunkDemuxer ChunkDemuxer;
  typedef ::cobalt::media::WebMediaPlayer WebMediaPlayer;
  typedef ::cobalt::media::WebMediaPlayerImpl WebMediaPlayerImpl;
  typedef ::cobalt::media::SbPlayerInterface SbPlayerInterface;
  typedef ::cobalt::media::DefaultSbPlayerInterface DefaultSbPlayerInterface;

  WebMediaPlayerTest() : sbplayer_interface_(new DefaultSbPlayerInterface) {
    player_ = std::unique_ptr<WebMediaPlayer>(new WebMediaPlayerImpl(
        sbplayer_interface_.get(), nullptr,
        base::Bind(MockGetSbDecodeTargetGraphicsContextProvider), this, this,
        true, false, true,
#if SB_API_VERSION >= 15
        10, 10,
#endif  // SB_API_VERSION >= 15
        nullptr));
  }

 protected:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME};

  std::unique_ptr<SbPlayerInterface> sbplayer_interface_;
  std::unique_ptr<WebMediaPlayer> player_;

 private:
  // WebMediaPlayerClient methods
  void NetworkStateChanged() override{};
  void NetworkError(const std::string& message) override{};
  void ReadyStateChanged() override {}
  void TimeChanged(bool eos_played) override {}
  void DurationChanged() override {}
  void OutputModeChanged() override {}
  void ContentSizeChanged() override {}
  void PlaybackStateChanged() override {}
  float Volume() const override { return 0.; }
  void SourceOpened(ChunkDemuxer* chunk_demuxer) override{};
  std::string SourceURL() const override { return ""; }
  std::string MaxVideoCapabilities() const override { return ""; }
  bool PreferDecodeToTexture() override { return true; }
  void EncryptedMediaInitDataEncountered(
      const char* init_data_type, const unsigned char* init_data,
      unsigned int init_data_length) override{};
  // WebMediaPlayerDelegate methods
  void RegisterPlayer(WebMediaPlayer* player) override {}
  void UnregisterPlayer(WebMediaPlayer* player) override {}
};

TEST_F(WebMediaPlayerTest, Wurks) { EXPECT_TRUE(true); }

TEST_F(WebMediaPlayerTest, OverrideVideoBufferBudget) {
  player_->EnableVideoBufferBudgetOverride();
  // chunkDemuxer = ;
  // chunkDemuxerStream = ;
  // sourceBufferStream = ;
  // EXPECT_TRUE(source_buffer_stream->is_video_buffer_budget_override_enabled_);
}
