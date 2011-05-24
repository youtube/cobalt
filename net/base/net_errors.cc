// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include "base/basictypes.h"
#include "base/metrics/histogram.h"
#include "base/stringize_macros.h"

namespace {

// Get all valid error codes into an array as positive numbers, for use in the
// |GetAllErrorCodesForUma| function below.
#define NET_ERROR(label, value) -(value),
const int kAllErrorCodes[] = {
#include "net/base/net_error_list.h"
};
#undef NET_ERROR

}  // namespace

namespace net {

const char kErrorDomain[] = "net";

const char* ErrorToString(int error) {
  if (error == 0)
    return "net::OK";

  switch (error) {
#define NET_ERROR(label, value) \
  case ERR_ ## label: \
    return "net::" STRINGIZE_NO_EXPANSION(ERR_ ## label);
#include "net/base/net_error_list.h"
#undef NET_ERROR
  default:
    return "net::<unknown>";
  }
}

std::vector<int> GetAllErrorCodesForUma() {
  return base::CustomHistogram::ArrayToCustomRanges(
      kAllErrorCodes, arraysize(kAllErrorCodes));
}

}  // namespace net
