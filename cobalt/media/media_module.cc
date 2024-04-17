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

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/media/base/format_support_query_metrics.h"
#include "media/base/mime_util.h"
#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/window.h"

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "cobalt/browser/switches.h"
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

namespace cobalt {
namespace media {

namespace {

// TODO: Determine if ExtractCodecs() and ExtractEncryptionScheme() can be
// combined and simplified.
static std::vector<std::string> ExtractCodecs(const std::string& mime_type) {
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
      ::media::SplitCodecs(name_and_value[1], &codecs);
      return codecs;
    }
  }
  return codecs;
}

static std::string ExtractEncryptionScheme(const std::string& key_system) {
  std::vector<std::string> components = base::SplitString(
      key_system, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  auto iter = components.begin();
  for (; iter != components.end(); ++iter) {
    std::vector<std::string> name_and_value = base::SplitString(
        *iter, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (name_and_value.size() != 1 && name_and_value.size() != 2) {
      LOG(WARNING) << "parameter for key_system \"" << key_system
                   << "\" is not valid.";
      continue;
    }
    if (name_and_value[0] == "encryptionscheme") {
      if (name_and_value.size() < 2) {
        return "";
      }
      base::RemoveChars(name_and_value[1], "\"", &name_and_value[1]);
      return name_and_value[1];
    }
  }
  return "";
}

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

  void SetDisabledMediaEncryptionSchemes(
      const std::string& disabled_encryption_schemes) override {
    disabled_encryption_schemes_ =
        base::SplitString(disabled_encryption_schemes, ";",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    LOG(INFO) << "Disabled encryption scheme(s) \""
              << disabled_encryption_schemes << "\" from command line.";
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
    SbMediaSupportType support_type;
    media::FormatSupportQueryMetrics metrics;
    if (strstr(mime_type.c_str(), "video/mp4") == 0 &&
        strstr(mime_type.c_str(), "application/x-mpegURL") == 0) {
      support_type = kSbMediaSupportTypeNotSupported;
    } else {
      support_type = CanPlayType(mime_type, "");
    }
    metrics.RecordAndLogQuery("HTMLMediaElement::canPlayType", mime_type, "",
                              support_type);
    return support_type;
  }

  SbMediaSupportType CanPlayAdaptive(
      const std::string& mime_type,
      const std::string& key_system) const override {
    media::FormatSupportQueryMetrics metrics;
    SbMediaSupportType support_type = CanPlayType(mime_type, key_system);
    metrics.RecordAndLogQuery("MediaSource::IsTypeSupported", mime_type,
                              key_system, support_type);
    return support_type;
  }

 private:
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

    if (!disabled_encryption_schemes_.empty()) {
      std::string encryption_scheme = ExtractEncryptionScheme(key_system);
      if (std::find(disabled_encryption_schemes_.begin(),
                    disabled_encryption_schemes_.end(),
                    encryption_scheme) != disabled_encryption_schemes_.end()) {
        LOG(INFO) << "Encryption scheme (" << encryption_scheme
                  << ") is disabled via console/command line.";
        return kSbMediaSupportTypeNotSupported;
      }
    }
    SbMediaSupportType type =
        SbMediaCanPlayMimeAndKeySystem(mime_type.c_str(), key_system.c_str());
    return type;
  }

  // List of disabled media codecs that will be treated as unsupported.
  std::vector<std::string> disabled_media_codecs_;
  // List of disabled DRM encryption schemes that will be treated as
  // unsupported.
  std::vector<std::string> disabled_encryption_schemes_;
};

}  // namespace

bool MediaModule::SetConfiguration(const std::string& name, int32 value) {
  if (name == "EnableBatchedSampleWrite") {
    allow_batched_sample_write_ = value;
    LOG(INFO) << (allow_batched_sample_write_ ? "Enabling" : "Disabling")
              << " batched sample write.";
    return true;
  } else if (name == "ForcePunchOutByDefault") {
    force_punch_out_by_default_ = value;
    LOG(INFO) << "Force punch out by default : "
              << (force_punch_out_by_default_ ? "enabled" : "disabled");
    return true;
  } else if (name == "EnableMetrics") {
    sbplayer_interface_->EnableCValStats(value);
    LOG(INFO) << (value ? "Enabling" : "Disabling")
              << " media metrics collection.";
    return true;
#if SB_API_VERSION >= 15
  } else if (name == "AudioWriteDurationLocal" && value > 0) {
    audio_write_duration_local_ = base::TimeDelta::FromMicroseconds(value);
    LOG(INFO) << "Set AudioWriteDurationLocal to "
              << audio_write_duration_local_.InMicroseconds();
    return true;
  } else if (name == "AudioWriteDurationRemote" && value > 0) {
    audio_write_duration_remote_ = base::TimeDelta::FromMicroseconds(value);
    LOG(INFO) << "Set AudioWriteDurationRemote to "
              << audio_write_duration_remote_.InMicroseconds();
    return true;
#endif  // SB_API_VERSION >= 15
  }

  return false;
}

std::unique_ptr<WebMediaPlayer> MediaModule::CreateWebMediaPlayer(
    WebMediaPlayerClient* client) {
  SbWindow window = kSbWindowInvalid;
  if (system_window_) {
    window = system_window_->GetSbWindow();
  }

  return std::unique_ptr<WebMediaPlayer>(new media::WebMediaPlayerImpl(
      sbplayer_interface_.get(), window,
      base::Bind(&MediaModule::GetSbDecodeTargetGraphicsContextProvider,
                 base::Unretained(this)),
      client, this, options_.allow_resume_after_suspend,
      allow_batched_sample_write_, force_punch_out_by_default_,
#if SB_API_VERSION >= 15
      audio_write_duration_local_, audio_write_duration_remote_,
#endif  // SB_API_VERSION >= 15
      &media_log_));
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

  decoder_buffer_allocator_.Suspend();

  resource_provider_ = NULL;
}

void MediaModule::Resume(render_tree::ResourceProvider* resource_provider) {
  starboard::ScopedLock scoped_lock(players_lock_);

  resource_provider_ = resource_provider;

  SbWindow window = kSbWindowInvalid;
  if (system_window_) {
    window = system_window_->GetSbWindow();
  }

  decoder_buffer_allocator_.Resume();

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
