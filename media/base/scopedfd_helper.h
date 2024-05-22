// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SCOPEDFD_HELPER_H_
#define MEDIA_BASE_SCOPEDFD_HELPER_H_

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "media/base/media_export.h"

namespace media {

// Theoretically, we can test on BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA),
// but since the only current user is V4L2 we are limiting the scope to OS_LINUX
// so the binary size does not inflate on non-using systems. Feel free to adapt
// this and BUILD.gn as our needs evolve.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Return a new vector containing duplicates of |fds|, or PCHECKs in case of an
// error.
MEDIA_EXPORT std::vector<base::ScopedFD> DuplicateFDs(
    const std::vector<base::ScopedFD>& fds);

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

}  // namespace media

#endif  // MEDIA_BASE_SCOPEDFD_HELPER_H_
