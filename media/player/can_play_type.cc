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

#include "media/player/can_play_type.h"

#include <algorithm>

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "media/audio/shell_audio_streamer.h"
#include "media/player/crypto/key_systems.h"
#include "media/player/mime_util.h"
#if defined(OS_STARBOARD)
#include "starboard/media.h"
#endif  // defined(OS_STARBOARD)

namespace media {

#if defined(OS_STARBOARD)

std::string CanPlayType(const std::string& content_type,
                        const std::string& key_system) {
  SbMediaSupportType type =
      SbMediaCanPlayMimeAndKeySystem(content_type.c_str(), key_system.c_str());
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

#else  // defined(OS_STARBOARD)

namespace {

bool ContainsAAC51(const std::vector<std::string>& codecs) {
  const std::string kAAC51Codec = "aac51";

  std::vector<std::string>::const_iterator iter =
      std::find(codecs.begin(), codecs.end(), kAAC51Codec);
  return iter != codecs.end();
}

bool HasAAC51HardwareSupport() {
#if defined(COBALT_WIN)
  return false;
#else   // defined(COBALT_WIN)
  ShellAudioStreamer* streamer = ShellAudioStreamer::Instance();
  DCHECK(streamer);

  return streamer->GetConfig().max_hardware_channels() >= 6;
#endif  // defined(COBALT_WIN)
}

// Parse mime and codecs from content type. It will return "video/mp4" and
// "avc1.42E01E" for "video/mp4; codecs="avc1.42E01E".  Note that this function
// does minimum validation as the media stack will check the mime type and
// codecs strictly.
bool ParseContentType(const std::string& content_type,
                      std::string* mime,
                      std::string* codecs) {
  DCHECK(mime);
  DCHECK(codecs);
  static const char kCodecs[] = "codecs=";

  std::vector<std::string> tokens;
  // SplitString will also trim the results.
  ::base::SplitString(content_type, ';', &tokens);

  // The first one has to be mime type with delimiter '/' like 'video/mp4'.
  if (tokens.empty() || tokens[0].find('/') == tokens[0].npos) {
    return false;
  }

  *mime = tokens[0];
  codecs->clear();

  for (size_t i = 1; i < tokens.size(); ++i) {
    if (strncasecmp(tokens[i].c_str(), kCodecs, strlen(kCodecs))) {
      continue;
    }
    *codecs = tokens[i].substr(strlen(kCodecs));
    TrimString(*codecs, " \"", codecs);
    break;
  }

  return true;
}

}  // namespace

std::string CanPlayType(const std::string& content_type,
                        const std::string& key_system) {
  std::string mime_type;
  std::string codecs;
  const char kNotSupported[] = "";
  const char kMaybe[] = "maybe";
  const char kProbably[] = "probably";

  // Check if the content type is in valid format.
  if (!ParseContentType(content_type, &mime_type, &codecs)) {
    return kNotSupported;
  }

  if (!IsSupportedMediaMimeType(mime_type)) {
    return kNotSupported;
  }

  if (!key_system.empty()) {
    if (!IsSupportedKeySystem(key_system)) {
      return kNotSupported;
    }

    std::vector<std::string> strict_codecs;
    bool strip_suffix = !IsStrictMediaMimeType(mime_type);
    ParseCodecString(codecs, &strict_codecs, strip_suffix);

    if (!IsSupportedKeySystemWithMediaMimeType(mime_type, strict_codecs,
                                               key_system)) {
      return kNotSupported;
    }

    // Even if all above functions don't claim that they don't support of the
    // particular format with the key_system, we still want to double check if
    // the implementation support this format without encryption.
  }

  if (IsStrictMediaMimeType(mime_type)) {
    if (codecs.empty()) {
      return kMaybe;
    }

    std::vector<std::string> strict_codecs;
    ParseCodecString(codecs, &strict_codecs, false);

    if (ContainsAAC51(strict_codecs) && !HasAAC51HardwareSupport()) {
      return kNotSupported;
    }

    if (!IsSupportedStrictMediaMimeType(mime_type, strict_codecs)) {
      return kNotSupported;
    }

    return kProbably;
  }

  std::vector<std::string> parsed_codecs;
  ParseCodecString(codecs, &parsed_codecs, true);

  if (ContainsAAC51(parsed_codecs) && !HasAAC51HardwareSupport()) {
    return kNotSupported;
  }

  if (!AreSupportedMediaCodecs(parsed_codecs)) {
    return kMaybe;
  }

  return kProbably;
}

#endif  // defined(OS_STARBOARD)

}  // namespace media
