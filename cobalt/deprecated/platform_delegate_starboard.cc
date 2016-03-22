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
#include "cobalt/deprecated/platform_delegate.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "starboard/system.h"
#include "starboard/types.h"

namespace cobalt {
namespace deprecated {

namespace {

class PlatformDelegateStarboard : public PlatformDelegate {
 public:
  PlatformDelegateStarboard();
  ~PlatformDelegateStarboard() OVERRIDE;

  std::string GetSystemLanguage() OVERRIDE;
  bool AreKeysReversed() const OVERRIDE;
  std::string GetPlatformName() const OVERRIDE;
};

PlatformDelegateStarboard::PlatformDelegateStarboard() {
  scoped_array<char> path(new char[SB_FILE_MAX_PATH]);
  path[0] = '\0';

  if (!SbSystemGetPath(kSbSystemPathSourceDirectory, path.get(),
                       SB_FILE_MAX_PATH)) {
    DLOG(FATAL) << "Failed to get path for dir_source_root_";
  } else {
    dir_source_root_ = path.get();
  }

  if (!SbSystemGetPath(kSbSystemPathContentDirectory, path.get(),
                       SB_FILE_MAX_PATH)) {
    DLOG(FATAL) << "Failed to get path for game_content_path_";
  } else {
    game_content_path_ = path.get();
  }

  if (!SbSystemGetPath(kSbSystemPathDebugOutputDirectory, path.get(),
                       SB_FILE_MAX_PATH)) {
    DLOG(FATAL) << "Failed to get path for screenshot_output_path_";
  } else {
    screenshot_output_path_ = path.get();
    logging_output_path_ = path.get();
  }

  if (!SbSystemGetPath(kSbSystemPathTempDirectory, path.get(),
                       SB_FILE_MAX_PATH)) {
    DLOG(FATAL) << "Failed to get path for temp_path_";
  } else {
    temp_path_ = path.get();
  }
}

PlatformDelegateStarboard::~PlatformDelegateStarboard() {}

std::string PlatformDelegateStarboard::GetSystemLanguage() {
  // Get Locale ID (e.g. "en_US.UTF8").
  std::string locale_id = SbSystemGetLocaleId();

  // Split Locale ID up into components, if there are any.
  std::vector<std::string> components;
  size_t count = Tokenize(locale_id, ".", &components);
  if (count == 0 || components[0].length() == 0) {
    DLOG(FATAL) << "Unable to get valid locale_id: " << locale_id;
    return "en-US";
  }

  // Take only the first component, and replace '_' with '-'.
  std::string language;
  ReplaceChars(components[0], "_", "-", &language);

  // We should end up with something like "en-US".
  return language;
}

bool PlatformDelegateStarboard::AreKeysReversed() const {
  return SbSystemHasCapability(kSbSystemCapabilityReversedEnterAndBack);
}

std::string PlatformDelegateStarboard::GetPlatformName() const {
  char property[512];

  std::string result;
  if (!SbSystemGetProperty(kSbSystemPropertyPlatformName, property,
                           SB_ARRAY_SIZE_INT(property))) {
    DLOG(FATAL) << "Failed to get kSbSystemPropertyPlatformName.";
  } else {
    result = property;
  }

  return result;
}

}  // namespace

PlatformDelegate* PlatformDelegate::Create() {
  return new PlatformDelegateStarboard();
}

}  // namespace deprecated
}  // namespace cobalt
