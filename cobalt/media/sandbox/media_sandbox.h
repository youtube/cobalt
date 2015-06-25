/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_SANDBOX_MEDIA_SANDBOX_H_
#define MEDIA_SANDBOX_MEDIA_SANDBOX_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "cobalt/media/media_module.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "googleurl/src/gurl.h"
#include "media/player/web_media_player.h"

namespace cobalt {
namespace media {
namespace sandbox {

class MediaSandbox : public ::media::WebMediaPlayerClient {
 public:
  typedef render_tree::Image Image;

  explicit MediaSandbox(
      cobalt::render_tree::ResourceProvider* resource_provider);

  void LoadAndPlay(const GURL& url, loader::FetcherFactory* fetcher_factory);
  scoped_refptr<Image> GetCurrentFrame();

 private:
  typedef ::media::WebMediaPlayer WebMediaPlayer;

  // WebMediaPlayerClient methods
  void NetworkStateChanged() OVERRIDE {}
  void ReadyStateChanged() OVERRIDE {}
  void VolumeChanged(float volume) OVERRIDE {}
  void MuteChanged(bool mute) OVERRIDE {}
  void TimeChanged() OVERRIDE {}
  void DurationChanged() OVERRIDE {}
  void RateChanged() OVERRIDE {}
  void SizeChanged() OVERRIDE {}
  void PlaybackStateChanged() OVERRIDE {}
  void Repaint() OVERRIDE {}
  void SawUnsupportedTracks() OVERRIDE {}
  float Volume() const OVERRIDE { return 1.f; }
  void SourceOpened() OVERRIDE {}
  std::string SourceURL() const OVERRIDE { return ""; }

  scoped_ptr<MediaModule> media_module_;
  scoped_ptr<WebMediaPlayer> player_;
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // MEDIA_SANDBOX_MEDIA_SANDBOX_H_
