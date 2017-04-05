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

#include "cobalt/media/media_module_stub.h"

#include <string>

#include "base/compiler_specific.h"

namespace cobalt {
namespace media {

#if !defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::BufferedDataSource BufferedDataSource;
typedef ::media::WebMediaPlayer WebMediaPlayer;
typedef ::media::WebMediaPlayerClient WebMediaPlayerClient;
using ::media::Ranges;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

namespace {

class DummyWebMediaPlayer : public WebMediaPlayer {
 private:
  void LoadMediaSource() OVERRIDE {}
  void LoadProgressive(const GURL& url,
                       scoped_ptr<BufferedDataSource> data_source,
                       WebMediaPlayer::CORSMode cors_mode) OVERRIDE {
    UNREFERENCED_PARAMETER(url);
    UNREFERENCED_PARAMETER(data_source);
    UNREFERENCED_PARAMETER(cors_mode);
  }
  void CancelLoad() OVERRIDE {}

  // Playback controls.
  void Play() OVERRIDE {}
  void Pause() OVERRIDE {}
  bool SupportsFullscreen() const OVERRIDE { return false; }
  bool SupportsSave() const OVERRIDE { return false; }
  void Seek(float seconds) OVERRIDE { UNREFERENCED_PARAMETER(seconds); }
  void SetEndTime(float seconds) OVERRIDE { UNREFERENCED_PARAMETER(seconds); }
  void SetRate(float rate) OVERRIDE { UNREFERENCED_PARAMETER(rate); }
  void SetVolume(float volume) OVERRIDE { UNREFERENCED_PARAMETER(volume); }
  void SetVisible(bool visible) OVERRIDE { UNREFERENCED_PARAMETER(visible); }
  const Ranges<base::TimeDelta>& GetBufferedTimeRanges() OVERRIDE {
    return buffer_;
  }
  float GetMaxTimeSeekable() const OVERRIDE { return 0.f; }

  // Suspend/Resume
  void Suspend() OVERRIDE {}
  void Resume() OVERRIDE {}

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const OVERRIDE { return false; }
  bool HasAudio() const OVERRIDE { return false; }

  // Dimension of the video.
  gfx::Size GetNaturalSize() const OVERRIDE { return gfx::Size(); }

  // Getters of playback state.
  bool IsPaused() const OVERRIDE { return false; }
  bool IsSeeking() const OVERRIDE { return false; }
  float GetDuration() const OVERRIDE { return 0.f; }
  float GetCurrentTime() const OVERRIDE { return 0.f; }

  // Get rate of loading the resource.
  int GetDataRate() const OVERRIDE { return 1; }

  // Internal states of loading and network.
  NetworkState GetNetworkState() const OVERRIDE { return kNetworkStateEmpty; }
  ReadyState GetReadyState() const OVERRIDE { return kReadyStateHaveNothing; }

  bool DidLoadingProgress() const OVERRIDE { return false; }
#if !defined(COBALT_MEDIA_SOURCE_2016)
  unsigned long long GetTotalBytes() const OVERRIDE { return 0; }
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

  bool HasSingleSecurityOrigin() const OVERRIDE { return false; }
  bool DidPassCORSAccessCheck() const OVERRIDE { return false; }

  float MediaTimeForTimeValue(float time_value) const OVERRIDE {
    UNREFERENCED_PARAMETER(time_value);
    return 0.f;
  }

  unsigned GetDecodedFrameCount() const OVERRIDE { return 0; }
  unsigned GetDroppedFrameCount() const OVERRIDE { return 0; }
  unsigned GetAudioDecodedByteCount() const OVERRIDE { return 0; }
  unsigned GetVideoDecodedByteCount() const OVERRIDE { return 0; }

  Ranges<base::TimeDelta> buffer_;
};

}  // namespace

std::string MediaModuleStub::CanPlayType(const std::string& mime_type,
                                         const std::string& key_system) {
  UNREFERENCED_PARAMETER(mime_type);
  UNREFERENCED_PARAMETER(key_system);
  return "";  // Cannot play.
}

scoped_ptr<WebMediaPlayer> MediaModuleStub::CreateWebMediaPlayer(
    WebMediaPlayerClient* client) {
  UNREFERENCED_PARAMETER(client);
  return make_scoped_ptr<WebMediaPlayer>(new DummyWebMediaPlayer);
}

}  // namespace media
}  // namespace cobalt
