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
#include "base/logging.h"
#include "media/audio/shell_audio_sink.h"
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "media/player/web_media_player_impl.h"

namespace cobalt {
namespace media {

using ::media::FilterCollection;
using ::media::MessageLoopFactory;
using ::media::WebMediaPlayer;

scoped_ptr<WebMediaPlayer> MediaModule::CreateWebMediaPlayer(
    ::media::WebMediaPlayerClient* client) {
  scoped_ptr<MessageLoopFactory> message_loop_factory(new MessageLoopFactory);
  scoped_refptr<base::MessageLoopProxy> pipeline_message_loop =
      message_loop_factory->GetMessageLoop(MessageLoopFactory::kPipeline);
  scoped_ptr<FilterCollection> filter_collection(new FilterCollection);
  AddFilters(filter_collection.get(), pipeline_message_loop);
  DCHECK_EQ(filter_collection->GetAudioDecoders()->size(), 1);
  DCHECK_EQ(filter_collection->GetVideoDecoders()->size(), 1);
  return make_scoped_ptr<WebMediaPlayer>(new ::media::WebMediaPlayerImpl(
      client, filter_collection.Pass(),
      new ::media::ShellAudioSink(::media::ShellAudioStreamer::Instance()),
      message_loop_factory.Pass(), new ::media::MediaLog));
}

void MediaModule::AddFilters(
    FilterCollection* filter_collection,
    const scoped_refptr<MessageLoopProxy>& pipeline_message_loop) {
  UNREFERENCED_PARAMETER(filter_collection);
  UNREFERENCED_PARAMETER(pipeline_message_loop);
  NOTREACHED();
}

}  // namespace media
}  // namespace cobalt
