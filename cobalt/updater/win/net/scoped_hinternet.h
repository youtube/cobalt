// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_NET_SCOPED_HINTERNET_H_
#define CHROME_UPDATER_WIN_NET_SCOPED_HINTERNET_H_

#include <windows.h>
#include <winhttp.h>

#include "base/scoped_generic.h"

namespace updater {

namespace internal {

struct ScopedHInternetTraits {
  static HINTERNET InvalidValue() { return nullptr; }
  static void Free(HINTERNET handle) {
    if (handle != InvalidValue())
      WinHttpCloseHandle(handle);
  }
};

}  // namespace internal

// Manages the lifetime of HINTERNET handles allocated by WinHTTP.
using scoped_hinternet =
    base::ScopedGeneric<HINTERNET, updater::internal::ScopedHInternetTraits>;

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_NET_SCOPED_HINTERNET_H_
