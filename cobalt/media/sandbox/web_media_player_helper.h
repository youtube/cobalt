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

#ifndef COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_
#define COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/video_frame_provider.h"
#include "cobalt/media/media_module.h"
#include "cobalt/media/player/web_media_player.h"
#include "url/gurl.h"

namespace cobalt {
namespace media {
namespace sandbox {

// This class creates and manages a WebMediaPlayer internally.  It provides the
// necessary WebMediaPlayerClient implementation and helper functions to
// simplify the using of WebMediaPlayer.
class WebMediaPlayerHelper {
 public:
  typedef base::Callback<void(ChunkDemuxer*)> ChunkDemuxerOpenCB;

  // Ctor to create an adaptive pipeline.  |open_cb| will be called when the
  // ChunkDemuxer is ready to add source buffers.
  WebMediaPlayerHelper(MediaModule* media_module,
                       const ChunkDemuxerOpenCB& chunk_demuxer_open_cb,
                       const math::Size& viewport_size);
  // Ctor to create a progressive pipeline.
  WebMediaPlayerHelper(MediaModule* media_module,
                       loader::FetcherFactory* fetcher_factory,
                       const GURL& video_url, const math::Size& viewport_size);
  ~WebMediaPlayerHelper();

  SbDecodeTarget GetCurrentDecodeTarget() const;
  bool IsPlaybackFinished() const;

  WebMediaPlayer* player() { return player_.get(); }

 private:
  class WebMediaPlayerClientStub;

  WebMediaPlayerClientStub* client_;
  std::unique_ptr<WebMediaPlayer> player_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerHelper);
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_
