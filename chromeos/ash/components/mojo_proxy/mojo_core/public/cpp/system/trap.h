// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_TRAP_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_TRAP_H_

#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/trap.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/types.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/system/system_export.h"

namespace mojo_legacy {

// A strongly-typed representation of a |MojoHandle| for a trap.
class TrapHandle : public Handle {
 public:
  TrapHandle() = default;
  explicit TrapHandle(MojoHandle value) : Handle(value) {}

  // Copying and assignment allowed.
};

static_assert(sizeof(TrapHandle) == sizeof(Handle),
              "Bad size for C++ TrapHandle");

typedef ScopedHandleBase<TrapHandle> ScopedTrapHandle;
static_assert(sizeof(ScopedTrapHandle) == sizeof(TrapHandle),
              "Bad size for C++ ScopedTrapHandle");

MOJO_LEGACY_CPP_SYSTEM_EXPORT MojoResult
CreateTrap(MojoTrapEventHandler handler, ScopedTrapHandle* trap_handle);

}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_PUBLIC_CPP_SYSTEM_TRAP_H_
