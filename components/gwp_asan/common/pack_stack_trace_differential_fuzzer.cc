// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/pack_stack_trace.h"

#include <string.h>
#include <algorithm>
#include <memory>

// Tests that whatever we give to Pack() is the same as what comes out of
// Unpack().

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size) {
  if (Size < sizeof(size_t) * 2)
    return 0;

  size_t unpacked_max_size = reinterpret_cast<const size_t*>(Data)[0];
  size_t packed_max_size = reinterpret_cast<const size_t*>(Data)[1];
  Data += sizeof(size_t) * 2;
  Size -= sizeof(size_t) * 2;

  size_t entries = Size / sizeof(uintptr_t);

  // We don't need a buffer large than Size*10 as the longest variable length
  // encoding of a 64-bit integer is 10 bytes long.)
  size_t packed_array_size = std::min(Size * 10, packed_max_size);
  std::unique_ptr<uint8_t[]> packed(new uint8_t[packed_array_size]);
  size_t packed_out =
      gwp_asan::internal::Pack(reinterpret_cast<const uintptr_t*>(Data),
                               entries, packed.get(), packed_max_size);
  if (packed_out > packed_array_size)
    __builtin_trap();

  size_t unpacked_array_size = std::min(unpacked_max_size, entries);
  std::unique_ptr<uintptr_t[]> unpacked(new uintptr_t[unpacked_array_size]);
  size_t unpacked_out = gwp_asan::internal::Unpack(
      packed.get(), packed_out, unpacked.get(), unpacked_max_size);
  // We can only be sure there was enough room to pack the entire input when
  // packed_max_size was larger than the space allocated for packed data.
  if (packed_max_size > packed_array_size &&
      unpacked_out != unpacked_array_size)
    __builtin_trap();
  if (memcmp(Data, unpacked.get(), unpacked_out * sizeof(uintptr_t)))
    __builtin_trap();
  return 0;
}
