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

#ifndef COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_
#define COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/media_module.h"
#include "googleurl/src/gurl.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/shell_video_frame_provider.h"
#include "cobalt/media/player/web_media_player.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/video_frame.h"
#include "media/player/web_media_player.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace media {
namespace sandbox {

#if !defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ChunkDemuxer ChunkDemuxer;
typedef ::media::VideoFrame VideoFrame;
typedef ::media::WebMediaPlayer WebMediaPlayer;
#endif  // !defined(WebMediaPlayerDelegate)

// This class creates and manages a WebMediaPlayer internally.  It provides the
// necessary WebMediaPlayerClient implementation and helper functions to
// simplify the using of WebMediaPlayer.
class WebMediaPlayerHelper {
 public:
  typedef base::Callback<void(ChunkDemuxer*)> ChunkDemuxerOpenCB;

  // Ctor to create an adaptive pipeline.  |open_cb| will be called when the
  // ChunkDemuxer is ready to add source buffers.
  WebMediaPlayerHelper(MediaModule* media_module,
                       const ChunkDemuxerOpenCB& chunk_demuxer_open_cb);
  // Ctor to create a progressive pipeline.
  WebMediaPlayerHelper(MediaModule* media_module,
                       loader::FetcherFactory* fetcher_factory,
                       const GURL& video_url);
  ~WebMediaPlayerHelper();

  scoped_refptr<VideoFrame> GetCurrentFrame() const;
  SbDecodeTarget GetCurrentDecodeTarget() const;
  bool IsPlaybackFinished() const;

  WebMediaPlayer* player() { return player_.get(); }

 private:
  class WebMediaPlayerClientStub;

  WebMediaPlayerClientStub* client_;
  scoped_ptr<WebMediaPlayer> player_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerHelper);
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_WEB_MEDIA_PLAYER_HELPER_H_
