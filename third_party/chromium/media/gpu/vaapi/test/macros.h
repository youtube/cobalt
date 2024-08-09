// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_TEST_MACROS_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_TEST_MACROS_H_

#include "base/logging.h"

#define VA_LOG_ASSERT(va_error, name)         \
  LOG_ASSERT((va_error) == VA_STATUS_SUCCESS) \
      << name << " failed, VA error: " << vaErrorStr(va_error);

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_TEST_MACROS_H_
