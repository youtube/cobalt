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
#include <vector>

// This file can not depend on //base since that indirectly depends on this
// file, but since base::NoDestructor is a header-only class, we can safely use
// it here.
#include "base/no_destructor.h"  // nogncheck

namespace {

class Environment {
 public:
  typedef std::vector<std::string> Strings;

  static char* getenv_impl(const char* name);
  static int setenv_impl(const char* name, const char* value, int overwrite);
  static int unsetenv_impl(const char* name);
  static char** InitializeGlobalEnviron();

 private:
  static base::NoDestructor<Strings> stored_environment_;
  static base::NoDestructor<std::vector<char*>> environ_pointers_;

  static void RebuildAndSetGlobalEnviron();

  // Helper function to find an environment variable by name.
  // Returns an iterator to the found element, or stored_environment_->end() if
  // not found.
  static Strings::iterator FindEnvironmentVariable(const char* name);
};

base::NoDestructor<Environment::Strings> Environment::stored_environment_;
base::NoDestructor<std::vector<char*>> Environment::environ_pointers_;

Environment::Strings::iterator Environment::FindEnvironmentVariable(
    const char* name) {
  auto& stored_strings = *stored_environment_;
  size_t name_len = std::strlen(name);
  return std::find_if(stored_strings.begin(), stored_strings.end(),
                      [&](const std::string& entry) {
                        return entry.length() > name_len &&
                               entry[name_len] == '=' &&
                               std::strncmp(entry.c_str(), name, name_len) == 0;
                      });
}

void Environment::RebuildAndSetGlobalEnviron() {
  auto& stored_strings = *stored_environment_;
  auto& pointers_vec = *environ_pointers_;

  pointers_vec.clear();

  for (const std::string& entry : stored_strings) {
    // const_cast is necessary because ::environ is char**,
    // but our source strings are std::string.
    pointers_vec.push_back(const_cast<char*>(entry.c_str()));
  }
  pointers_vec.push_back(nullptr);

  ::environ = pointers_vec.data();
}

char** Environment::InitializeGlobalEnviron() {
  // Set the PATH environment variable, expected by `EnvironmentTest` from
  // base:base_unittests to exist and not be an empty string.
  setenv("PATH", "none", 0);
  RebuildAndSetGlobalEnviron();
  return ::environ;
}

char* Environment::getenv_impl(const char* name) {
  if (!name) {
    return nullptr;
  }

  auto it = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    size_t name_len = std::strlen(name);
    return const_cast<char*>(it->c_str() + name_len + 1);
  }
  return nullptr;
}

int Environment::setenv_impl(const char* name,
                             const char* value,
                             int overwrite) {
  if (!name || name[0] == '\0' || std::strchr(name, '=') != nullptr) {
    errno = EINVAL;
    return -1;
  }
  if (!value) {
    value = "";
  }

  std::string new_entry = std::string(name) + "=" + std::string(value);
  bool changed_in_storage = false;

  auto it = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    if (overwrite) {
      *it = new_entry;
      changed_in_storage = true;
    }
  } else {
    stored_environment_->push_back(new_entry);
    changed_in_storage = true;
  }

  if (changed_in_storage) {
    RebuildAndSetGlobalEnviron();
  }
  return 0;
}

int Environment::unsetenv_impl(const char* name) {
  if (!name || name[0] == '\0' || std::strchr(name, '=') != nullptr) {
    errno = EINVAL;
    return -1;
  }

  bool changed_in_storage = false;
  auto it = FindEnvironmentVariable(name);

  if (it != stored_environment_->end()) {
    stored_environment_->erase(it);
    changed_in_storage = true;
  }

  if (changed_in_storage) {
    RebuildAndSetGlobalEnviron();
  }
  return 0;
}

}  // namespace

#ifdef __cplusplus
extern "C" {
#endif

char** environ = Environment::InitializeGlobalEnviron();

char* getenv(const char* name) {
  return Environment::getenv_impl(name);
}

int setenv(const char* name, const char* value, int overwrite) {
  return Environment::setenv_impl(name, value, overwrite);
}

int unsetenv(const char* name) {
  return Environment::unsetenv_impl(name);
}

#ifdef __cplusplus
}  // extern "C"
#endif
