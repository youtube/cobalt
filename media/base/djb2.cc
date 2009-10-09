// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/djb2.h"

uint32 DJB2Hash(const void* buf, size_t len, uint32 hash) {
  const uint8* s = reinterpret_cast<const uint8*>(buf);
  if (len > 0) {
    do {
      hash = hash * 33 + *s++;
    } while (--len);
  }
  return hash;
}

