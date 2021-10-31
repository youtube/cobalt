// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_platform.h"

#include <sstream>

// This is a reference implementation using the C++ standard library.
// Downstream projects may invoke their preferred routines instead,
// by modifying / replacing this file to call them.
// Examples of optimized string<->number conversion libraries:
// - https://github.com/google/double-conversion
// - https://github.com/abseil/abseil-cpp/blob/master/absl/strings/numbers.h
namespace crdtp {
namespace json {
namespace platform {
bool StrToD(const char* str, double* result) {
#if SB_IS(EVERGREEN)
#error "The std::locale::classic() is not supported for Evergreen. Please use base::StringToDouble()."
#endif
  std::istringstream is(str);
  is.imbue(std::locale::classic());
  is >> *result;
  return !is.fail() && is.eof();
}

std::string DToStr(double value) {
#if SB_IS(EVERGREEN)
#error "The std::locale::classic() is not supported for Evergreen. Please use base::NumberToString()."
#endif
  std::stringstream ss;
  ss.imbue(std::locale::classic());
  ss << value;
  return ss.str();
}
}  // namespace platform
}  // namespace json
}  // namespace crdtp
