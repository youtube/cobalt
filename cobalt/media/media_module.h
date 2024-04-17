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

#ifndef COBALT_MEDIA_MEDIA_MODULE_H_
#define COBALT_MEDIA_MEDIA_MODULE_H_

#include <map>
#include <memory>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "cobalt/math/size.h"
#include "cobalt/media/base/sbplayer_interface.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "cobalt/media/player/web_media_player_delegate.h"
#include "cobalt/media/player/web_media_player_impl.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/system_window/system_window.h"
#include "media/base/media_log.h"
#include "starboard/common/mutex.h"
#include "starboard/player.h"

namespace cobalt {
namespace media {

class MediaModule : public WebMediaPlayerFactory,
                    public WebMediaPlayerDelegate {
 public:
  struct Options {
    Options() {}

    bool allow_resume_after_suspend = true;
  };

  typedef render_tree::Image Image;

  // MediaModule implementation should implement this function to allow creation
  // of CanPlayTypeHandler.
  static std::unique_ptr<CanPlayTypeHandler> CreateCanPlayTypeHandler();

  MediaModule(system_window::SystemWindow* system_window,
              render_tree::ResourceProvider* resource_provider,
              const Options& options = Options())
      : sbplayer_interface_(new DefaultSbPlayerInterface),
        options_(options),
        system_window_(system_window),
        resource_provider_(resource_provider) {}

  // Returns true when the setting is set successfully or if the setting has
  // already been set to the expected value.  Returns false when the setting is
  // invalid or not set to the expected value.
  bool SetConfiguration(const std::string& name, int32 value);

  const DecoderBufferAllocator* GetDecoderBufferAllocator() const {
    return &decoder_buffer_allocator_;
  }

  // WebMediaPlayerFactory methods
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      WebMediaPlayerClient* client) override;
  void EnumerateWebMediaPlayers(
      const EnumeratePlayersCB& enumerate_callback) const override;

  void Suspend();
  void Resume(render_tree::ResourceProvider* resource_provider);

  // TODO: Move the following methods into class like MediaModuleBase
  // to ensure that MediaModule is an interface.
  // WebMediaPlayerDelegate methods
  void RegisterPlayer(WebMediaPlayer* player) override;
  void UnregisterPlayer(WebMediaPlayer* player) override;

  void UpdateSystemWindowAndResourceProvider(
      system_window::SystemWindow* system_window,
      render_tree::ResourceProvider* resource_provider) {
    Suspend();
    system_window_ = system_window;
    Resume(resource_provider);
  }

 private:
  void RegisterDebugState(WebMediaPlayer* player);
  void DeregisterDebugState();

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() {
    return resource_provider_->GetSbDecodeTargetGraphicsContextProvider();
  }

  // When the value of a particular player is true, it means the player is
  // paused by us.
  typedef std::map<WebMediaPlayer*, bool> Players;

  std::unique_ptr<SbPlayerInterface> sbplayer_interface_;
  const Options options_;
  system_window::SystemWindow* system_window_;
  cobalt::render_tree::ResourceProvider* resource_provider_;

  ::media::MediaLog media_log_;

  // Protect access to the list of players.
  starboard::Mutex players_lock_;

  Players players_;
  bool suspended_ = false;

  bool allow_batched_sample_write_ = false;
  // When set to `false` (the default value), Cobalt calls
  // `SbPlayerGetPreferredOutputMode()` with `kSbPlayerOutputModeInvalid` when
  // there is no preference on output mode.
  // When set to `true` via `h5vcc.settings`, Cobalt calls
  // `SbPlayerGetPreferredOutputMode()` with `kSbPlayerOutputModePunchOut` when
  // there is no preference on output mode.  This allows us to fallback to the
  // previous behavior.
  bool force_punch_out_by_default_ = false;

#if SB_API_VERSION >= 15
  base::TimeDelta audio_write_duration_local_ =
      base::TimeDelta::FromMicroseconds(kSbPlayerWriteDurationLocal);
  base::TimeDelta audio_write_duration_remote_ =
      base::TimeDelta::FromMicroseconds(kSbPlayerWriteDurationRemote);
#endif  // SB_API_VERSION >= 15

  DecoderBufferAllocator decoder_buffer_allocator_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_MODULE_H_
