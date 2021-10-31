// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_UTILS_H_
#define COBALT_UPDATER_UTILS_H_

#include <string>

#include "base/version.h"

namespace base {
class FilePath;
}

namespace cobalt {
namespace updater {

// Create a directory where updater files or its data is stored.
bool CreateProductDirectory(base::FilePath* path);

// Return the path to the product directory where updater files or its data is
// stored.
bool GetProductDirectoryPath(base::FilePath* path);

// Returns the Evergreen version of the current installation.
const std::string GetCurrentEvergreenVersion();

// Read the Evergreen version of the installation dir.
base::Version ReadEvergreenVersion(base::FilePath installation_dir);

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UTILS_H_
