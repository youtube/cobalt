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

#include "cobalt/media/sandbox/media_sandbox.h"


namespace cobalt {
namespace media {
namespace sandbox {

using cobalt::render_tree::ResourceProvider;
using ::media::VideoFrame;
using ::media::WebMediaPlayerDelegate;

MediaSandbox::MediaSandbox(ResourceProvider* resource_provider)
    : media_module_(MediaModule::Create(resource_provider)),
      player_(media_module_->CreateWebMediaPlayer(this)) {}

void MediaSandbox::LoadAndPlay(const GURL& url) {
  player_->SetRate(1.0);
  player_->Load(url, ::media::WebMediaPlayer::kCORSModeUnspecified);
  player_->Play();
}

scoped_refptr<render_tree::Image> MediaSandbox::GetCurrentFrame() {
  scoped_refptr<VideoFrame> frame =
      player_->GetVideoFrameProvider()->GetCurrentFrame();
  return frame ? reinterpret_cast<Image*>(frame->texture_id()) : NULL;
}

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt
