// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "cobalt/common/libc/no_destructor.h"
#include "starboard/system.h"
#include "starboard/time_zone.h"

namespace cobalt {
namespace common {
namespace libc {
namespace stdlib {

namespace {

class Environment {
 public:
  typedef std::vector<std::string> Strings;

  static char* getenv(const char* name);
  static int setenv(const char* name, const char* value, int overwrite);
  static int unsetenv(const char* name);
  static char** InitializeGlobalEnviron();

 private:
  static NoDestructor<Strings> stored_environment_;
  static NoDestructor<std::vector<char*>> environ_pointers_;

  static void RebuildAndSetGlobalEnviron();

  // Returns true if name is valid as a name for an environment variable.
  static bool EnvironmentNameIsValid(const char* name);

  // Helper function to find an environment variable by name.
  // Returns a pair containing an iterator and the starting position of the
  // value. If not found, the iterator will be stored_environment_->end().
  static std::pair<Strings::iterator, size_t> FindEnvironmentVariable(
      const char* name);
};

NoDestructor<Environment::Strings> Environment::stored_environment_;
NoDestructor<std::vector<char*>> Environment::environ_pointers_;

std::pair<Environment::Strings::iterator, size_t>
Environment::FindEnvironmentVariable(const char* name) {
  auto& stored_strings = *stored_environment_;
  const std::string prefix = std::string(name) + "=";

  auto it = std::find_if(
      stored_strings.begin(), stored_strings.end(),
      [&](const std::string& entry) { return entry.find(prefix, 0) == 0; });

  return {it, it == stored_strings.end() ? std::string::npos : prefix.length()};
}

void Environment::RebuildAndSetGlobalEnviron() {
  auto& stored_strings = *stored_environment_;
  auto& pointers_vec = *environ_pointers_;

  pointers_vec.clear();

  for (std::string& entry : stored_strings) {
    pointers_vec.push_back(entry.data());
  }
  pointers_vec.push_back(nullptr);

  ::environ = pointers_vec.data();
}

char** Environment::InitializeGlobalEnviron() {
  // Set the PATH environment variable, expected by `EnvironmentTest` from
  // base:base_unittests to exist and not be an empty string.
  setenv("PATH", "none", 0);

  // Initialize the TZ variable from the Starboard API.
  const char* iana_id = SbTimeZoneGetName();
  if (iana_id) {
    // It is best to set TZ here versus during the first call to tzset(). TZ is
    // the authoritative source for the configured timezone of the system. This
    // will allow programs to read the TZ environment variable without first
    // calling tzset().
    setenv("TZ", iana_id, 0 /* overwrite */);
  }

  // Initialize the LANG variable from the Starboard API.
  setenv("LANG", SbSystemGetLocaleId(), 0);

  RebuildAndSetGlobalEnviron();
  return ::environ;
}

bool Environment::EnvironmentNameIsValid(const char* name) {
  return name && name[0] != '\0' && std::strchr(name, '=') == nullptr;
}

char* Environment::getenv(const char* name) {
  if (!EnvironmentNameIsValid(name)) {
    return nullptr;
  }

  auto [it, value_pos] = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    return it->data() + value_pos;
  }
  return nullptr;
}

int Environment::setenv(const char* name, const char* value, int overwrite) {
  if (!EnvironmentNameIsValid(name)) {
    errno = EINVAL;
    return -1;
  }
  if (!value) {
    value = "";
  }

  std::string new_entry = std::string(name) + "=" + std::string(value);

  auto [it, unused_pos] = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    if (overwrite) {
      *it = new_entry;
      RebuildAndSetGlobalEnviron();
    }
  } else {
    stored_environment_->push_back(new_entry);
    RebuildAndSetGlobalEnviron();
  }

  return 0;
}

int Environment::unsetenv(const char* name) {
  if (!EnvironmentNameIsValid(name)) {
    errno = EINVAL;
    return -1;
  }

  auto [it, unused_pos] = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    stored_environment_->erase(it);
    RebuildAndSetGlobalEnviron();
  }

  return 0;
}

}  // namespace

}  // namespace stdlib
}  // namespace libc
}  // namespace common
}  // namespace cobalt

using cobalt::common::libc::stdlib::Environment;

// The symbols below must have C linkage.
extern "C" {

// This file provides a hermetic environment implementation, including the
// global |environ| variable and the |getenv|, |setenv|, and |unsetenv|
// functions.
//
// By isolating the application from the host OS's environment, this ensures
// its behavior is not influenced by external settings. The application does
// not inherit the parent process's environment by design.
//
// This approach guarantees that platform-specific environment requirements or
// limitations do not affect the application. To use environment variables from
// the host, they must be passed to the application separately through a
// dedicated configuration API (for example SbSystemGetLocaleId).
//
// Note that this hermetic environment is scoped to the application. Code
// operating below the Starboard layer remains free to use the platform's
// native environment directly.

char** environ = Environment::InitializeGlobalEnviron();

char* getenv(const char* name) {
  return Environment::getenv(name);
}

int setenv(const char* name, const char* value, int overwrite) {
  return Environment::setenv(name, value, overwrite);
}

int unsetenv(const char* name) {
  return Environment::unsetenv(name);
}

}  // extern "C"
