// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIDEVINE_WIDEVINE_STORAGE_H_
#define STARBOARD_SHARED_WIDEVINE_WIDEVINE_STORAGE_H_

#include <map>
#include <string>
#include <vector>

#include "starboard/common/mutex.h"
#include "third_party/ce_cdm/cdm/include/cdm.h"

namespace starboard {
namespace shared {
namespace widevine {

// Manages the load and save of name/value pairs in std::string.  It is used by
// Widevine to store persistent data like device provisioning.
class WidevineStorage : public ::widevine::Cdm::IStorage {
 public:
  // This key is restricted to internal use only.
  static const char kCobaltWidevineKeyboxChecksumKey[];

  explicit WidevineStorage(const std::string& path_name);

  // For these accessor methods the |name| field cannot be
  // |kCobaltWidevineKeyboxChecksumKey|.
  bool read(const std::string& name, std::string* data) override;
  bool write(const std::string& name, const std::string& data) override;
  bool exists(const std::string& name) override;
  bool remove(const std::string& name) override;
  int32_t size(const std::string& name) override;

  // Populates |file_names| with the name of each file in the file system.
  // This is assumed to be a flat filename space (top level directory is
  // unnamed, and there are no subdirectories).
  bool list(std::vector<std::string>* records) override;

 private:
  bool readInternal(const std::string& name, std::string* data) const;
  bool writeInternal(const std::string& name, const std::string& data);
  bool existsInternal(const std::string& name) const;
  bool removeInternal(const std::string& name);

  Mutex lock_;
  std::string path_name_;
  std::map<std::string, std::string> cache_;
};

}  // namespace widevine
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIDEVINE_WIDEVINE_STORAGE_H_
