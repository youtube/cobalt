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

#include "base/bind.h"
#include "cobalt/media/shell_media_platform_starboard.h"
#include "media/audio/null_audio_streamer.h"
#include "media/audio/shell_audio_sink.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "media/filters/shell_audio_decoder_impl.h"
#include "media/filters/shell_raw_audio_decoder_stub.h"
#include "media/filters/shell_raw_video_decoder_stub.h"
#include "media/filters/shell_video_decoder_impl.h"
#include "media/player/web_media_player_impl.h"

namespace cobalt {
namespace media {

namespace {

using ::media::FilterCollection;
using ::media::MessageLoopFactory;

class MediaModuleStarboard : public MediaModule {
 public:
  MediaModuleStarboard(render_tree::ResourceProvider* resource_provider,
                       const Options& options)
      : options_(options), media_platform_(resource_provider) {}

  scoped_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      ::media::WebMediaPlayerClient* client) OVERRIDE {
    scoped_ptr<MessageLoopFactory> message_loop_factory(new MessageLoopFactory);
    scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
        message_loop_factory->GetMessageLoop(MessageLoopFactory::kPipeline);
    scoped_ptr<FilterCollection> filter_collection(new FilterCollection);

    ::media::ShellAudioStreamer* streamer = NULL;
    if (options_.use_null_audio_streamer) {
      DLOG(INFO) << "Use Null audio";
      streamer = ::media::NullAudioStreamer::GetInstance();
    } else {
      DLOG(INFO) << "Use Pulse audio";
      streamer = ::media::ShellAudioStreamer::Instance();
    }
    return make_scoped_ptr<WebMediaPlayer>(new ::media::WebMediaPlayerImpl(
        client, this, media_platform_.GetVideoFrameProvider(),
        filter_collection.Pass(), new ::media::ShellAudioSink(streamer),
        message_loop_factory.Pass(), new ::media::MediaLog));
  }

 private:
  const Options options_;
  ::media::ShellMediaPlatformStarboard media_platform_;
};

}  // namespace

scoped_ptr<MediaModule> MediaModule::Create(
    render_tree::ResourceProvider* resource_provider, const Options& options) {
  return make_scoped_ptr<MediaModule>(
      new MediaModuleStarboard(resource_provider, options));
}

}  // namespace media
}  // namespace cobalt
