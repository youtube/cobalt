// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_ENTRYPOINTS_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_ENTRYPOINTS_H_

#include "chromeos/ash/components/mojo_proxy/mojo_core/core/system_impl_export.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/thunks.h"

namespace mojo_legacy {
namespace core {

// Initializes the global Core object.
MOJO_LEGACY_SYSTEM_IMPL_EXPORT void InitializeCore();

// Destroys the global Core object.
MOJO_LEGACY_SYSTEM_IMPL_EXPORT void ShutDownCore();

// Returns a MojoSystemThunks2 struct populated with the EDK's implementation
// of each function. This may be used by embedders to populate thunks for
// application loading.
MOJO_LEGACY_SYSTEM_IMPL_EXPORT const MojoSystemThunks2& GetSystemThunks();

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_ENTRYPOINTS_H_
