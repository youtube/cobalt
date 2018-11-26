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

// A simple localized string table implementation.

#ifndef STARBOARD_SHARED_STARBOARD_LOCALIZED_STRINGS_H_
#define STARBOARD_SHARED_STARBOARD_LOCALIZED_STRINGS_H_

#include <map>
#include <string>

#include "starboard/file.h"

namespace starboard {
namespace shared {
namespace starboard {

// Stores a map of internationalized strings for a particular language.
// Initialized from a language-specific file in a simple, CSV-style format.
class LocalizedStrings {
 public:
  enum MatchType { kNoMatch, kPrimaryMatch, kSecondaryMatch };
  explicit LocalizedStrings(const std::string& language);

  // Gets a localized string.
  std::string GetString(const std::string& id,
                        const std::string& fallback) const;

  MatchType GetMatchType() const;

 private:
  typedef std::map<std::string, std::string> StringMap;

  // Loads the strings for a particular language.
  // Returns true if successful, false otherwise.
  bool LoadStrings(const std::string& language);

  // Loads a single string.
  // Returns true if successful, false otherwise.
  bool LoadSingleString(const std::string& message);

  StringMap strings_;
  MatchType match_type_ = kNoMatch;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_LOCALIZED_STRINGS_H_
