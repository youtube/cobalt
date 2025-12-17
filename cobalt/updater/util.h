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

#ifndef COBALT_UPDATER_UTIL_H_
#define COBALT_UPDATER_UTIL_H_

#include <functional>
#include <string>
#include <unordered_map>

#include "base/version.h"
#include "starboard/system.h"

namespace base {
class FilePath;
}

namespace cobalt {
namespace updater {

// Map of Omaha config IDs with channel and starboard version as indices.
extern const std::unordered_map<std::string, std::string>
    kChannelAndSbVersionToOmahaIdMap;

// The default manifest version to assume when the actual manifest cannot be
// parsed for any reason. This should not be used for installation manager
// errors, or any other error unrelated to parsing the manifest.
extern const char kDefaultManifestVersion[];

// Default config for the EAP branch.
extern const char kOmahaCobaltEAPAppID[];

extern const char kOmahaCobaltLTSNightlyAppID[];

extern const char kOmahaCobaltTrunkAppID[];

struct EvergreenLibraryMetadata {
  std::string version;
  std::string file_type;
};

// Create a directory where updater files or its data is stored.
bool CreateProductDirectory(base::FilePath* path);

// Return the path to the product directory where updater files or its data is
// stored.
bool GetProductDirectoryPath(base::FilePath* path);

// Returns the Evergreen library metadata of the current installation.
EvergreenLibraryMetadata GetCurrentEvergreenLibraryMetadata(
    std::function<const void*(const char*)> get_extension_fn =
        &SbSystemGetExtension);

// Returns the Evergreen version of the current installation.
const std::string GetCurrentEvergreenVersion(
    std::function<const void*(const char*)> get_extension_fn =
        &SbSystemGetExtension);

// Reads the Evergreen version of the installation dir.
base::Version ReadEvergreenVersion(base::FilePath installation_dir);

// Returns the hash of the libcobalt.so binary for the installation
// at slot |index|.
std::string GetLibrarySha256(int index);

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UTIL_H_
