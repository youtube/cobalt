// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_WEB_MEDIA_PLAYER_FACTORY_H_
#define COBALT_MEDIA_WEB_MEDIA_PLAYER_FACTORY_H_

#include <memory>

#include "base/callback.h"
#include "cobalt/media/player/web_media_player.h"

namespace cobalt {
namespace media {

class WebMediaPlayerFactory {
 public:
  typedef base::RepeatingCallback<void(const WebMediaPlayer*)>
      EnumeratePlayersCB;

  virtual std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      WebMediaPlayerClient* client) = 0;

  virtual void EnumerateWebMediaPlayers(
      const EnumeratePlayersCB& enumerate_callback) const = 0;

 protected:
  WebMediaPlayerFactory() {}
  ~WebMediaPlayerFactory() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerFactory);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_WEB_MEDIA_PLAYER_FACTORY_H_
