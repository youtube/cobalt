// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_EVERGREEN_CONFIG_H_
#define STARBOARD_ELF_LOADER_EVERGREEN_CONFIG_H_

#include <string>

#include "starboard/configuration.h"

namespace starboard {
namespace elf_loader {

// This is configuration published by the Evergreen loader.
// The Starboard implementation may customize its behavior
// based on it e.g. override the content location in SbSystemGetPath or
// add an custom extension to SbSystemGetExtension.
struct EvergreenConfig {
  // Factory method to create the configuration.
  static void Create(const char* library_path,
                     const char* content_path,
                     const void* (*custom_get_extension_)(const char* name));

  // Retrieves the singleton instance of the configuration.
  static const EvergreenConfig* GetInstance();

  // Absolute path to the shared library binary.
  const std::string library_path_;

  // Absolute path to the content directory for this binary.
  const std::string content_path_;

  // Getter to a custom extension to be added to Starboard.
  const void* (*custom_get_extension_)(const char* name);

 private:
  EvergreenConfig(const char* library_path,
                  const char* content_path,
                  const void* (*custom_get_extension_)(const char* name));
  EvergreenConfig(const EvergreenConfig&) = delete;
  void operator=(const EvergreenConfig&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_EVERGREEN_CONFIG_H_
