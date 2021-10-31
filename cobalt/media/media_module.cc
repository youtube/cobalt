// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/media/base/media_log.h"
#include "cobalt/media/base/mime_util.h"
#include "nb/memory_scope.h"
#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/window.h"

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "cobalt/browser/switches.h"
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

namespace cobalt {
namespace media {

namespace {

class CanPlayTypeHandlerStarboard : public CanPlayTypeHandler {
 public:
  void SetDisabledMediaCodecs(
      const std::string& disabled_media_codecs) override {
    disabled_media_codecs_ =
        base::SplitString(disabled_media_codecs, ";", base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    LOG(INFO) << "Disabled media codecs \"" << disabled_media_codecs
              << "\" from console/command line.";
  }

  SbMediaSupportType CanPlayProgressive(
      const std::string& mime_type) const override {
    // |mime_type| is something like:
    //   video/mp4
    //   video/webm
    //   video/mp4; codecs="avc1.4d401e"
    //   video/webm; codecs="vp9"
    // We do a rough pre-filter to ensure that only video/mp4 is supported as
    // progressive.
    if (strstr(mime_type.c_str(), "video/mp4") == 0 &&
        strstr(mime_type.c_str(), "application/x-mpegURL") == 0) {
      return kSbMediaSupportTypeNotSupported;
    }

    return CanPlayType(mime_type, "");
  }

  SbMediaSupportType CanPlayAdaptive(
      const std::string& mime_type,
      const std::string& key_system) const override {
    return CanPlayType(mime_type, key_system);
  }

 private:
  std::vector<std::string> ExtractCodecs(const std::string& mime_type) const {
    std::vector<std::string> codecs;
    std::vector<std::string> components = base::SplitString(
        mime_type, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    LOG_IF(WARNING, components.empty())
        << "argument mime type \"" << mime_type << "\" is not valid.";
    // The first component is the type/subtype pair. We want to iterate over the
    // remaining components to search for the codecs.
    auto iter = components.begin() + 1;
    for (; iter != components.end(); ++iter) {
      std::vector<std::string> name_and_value = base::SplitString(
          *iter, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      if (name_and_value.size() != 2) {
        LOG(WARNING) << "parameter for mime_type \"" << mime_type
                     << "\" is not valid.";
        continue;
      }
      if (name_and_value[0] == "codecs") {
        ParseCodecString(name_and_value[1], &codecs, /* strip= */ false);
        return codecs;
      }
    }
    return codecs;
  }

  SbMediaSupportType CanPlayType(const std::string& mime_type,
                                 const std::string& key_system) const {
    if (!disabled_media_codecs_.empty()) {
      auto mime_codecs = ExtractCodecs(mime_type);
      for (auto& disabled_codec : disabled_media_codecs_) {
        for (auto& mime_codec : mime_codecs) {
          if (mime_codec.find(disabled_codec) != std::string::npos) {
            LOG(INFO) << "Codec (" << mime_codec
                      << ") is disabled via console/command line.";
            return kSbMediaSupportTypeNotSupported;
          }
        }
      }
    }
    SbMediaSupportType type =
        SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
    return type;
  }

  // List of disabled media codecs that will be treated as unsupported.
  std::vector<std::string> disabled_media_codecs_;
};

}  // namespace

std::unique_ptr<WebMediaPlayer> MediaModule::CreateWebMediaPlayer(
    WebMediaPlayerClient* client) {
  TRACK_MEMORY_SCOPE("Media");
  SbWindow window = kSbWindowInvalid;
  if (system_window_) {
    window = system_window_->GetSbWindow();
  }

  return std::unique_ptr<WebMediaPlayer>(new media::WebMediaPlayerImpl(
      window,
      base::Bind(&MediaModule::GetSbDecodeTargetGraphicsContextProvider,
                 base::Unretained(this)),
      client, this, &decoder_buffer_allocator_,
      options_.allow_resume_after_suspend, new media::MediaLog));
}

void MediaModule::Suspend() {
  starboard::ScopedLock scoped_lock(players_lock_);

  suspended_ = true;

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Suspend();
    }
  }

  resource_provider_ = NULL;
}

void MediaModule::Resume(render_tree::ResourceProvider* resource_provider) {
  starboard::ScopedLock scoped_lock(players_lock_);

  resource_provider_ = resource_provider;

  SbWindow window = kSbWindowInvalid;
  if (system_window_) {
    window = system_window_->GetSbWindow();
  }

  for (Players::iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    DCHECK(!iter->second);
    if (!iter->second) {
      iter->first->Resume(window);
    }
  }

  suspended_ = false;
}

void MediaModule::RegisterPlayer(WebMediaPlayer* player) {
  starboard::ScopedLock scoped_lock(players_lock_);

  DCHECK(players_.find(player) == players_.end());
  players_.insert(std::make_pair(player, false));

  if (suspended_) {
    player->Suspend();
  }
}

void MediaModule::UnregisterPlayer(WebMediaPlayer* player) {
  starboard::ScopedLock scoped_lock(players_lock_);

  DCHECK(players_.find(player) != players_.end());
  players_.erase(players_.find(player));
}

void MediaModule::EnumerateWebMediaPlayers(
    const EnumeratePlayersCB& enumerate_callback) const {
  starboard::ScopedLock scoped_lock(players_lock_);

  for (Players::const_iterator iter = players_.begin(); iter != players_.end();
       ++iter) {
    enumerate_callback.Run(iter->first);
  }
}

std::unique_ptr<CanPlayTypeHandler> MediaModule::CreateCanPlayTypeHandler() {
  return std::unique_ptr<CanPlayTypeHandler>(new CanPlayTypeHandlerStarboard);
}

}  // namespace media
}  // namespace cobalt
