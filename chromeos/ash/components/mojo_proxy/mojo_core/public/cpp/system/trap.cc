// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/trap.h"

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/functions.h"

namespace mojo_legacy {

MojoResult CreateTrap(MojoTrapEventHandler handler,
                      ScopedTrapHandle* trap_handle) {
  MojoHandle handle;
  MojoResult rv = MojoCreateTrap(handler, nullptr, &handle);
  if (rv == MOJO_LEGACY_RESULT_OK) {
    trap_handle->reset(TrapHandle(handle));
  }
  return rv;
}

}  // namespace mojo_legacy
