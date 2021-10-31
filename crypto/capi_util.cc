// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/capi_util.h"

#include <stdlib.h>

#include "starboard/memory.h"
#include "starboard/types.h"

namespace crypto {

void* WINAPI CryptAlloc(size_t size) {
  return SbMemoryAllocate(size);
}

void WINAPI CryptFree(void* p) {
  SbMemoryFree(p);
}

}  // namespace crypto
