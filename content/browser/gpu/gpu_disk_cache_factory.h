// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_
#define CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "gpu/ipc/host/gpu_disk_cache.h"

#if BUILDFLAG(IS_COBALT)
#include "base/files/file_path.h"
#include "gpu/ipc/common/gpu_disk_cache_type.h"
#endif

namespace content {

// Initializes the GpuDiskCacheFactory singleton instance.
CONTENT_EXPORT void InitGpuDiskCacheFactorySingleton();

// Returns an instance previously created by InitGpuDiskCacheFactorySingleton().
// This can return nullptr if an instance has not yet been created.
CONTENT_EXPORT gpu::GpuDiskCacheFactory* GetGpuDiskCacheFactorySingleton();

#if BUILDFLAG(IS_COBALT)
// Resolves the directory path for a given GPU disk cache type under
// base::DIR_CACHE (or returns an empty FilePath if DIR_CACHE is unavailable).
CONTENT_EXPORT base::FilePath GetCobaltGpuDiskCachePath(
    gpu::GpuDiskCacheType type);
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DISK_CACHE_FACTORY_H_
