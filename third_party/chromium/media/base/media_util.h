// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_UTIL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_UTIL_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/base/media_log.h"

namespace media_m96 {

// Simply returns an empty vector. {Audio|Video}DecoderConfig are often
// constructed with empty extra data.
MEDIA_EXPORT std::vector<uint8_t> EmptyExtraData();

// Helpers for PPAPI UMAs. There wasn't an obvious place to put them in
// //content/renderer/pepper.
MEDIA_EXPORT void ReportPepperVideoDecoderOutputPictureCountHW(int height);
MEDIA_EXPORT void ReportPepperVideoDecoderOutputPictureCountSW(int height);

class MEDIA_EXPORT NullMediaLog : public media_m96::MediaLog {
 public:
  NullMediaLog() = default;

  NullMediaLog(const NullMediaLog&) = delete;
  NullMediaLog& operator=(const NullMediaLog&) = delete;

  ~NullMediaLog() override = default;

  void AddLogRecordLocked(
      std::unique_ptr<media_m96::MediaLogRecord> event) override {}
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_BASE_MEDIA_UTIL_H_
