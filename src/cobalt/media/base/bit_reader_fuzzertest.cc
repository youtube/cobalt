// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/hash.h"
#include "base/numerics/safe_conversions.h"
#include "cobalt/media/base/bit_reader.h"
#include "cobalt/media/base/test_random.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  media::BitReader reader(data, base::checked_cast<int>(size));

  // Need a simple random number generator to generate the number of bits to
  // read/skip in a reproducible way (given the same |data|). Using Hash() to
  // ensure the seed varies significantly over minor changes in |data|.
  media::TestRandom rnd(base::Hash(reinterpret_cast<const char*>(data), size));

  // Read and skip through the data in |reader|.
  while (reader.bits_available() > 0) {
    if (rnd.Rand() & 1) {
      // Read up to 64 bits. This may fail if there is not enough bits
      // remaining, but it doesn't matter (testing for failures is also good).
      uint64_t value;
      if (!reader.ReadBits(rnd.Rand() % 64 + 1, &value)) break;
    } else {
      // Skip up to 128 bits. As above, this may fail.
      if (!reader.SkipBits(rnd.Rand() % 128 + 1)) break;
    }
  }
  return 0;
}
