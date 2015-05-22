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

#ifndef MEDIA_MEDIA_MODULE_H_
#define MEDIA_MEDIA_MODULE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/media/web_media_player_factory.h"
#include "media/base/shell_media_platform.h"
#include "media/base/shell_video_frame_provider.h"

namespace cobalt {
namespace media {

// TODO(***REMOVED***): Collapse MediaModule into ShellMediaPlatform.
class MediaModule : public WebMediaPlayerFactory {
 public:
  typedef render_tree::Image Image;

  virtual ~MediaModule() {}

  // Provide a default implementation to create a dummy WebMediaPlayer instance.
  // Inherited class can override it and create real WebMediaPlayer accordingly.
  scoped_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      ::media::WebMediaPlayerClient* client) OVERRIDE;

  // This function should be defined on individual platform to create the
  // platform specific MediaModule.
  static scoped_ptr<MediaModule> Create(
      render_tree::ResourceProvider* resource_provider);

 protected:
  MediaModule() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaModule);
};

}  // namespace media
}  // namespace cobalt

#endif  // MEDIA_MEDIA_MODULE_H_
