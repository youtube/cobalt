// Copyright 2021 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_UTILS_H_
#define COBALT_UPDATER_UTILS_H_

#include <string>

#include "base/version.h"
#include "starboard/system.h"

namespace base {
class FilePath;
}

namespace cobalt {
namespace updater {

// The default manifest version to assume when the actual manifest cannot be
// parsed for any reason. This should not be used for installation manager
// errors, or any other error unrelated to parsing the manifest.
extern const char kDefaultManifestVersion[];

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

#endif  // COBALT_UPDATER_UTILS_H_
