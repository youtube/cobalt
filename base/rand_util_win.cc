// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <stdlib.h>

#include <objbase.h>
#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"

namespace {

uint32 RandUint32() {
  uint32 number;
  CHECK_EQ(rand_s(&number), 0);
  return number;
}

}  // namespace

namespace base {

uint64 RandUint64() {
  uint32 first_half = RandUint32();
  uint32 second_half = RandUint32();
  return (static_cast<uint64>(first_half) << 32) + second_half;
}

std::string GenerateGUID() {
  const int kGUIDSize = 39;

  GUID guid;
  HRESULT guid_result = CoCreateGuid(&guid);
  DCHECK(SUCCEEDED(guid_result));

  std::wstring guid_string;
  int result = StringFromGUID2(guid,
                               WriteInto(&guid_string, kGUIDSize), kGUIDSize);
  DCHECK(result == kGUIDSize);

  return WideToUTF8(guid_string.substr(1, guid_string.length() - 2));
}

}  // namespace base
