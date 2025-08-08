// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/log_util.h"

#include "base/strings/string_piece.h"

namespace media_router::log_util {

// Gets the first nine characters of an ID string.
//
// For Cast and DIAL sink IDs, this happens to return the cast: or dial: prefix
// and the first four characters of the UUID.
base::StringPiece TruncateId(base::StringPiece id) {
  return id.size() <= 9 ? id : id.substr(0, 9);
}

}  // namespace media_router::log_util
