// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/djb2.h"

uint32_t DJB2Hash(const void* buf, size_t len, uint32_t seed) {
  const uint8_t* src = reinterpret_cast<const uint8_t*>(buf);
  uint32_t hash = seed;
  for (size_t i = 0; i < len; ++i) {
    hash = hash * 33 + src[i];
  }
  return hash;
}
