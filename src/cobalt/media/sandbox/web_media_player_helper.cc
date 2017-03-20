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

using ::media::BufferedDataSource;
using ::media::VideoFrame;

class WebMediaPlayerHelper::WebMediaPlayerClientStub
    : public ::media::WebMediaPlayerClient {
 public:
  ~WebMediaPlayerClientStub() {}

 private:
  // WebMediaPlayerClient methods
  void NetworkStateChanged() OVERRIDE {}
  void ReadyStateChanged() OVERRIDE {}
  void TimeChanged() OVERRIDE {}
  void DurationChanged() OVERRIDE {}
  void PlaybackStateChanged() OVERRIDE {}
  void SawUnsupportedTracks() OVERRIDE {}
  float Volume() const OVERRIDE { return 1.f; }
#if defined(COBALT_MEDIA_SOURCE_2016)
  void SourceOpened(::media::ChunkDemuxer*) OVERRIDE {}
#else   // defined(COBALT_MEDIA_SOURCE_2016)
  void SourceOpened() OVERRIDE {}
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  std::string SourceURL() const OVERRIDE { return ""; }
};

WebMediaPlayerHelper::WebMediaPlayerHelper(MediaModule* media_module)
    : client_(new WebMediaPlayerClientStub),
      player_(media_module->CreateWebMediaPlayer(client_)) {
  player_->SetRate(1.0);
  player_->LoadMediaSource();
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
      fetcher_factory->network_module()));
  player_->LoadProgressive(video_url, data_source.Pass(),
                           WebMediaPlayer::kCORSModeUnspecified);
  player_->Play();
}

WebMediaPlayerHelper::~WebMediaPlayerHelper() {
  player_.reset();
  delete client_;
}

scoped_refptr<VideoFrame> WebMediaPlayerHelper::GetCurrentFrame() const {
  return player_->GetVideoFrameProvider()->GetCurrentFrame();
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
