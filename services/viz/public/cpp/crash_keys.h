// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_
#define SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_

#include "base/strings/string_piece.h"

namespace viz {

// Sets a crash key to indicate what structure triggered a deserialization error
// in viz mojom code.
void SetDeserializationCrashKeyString(base::StringPiece str);

}  // namespace viz

#endif  // SERVICES_VIZ_PUBLIC_CPP_CRASH_KEYS_H_
