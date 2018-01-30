// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/media/sandbox/web_media_player_helper.h"

#include "cobalt/media/fetcher_buffered_data_source.h"

namespace cobalt {
namespace media {
namespace sandbox {

#if !defined(COBALT_MEDIA_SOURCE_2016)
using ::media::BufferedDataSource;
using ::media::VideoFrame;
using ::media::WebMediaPlayerClient;
#endif  // !defined(WebMediaPlayerDelegate)

class WebMediaPlayerHelper::WebMediaPlayerClientStub
    : public WebMediaPlayerClient {
 public:
  WebMediaPlayerClientStub() {}
  explicit WebMediaPlayerClientStub(
      const ChunkDemuxerOpenCB& chunk_demuxer_open_cb)
      : chunk_demuxer_open_cb_(chunk_demuxer_open_cb) {
    DCHECK(!chunk_demuxer_open_cb_.is_null());
  }
  ~WebMediaPlayerClientStub() {}

 private:
  // WebMediaPlayerClient methods
  void NetworkStateChanged() override {}
  void ReadyStateChanged() override {}
  void TimeChanged(bool) override {}
  void DurationChanged() override {}
  void OutputModeChanged() override {}
  void ContentSizeChanged() override {}
  void PlaybackStateChanged() override {}
  void SawUnsupportedTracks() override {}
  float Volume() const override { return 1.f; }
  void SourceOpened(ChunkDemuxer* chunk_demuxer) override {
    DCHECK(!chunk_demuxer_open_cb_.is_null());
    chunk_demuxer_open_cb_.Run(chunk_demuxer);
  }
  std::string SourceURL() const override { return ""; }
  bool PreferDecodeToTexture() { return true; }

#if defined(COBALT_MEDIA_SOURCE_2016)
  void EncryptedMediaInitDataEncountered(EmeInitDataType, const unsigned char*,
                                         unsigned) override {}
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

  ChunkDemuxerOpenCB chunk_demuxer_open_cb_;
};

WebMediaPlayerHelper::WebMediaPlayerHelper(MediaModule* media_module,
                                           const ChunkDemuxerOpenCB& open_cb)
    : client_(new WebMediaPlayerClientStub(open_cb)),
      player_(media_module->CreateWebMediaPlayer(client_)) {
  player_->SetRate(1.0);
// TODO: Investigate a better way to exclude this when SB_HAS(PLAYER_WITH_URL)
//       is enabled.
#if !SB_HAS(PLAYER_WITH_URL)
  player_->LoadMediaSource();
#endif  // !SB_HAS(PLAYER_WITH_URL)
  player_->Play();
}

WebMediaPlayerHelper::WebMediaPlayerHelper(
    MediaModule* media_module, loader::FetcherFactory* fetcher_factory,
    const GURL& video_url)
    : client_(new WebMediaPlayerClientStub),
      player_(media_module->CreateWebMediaPlayer(client_)) {
  player_->SetRate(1.0);
  scoped_ptr<BufferedDataSource> data_source(new FetcherBufferedDataSource(
      base::MessageLoopProxy::current(), video_url, csp::SecurityCallback(),
      fetcher_factory->network_module(), loader::kNoCORSMode,
      loader::Origin()));
// TODO: Investigate a better way to exclude this when SB_HAS(PLAYER_WITH_URL)
//       is enabled.
#if !SB_HAS(PLAYER_WITH_URL)
  player_->LoadProgressive(video_url, data_source.Pass());
#endif  // !SB_HAS(PLAYER_WITH_URL)
  player_->Play();
}

WebMediaPlayerHelper::~WebMediaPlayerHelper() {
  player_.reset();
  delete client_;
}

#if !defined(COBALT_MEDIA_SOURCE_2016)
scoped_refptr<VideoFrame> WebMediaPlayerHelper::GetCurrentFrame() const {
  return player_->GetVideoFrameProvider()->GetCurrentFrame();
}
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

SbDecodeTarget WebMediaPlayerHelper::GetCurrentDecodeTarget() const {
  return player_->GetVideoFrameProvider()->GetCurrentSbDecodeTarget();
}

bool WebMediaPlayerHelper::IsPlaybackFinished() const {
  // Use a small epsilon to ensure that the video can finish properly even when
  // the audio and video streams are shorter than the duration specified in the
  // container.
  return player_->GetCurrentTime() >= player_->GetDuration() - 0.1f;
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
