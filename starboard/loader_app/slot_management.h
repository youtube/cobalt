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

#ifndef STARBOARD_LOADER_APP_SLOT_MANAGEMENT_H_
#define STARBOARD_LOADER_APP_SLOT_MANAGEMENT_H_

#include <string>

namespace starboard {
namespace loader_app {

// Interface for loading a library.
class LibraryLoader {
 public:
  virtual ~LibraryLoader() {}

  // Load the library with the provided full path to |library_path| and
  // |content_path|. If |use_compression| is true the library is assumed
  // to be compressed. If |use_memory_mapped_file| is true the library
  // would be loaded as a memory mapped file. The |use_compression| and
  // |use_memory_mapped_file|  are not compatible and can't be both enabled.
  virtual bool Load(const std::string& library_path,
                    const std::string& content_path,
                    bool use_compression,
                    bool use_memory_mapped_file,
                    bool use_binary_diff) = 0;

  // Resolve a symbol by name.
  virtual void* Resolve(const std::string& symbol) = 0;
};

// Load the library for the app specified by |app_key| and manage the
// current slot selection by rolling forward or back based on the slot status.
// The actual loading from the slot is performed by the |library_loader|.
// An alternative content can be used by specifying non-empty
// |alternative_content_path| with the full path to the content.
// If |use_memory_mapped_file| is true the library would be loaded as a memory
// mapped file. It should not be enabled if the library selected may be
// compressed since compression and memory mapping are incompatible.
// Returns a pointer to the |SbEventHandle| symbol in the library.
void* LoadSlotManagedLibrary(const std::string& app_key,
                             const std::string& alternative_content_path,
                             LibraryLoader* library_loader,
                             bool use_memory_mapped_file);

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_SLOT_MANAGEMENT_H_
