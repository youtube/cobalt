// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PLATFORM_HANDLE_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PLATFORM_HANDLE_UTILS_H_

#include "base/memory/platform_shared_memory_region.h"
#include "base/process/process.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/core/system_impl_export.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/invitation.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/platform_handle.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/c/system/types.h"
#include "chromeos/ash/components/mojo_proxy/mojo_core/public/cpp/platform/platform_handle.h"

namespace mojo_legacy {
namespace core {

// Converts a base shared memory platform handle into one (maybe two on POSIX)
// PlatformHandle(s).
MOJO_LEGACY_SYSTEM_IMPL_EXPORT void
ExtractPlatformHandlesFromSharedMemoryRegionHandle(
    base::subtle::ScopedPlatformSharedMemoryHandle handle,
    PlatformHandle* extracted_handle,
    PlatformHandle* extracted_readonly_handle);

// Converts one (maybe two on POSIX) PlatformHandle(s) to a base shared memory
// platform handle.
MOJO_LEGACY_SYSTEM_IMPL_EXPORT
base::subtle::ScopedPlatformSharedMemoryHandle
CreateSharedMemoryRegionHandleFromPlatformHandles(
    PlatformHandle handle,
    PlatformHandle readonly_handle);

// Takes a MojoPlatformProcessHandle, which does not own the handle value
// contained within, duplicates the value, and stores the strongly-owned result
// in |process|.
MojoResult UnwrapAndClonePlatformProcessHandle(
    const MojoPlatformProcessHandle* process_handle,
    base::Process& process);

}  // namespace core
}  // namespace mojo_legacy

#endif  // CHROMEOS_ASH_COMPONENTS_MOJO_PROXY_MOJO_CORE_CORE_PLATFORM_HANDLE_UTILS_H_
