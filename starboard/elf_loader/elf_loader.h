// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_ELF_LOADER_H_
#define STARBOARD_ELF_LOADER_ELF_LOADER_H_

#include <string>

#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"

namespace starboard {
namespace elf_loader {

class ElfLoaderImpl;

// A loader for ELF dynamic shared library.
class ElfLoader {
 public:
  ElfLoader();
  ~ElfLoader();

  // Gets the current instance of the ELF Loader. SB_DCHECKS if called before
  // the ELF Loader has been constructed.
  static ElfLoader* Get();

  // Loads the shared library. Returns false if |library_path| or |content_path|
  // is empty, or if the library could not be loaded.
  // An optional |custom_get_extension| function pointer can be passed in order
  // to override the |SbSystemGetExtension| function.
  bool Load(const std::string& library_path,
            const std::string& content_path,
            bool is_relative_path,
            const void* (*custom_get_extension)(const char* name) = NULL);

  // Looks up the symbol address in the
  // shared library.
  void* LookupSymbol(const char* symbol);

  const std::string& GetLibraryPath() const { return library_path_; }
  const std::string& GetContentPath() const { return content_path_; }

 private:
  // Adjusts |path| to be relative to the content directory of the ELF Loader.
  // Returns an empty string on error.
  std::string MakeRelativeToContentPath(const std::string& path);

  // The paths to the loaded shared library and it's content.
  std::string library_path_;
  std::string content_path_;

  // The ELF Loader implementation.
  scoped_ptr<ElfLoaderImpl> impl_;

  // The single ELF Loader instance.
  static ElfLoader* g_instance;

  ElfLoader(const ElfLoader&) = delete;
  void operator=(const ElfLoader&) = delete;
};

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_ELF_LOADER_H_
