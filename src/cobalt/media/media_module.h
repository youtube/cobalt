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
#include "base/threading/thread.h"
#include "cobalt/base/user_log.h"
#include "cobalt/math/size.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/system_window/system_window.h"
#include "media/base/shell_media_platform.h"
#include "media/base/shell_video_frame_provider.h"
#include "media/filters/shell_video_decoder_impl.h"
#include "media/player/web_media_player_delegate.h"

namespace cobalt {
namespace media {

// TODO: Collapse MediaModule into ShellMediaPlatform.
class MediaModule : public CanPlayTypeHandler,
                    public WebMediaPlayerFactory,
                    public ::media::WebMediaPlayerDelegate {
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
  };

  typedef ::media::WebMediaPlayer WebMediaPlayer;
  typedef render_tree::Image Image;

  MediaModule() : thread_("media_module") {
    thread_.Start();
    message_loop_ = thread_.message_loop_proxy();
  }
  virtual ~MediaModule() {}

  // Returns true when the setting is set successfully or if the setting has
  // already been set to the expected value.  Returns false when the setting is
  // invalid or not set to the expected value.
  virtual bool SetConfiguration(const std::string& name, int32 value) {
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(value);
    return false;
  }

  void Suspend();
  void Resume();

#if !defined(COBALT_BUILD_TYPE_GOLD)
  virtual ::media::ShellRawVideoDecoderFactory* GetRawVideoDecoderFactory() {
    return NULL;
  }
#endif  // !defined(COBALT_RELEASE)

  // TODO: Move the following methods into class like MediaModuleBase
  // to ensure that MediaModule is an interface.
  // WebMediaPlayerDelegate methods
  void RegisterPlayer(WebMediaPlayer* player) OVERRIDE;
  void UnregisterPlayer(WebMediaPlayer* player) OVERRIDE;

  // This function should be defined on individual platform to create the
  // platform specific MediaModule.
  static scoped_ptr<MediaModule> Create(
      system_window::SystemWindow* system_window, const math::Size& output_size,
      render_tree::ResourceProvider* resource_provider,
      const Options& options = Options());

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
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_MEDIA_MODULE_H_
