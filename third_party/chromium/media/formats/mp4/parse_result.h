// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MP4_PARSE_RESULT_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MP4_PARSE_RESULT_H_

namespace media_m96 {
namespace mp4 {

enum class ParseResult {
  kOk,            // Parsing was successful.
  kError,         // The data is invalid (usually unrecoverable).
  kNeedMoreData,  // More data is required to parse successfully.
};

// Evaluate |expr| once. If the result is not ParseResult::kOk, (early) return
// it from the containing function.
#define RCHECK_OK_PARSE_RESULT(expr)                      \
  do {                                                    \
    ::media_m96::mp4::ParseResult result = (expr);            \
    if (result == ::media_m96::mp4::ParseResult::kError)      \
      DLOG(ERROR) << "Failure while parsing MP4: " #expr; \
    if (result != ::media_m96::mp4::ParseResult::kOk)         \
      return result;                                      \
  } while (0)

}  // namespace mp4
}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_FORMATS_MP4_PARSE_RESULT_H_
