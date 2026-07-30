// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits.h>  // for HOST_NAME_MAX
#include <unistd.h>  // for gethostname()

#include <string>

#include "base/linux_util.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  char hostname[HOST_NAME_MAX];
  if (gethostname(hostname, HOST_NAME_MAX) == 0) {  // Success.
    // Many Linux installations default to "localhost" or
    // "localhost.localdomain" if the hostname is not configured during setup.
    // These are generic names and do not help identify the device, so they are
    // discarded in favor of the Linux distribution name.
    std::string hostname_str(hostname);
    if (hostname_str != "localhost" &&
        hostname_str != "localhost.localdomain") {
      return hostname_str;
    }
  }
  return base::GetLinuxDistro();
}

}  // namespace syncer
