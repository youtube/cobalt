// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BASE_LOCALIZED_STRINGS_H_
#define COBALT_BASE_LOCALIZED_STRINGS_H_

#include <map>
#include <string>

#include "base/memory/singleton.h"

namespace base {

// A base::Singleton that stores the localized string table used by the app.
class LocalizedStrings {
 public:
  static LocalizedStrings* GetInstance();

  // Initializes the strings.
  void Initialize(const std::string& language);

  // Gets a localized string.
  std::string GetString(const std::string& id, const std::string& fallback);

 private:
  typedef std::map<std::string, std::string> StringMap;

  friend struct base::DefaultSingletonTraits<LocalizedStrings>;

  LocalizedStrings();
  ~LocalizedStrings();

  // Loads the strings for a particular language.
  // Returns true if successful, false otherwise.
  bool LoadStrings(const std::string& language);

  // Loads a single string.
  // Returns true if successful, false otherwise.
  bool LoadSingleString(const std::string& message);

  StringMap strings_;

  DISALLOW_COPY_AND_ASSIGN(LocalizedStrings);
};

}  // namespace base

#endif  // COBALT_BASE_LOCALIZED_STRINGS_H_
