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

#ifndef COBALT_MEDIA_MEDIA_MODULE_H_
#define COBALT_MEDIA_MEDIA_MODULE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/optional.h"
#include "base/threading/thread.h"
#include "cobalt/base/user_log.h"
#include "cobalt/math/size.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/system_window/system_window.h"

#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/player/web_media_player_delegate.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/filters/shell_video_decoder_impl.h"
#include "media/player/web_media_player_delegate.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)

namespace cobalt {
namespace media {

#if !defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::ShellRawVideoDecoderFactory ShellRawVideoDecoderFactory;
typedef ::media::WebMediaPlayer WebMediaPlayer;
typedef ::media::WebMediaPlayerDelegate WebMediaPlayerDelegate;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

// TODO: Collapse MediaModule into ShellMediaPlatform.
class MediaModule : public WebMediaPlayerFactory,
                    public WebMediaPlayerDelegate {
 public:
  struct Options {
    Options()
        : use_audio_decoder_stub(false),
          use_null_audio_streamer(false),
          use_video_decoder_stub(false),
          disable_webm_vp9(false) {}
    bool use_audio_decoder_stub;
    bool use_null_audio_streamer;
    bool use_video_decoder_stub;
    bool disable_webm_vp9;
    base::optional<math::Size> output_resolution_override;
  };

  typedef render_tree::Image Image;

  // Calculates the output resolution based on the window size and override.
  static math::Size CalculateOutputResolution(
      system_window::SystemWindow* system_window,
      const base::optional<math::Size>& output_resolution_override);
  // MediaModule implementation should implement this function to allow creation
  // of CanPlayTypeHandler.
  static scoped_ptr<CanPlayTypeHandler> CreateCanPlayTypeHandler();

  virtual ~MediaModule() {}

  // Returns true when the setting is set successfully or if the setting has
  // already been set to the expected value.  Returns false when the setting is
  // invalid or not set to the expected value.
  virtual bool SetConfiguration(const std::string& name, int32 value) {
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(value);
    return false;
  }

  // The following functions will be called inside Suspend() and Resume()
  // from the main thread.  Sub-classes can override these functions for
  // platform specific tasks.
  virtual void OnSuspend() {}
  virtual void OnResume(render_tree::ResourceProvider* resource_provider) {
    UNREFERENCED_PARAMETER(resource_provider);
  }

  virtual system_window::SystemWindow* system_window() const { return NULL; }

  void Suspend();
  void Resume(render_tree::ResourceProvider* resource_provider);

#if !defined(COBALT_MEDIA_SOURCE_2016)
#if !defined(COBALT_BUILD_TYPE_GOLD)
  virtual ShellRawVideoDecoderFactory* GetRawVideoDecoderFactory() {
    return NULL;
  }
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

  // TODO: Move the following methods into class like MediaModuleBase
  // to ensure that MediaModule is an interface.
  // WebMediaPlayerDelegate methods
  void RegisterPlayer(WebMediaPlayer* player) override;
  void UnregisterPlayer(WebMediaPlayer* player) override;

  // This function should be defined on individual platform to create the
  // platform specific MediaModule.
  static scoped_ptr<MediaModule> Create(
      system_window::SystemWindow* system_window,
      render_tree::ResourceProvider* resource_provider,
      const Options& options = Options());

 protected:
  MediaModule() : thread_("media_module"), suspended_(false) {
    thread_.Start();
    message_loop_ = thread_.message_loop_proxy();
  }

 private:
  void RegisterDebugState(WebMediaPlayer* player);
  void DeregisterDebugState();
  void SuspendTask();
  void ResumeTask();
  void RegisterPlayerTask(WebMediaPlayer* player);
  void UnregisterPlayerTask(WebMediaPlayer* player);

  // When the value of a particular player is true, it means the player is
  // paused by us.
  typedef std::map<WebMediaPlayer*, bool> Players;

  // The thread that |players_| is accessed from,
  base::Thread thread_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  Players players_;
  bool suspended_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_MODULE_H_
