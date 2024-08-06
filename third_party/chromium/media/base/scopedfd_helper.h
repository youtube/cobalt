// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_SCOPEDFD_HELPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_SCOPEDFD_HELPER_H_

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "third_party/chromium/media/base/media_export.h"

namespace media_m96 {

// Theoretically, we can test on defined(OS_POSIX) || defined(OS_FUCHSIA), but
// since the only current user is V4L2 we are limiting the scope to OS_LINUX so
// the binary size does not inflate on non-using systems. Feel free to adapt
// this and BUILD.gn as our needs evolve.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)

// Return a new vector containing duplicates of |fds|, or PCHECKs in case of an
// error.
MEDIA_EXPORT std::vector<base::ScopedFD> DuplicateFDs(
    const std::vector<base::ScopedFD>& fds);

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_SCOPEDFD_HELPER_H_
