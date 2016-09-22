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

#include "cobalt/media/media_module.h"

#include "base/compiler_specific.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/size.h"
#include "cobalt/media/shell_media_platform_starboard.h"
#include "cobalt/system_window/starboard/system_window.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "media/player/web_media_player_impl.h"
#include "starboard/media.h"
#include "starboard/window.h"

namespace cobalt {
namespace media {

namespace {

using ::base::polymorphic_downcast;
using ::media::FilterCollection;
using ::media::MessageLoopFactory;
using system_window::SystemWindowStarboard;

class MediaModuleStarboard : public MediaModule {
 public:
  MediaModuleStarboard(system_window::SystemWindow* system_window,
                       render_tree::ResourceProvider* resource_provider,
                       const Options& options)
      : options_(options),
        system_window_(system_window),
        media_platform_(resource_provider) {}

  std::string CanPlayType(const std::string& mime_type,
                          const std::string& key_system) OVERRIDE {
    SbMediaSupportType type =
        SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
    switch (type) {
      case kSbMediaSupportTypeNotSupported:
        return "";
      case kSbMediaSupportTypeMaybe:
        return "maybe";
      case kSbMediaSupportTypeProbably:
        return "probably";
    }
    NOTREACHED();
    return "";
  }
  scoped_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      ::media::WebMediaPlayerClient* client) OVERRIDE {
    scoped_ptr<MessageLoopFactory> message_loop_factory(new MessageLoopFactory);
    scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
        message_loop_factory->GetMessageLoop(MessageLoopFactory::kPipeline);
    scoped_ptr<FilterCollection> filter_collection(new FilterCollection);

    SbWindow window = kSbWindowInvalid;
    if (system_window_) {
      window = polymorphic_downcast<SystemWindowStarboard*>(system_window_)
                   ->GetSbWindow();
    }
    return make_scoped_ptr<WebMediaPlayer>(new ::media::WebMediaPlayerImpl(
        window, client, this, media_platform_.GetVideoFrameProvider(),
        filter_collection.Pass(), NULL, message_loop_factory.Pass(),
        new ::media::MediaLog));
  }

 private:
  const Options options_;
  system_window::SystemWindow* system_window_;
  ::media::ShellMediaPlatformStarboard media_platform_;
};

}  // namespace

scoped_ptr<MediaModule> MediaModule::Create(
    system_window::SystemWindow* system_window, const math::Size& output_size,
    render_tree::ResourceProvider* resource_provider, const Options& options) {
  UNREFERENCED_PARAMETER(output_size);
  return make_scoped_ptr<MediaModule>(
      new MediaModuleStarboard(system_window, resource_provider, options));
}

}  // namespace media
}  // namespace cobalt
