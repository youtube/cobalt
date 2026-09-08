// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_

#include "base/clang_profiling_buildflags.h"
#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "gpu/config/gpu_preferences.h"

namespace gpu {
class GpuChannelEstablishFactory;
}

namespace content {

CONTENT_EXPORT const gpu::GpuPreferences GetGpuPreferencesFromCommandLine();

// Kills the GPU process with a normal termination status.
CONTENT_EXPORT void KillGpuProcess();

#if BUILDFLAG(IS_COBALT)
// Asynchronously triggers full GPU resource and EGL display teardown on the
// GPU service, executing |callback| on the UI thread once the teardown is
// completely finished via a Mojo IPC barrier. Used during Cobalt conceal
// to guarantee GPU resources are freed before native window destruction.
CONTENT_EXPORT void CleanupGpuProcessOnUI(base::OnceClosure callback);

// Notifies the GPU service from the UI thread that the application is
// returning to the foreground, re-initializing the EGL display and default
// offscreen surface, and flushing queued GPU channel requests.
CONTENT_EXPORT void RestoreGpuProcessOnUI();
#endif

CONTENT_EXPORT gpu::GpuChannelEstablishFactory* GetGpuChannelEstablishFactory();

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
CONTENT_EXPORT void DumpGpuProfilingData(base::OnceClosure callback);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_UTILS_H_
