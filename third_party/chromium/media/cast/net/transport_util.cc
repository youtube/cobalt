// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/transport_util.h"

namespace media {
namespace cast {
namespace transport_util {

int LookupOptionWithDefault(const base::DictionaryValue& options,
                            const std::string& path,
                            int default_value) {
  int ret;
  if (options.GetInteger(path, &ret)) {
    return ret;
  } else {
    return default_value;
  }
}

}  // namespace transport_util
}  // namespace cast
}  // namespace media
