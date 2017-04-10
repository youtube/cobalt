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

#include "cobalt/media/media_module.h"

#include "base/compiler_specific.h"
#include "cobalt/math/size.h"
#include "cobalt/media/shell_media_platform_starboard.h"
#include "cobalt/system_window/system_window.h"
#if defined(COBALT_MEDIA_SOURCE_2016)
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/decoder_buffer_allocator.h"
#include "cobalt/media/player/web_media_player_impl.h"
#else  // defined(COBALT_MEDIA_SOURCE_2016)
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "media/player/web_media_player_impl.h"
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
#include "starboard/media.h"
#include "starboard/window.h"

namespace cobalt {
namespace media {

namespace {

#if !defined(COBALT_MEDIA_SOURCE_2016)
typedef ::media::FilterCollection FilterCollection;
typedef ::media::MessageLoopFactory MessageLoopFactory;
typedef ::media::WebMediaPlayerClient WebMediaPlayerClient;
typedef ::media::ShellMediaPlatformStarboard ShellMediaPlatformStarboard;
#endif  // !defined(COBALT_MEDIA_SOURCE_2016)

class MediaModuleStarboard : public MediaModule {
 public:
  MediaModuleStarboard(system_window::SystemWindow* system_window,
                       const math::Size& output_size,
                       render_tree::ResourceProvider* resource_provider,
                       const Options& options)
      : MediaModule(output_size),
        options_(options),
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
      WebMediaPlayerClient* client) OVERRIDE {
#if defined(COBALT_MEDIA_SOURCE_2016)
    SbWindow window = kSbWindowInvalid;
    if (system_window_) {
      window = system_window_->GetSbWindow();
    }
    return make_scoped_ptr<WebMediaPlayer>(new media::WebMediaPlayerImpl(
        window, client, this, &decoder_buffer_allocator_,
        media_platform_.GetVideoFrameProvider(), new media::MediaLog));
#else   // defined(COBALT_MEDIA_SOURCE_2016)
    scoped_ptr<MessageLoopFactory> message_loop_factory(new MessageLoopFactory);
    scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
        message_loop_factory->GetMessageLoop(MessageLoopFactory::kPipeline);

    SbWindow window = kSbWindowInvalid;
    if (system_window_) {
      window = system_window_->GetSbWindow();
    }
    return make_scoped_ptr<WebMediaPlayer>(new ::media::WebMediaPlayerImpl(
        window, client, this, media_platform_.GetVideoFrameProvider(),
        scoped_ptr<FilterCollection>(new FilterCollection), NULL,
        message_loop_factory.Pass(), new ::media::MediaLog));
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  }

  system_window::SystemWindow* system_window() const OVERRIDE {
    return system_window_;
  }

  void OnSuspend() OVERRIDE { media_platform_.Suspend(); }

  void OnResume(render_tree::ResourceProvider* resource_provider) OVERRIDE {
    media_platform_.Resume(resource_provider);
  }

 private:
  const Options options_;
  system_window::SystemWindow* system_window_;
#if defined(COBALT_MEDIA_SOURCE_2016)
  DecoderBufferAllocator decoder_buffer_allocator_;
#endif  // defined(COBALT_MEDIA_SOURCE_2016)
  ShellMediaPlatformStarboard media_platform_;
};

}  // namespace

scoped_ptr<MediaModule> MediaModule::Create(
    system_window::SystemWindow* system_window, const math::Size& output_size,
    render_tree::ResourceProvider* resource_provider, const Options& options) {
  return make_scoped_ptr<MediaModule>(new MediaModuleStarboard(
      system_window, output_size, resource_provider, options));
}

}  // namespace media
}  // namespace cobalt
