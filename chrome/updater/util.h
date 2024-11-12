// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_H_
#define CHROME_UPDATER_UTIL_H_

#include <string>

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

// Legacy prod config containing all prod, tests and static channels with all
// SB versions of C25 and prior.
extern const char kOmahaCobaltAppID[];

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

#endif  // CHROME_UPDATER_UTIL_H_
