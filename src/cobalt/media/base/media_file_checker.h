// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BASE_MEDIA_FILE_CHECKER_H_
#define COBALT_MEDIA_BASE_MEDIA_FILE_CHECKER_H_

#include "base/basictypes.h"
#include "base/files/file.h"
#include "cobalt/media/base/media_export.h"

namespace base {
class TimeDelta;
}

namespace media {

// This class tries to determine if a file is a valid media file. The entire
// file is not decoded so a positive result from this class does not make the
// file safe to use in the browser process.
class MEDIA_EXPORT MediaFileChecker {
 public:
  explicit MediaFileChecker(base::File file);
  ~MediaFileChecker();

  // After opening |file|, up to |check_time| amount of wall-clock time is spent
  // decoding the file. The amount of audio/video data decoded will depend on
  // the bitrate of the file and the speed of the CPU.
  bool Start(base::TimeDelta check_time);

 private:
  base::File file_;

  DISALLOW_COPY_AND_ASSIGN(MediaFileChecker);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_MEDIA_FILE_CHECKER_H_
