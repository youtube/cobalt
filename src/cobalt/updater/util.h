// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_UTIL_H_
#define COBALT_UPDATER_UTIL_H_

#include <string>

namespace base {
class FilePath;
}

namespace cobalt {
namespace updater {

// Returns a directory where updater files or its data is stored.
bool GetProductDirectory(base::FilePath* path);

// Returns the Evergreen version of the running installation.
const std::string GetEvergreenVersion();

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UTIL_H_
