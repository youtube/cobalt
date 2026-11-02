// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_SWITCHES_H_
#define GPU_CONFIG_GPU_SWITCHES_H_

<<<<<<< HEAD
#include "gpu/config/gpu_config_export.h"

namespace switches {

GPU_CONFIG_EXPORT extern const char kDisableGpuRasterization[];
GPU_CONFIG_EXPORT extern const char kDisableMipmapGeneration[];
GPU_CONFIG_EXPORT extern const char kEnableGpuRasterization[];
GPU_CONFIG_EXPORT extern const char kGpuBlocklistTestGroup[];
GPU_CONFIG_EXPORT extern const char kGpuDriverBugListTestGroup[];
GPU_CONFIG_EXPORT extern const char kGpuPreferences[];
GPU_CONFIG_EXPORT extern const char kIgnoreGpuBlocklist[];
GPU_CONFIG_EXPORT extern const char kGpuDiskCacheSizeKB[];
GPU_CONFIG_EXPORT extern const char kDisableGpuShaderDiskCache[];
GPU_CONFIG_EXPORT extern const char kDisableGpuProcessForDX12InfoCollection[];
GPU_CONFIG_EXPORT extern const char kEnableWebGLDraftExtensions[];
GPU_CONFIG_EXPORT extern const char kEnableUnsafeWebGPU[];
GPU_CONFIG_EXPORT extern const char kForceHighPerformanceGPU[];
GPU_CONFIG_EXPORT extern const char kEnableWebGPUDeveloperFeatures[];
GPU_CONFIG_EXPORT extern const char kEnableDawnBackendValidation[];
GPU_CONFIG_EXPORT extern const char kUseWebGPUAdapter[];
GPU_CONFIG_EXPORT extern const char kUseWebGPUPowerPreference[];
GPU_CONFIG_EXPORT extern const char kForceWebGPUCompat[];
GPU_CONFIG_EXPORT extern const char kEnableDawnFeatures[];
GPU_CONFIG_EXPORT extern const char kDisableDawnFeatures[];
GPU_CONFIG_EXPORT extern const char kCollectDawnInfoEagerly[];
GPU_CONFIG_EXPORT extern const char kNoDelayForDX12VulkanInfoCollection[];
GPU_CONFIG_EXPORT extern const char kEnableGpuBlockedTime[];
GPU_CONFIG_EXPORT extern const char kGpuVendorId[];
GPU_CONFIG_EXPORT extern const char kGpuDeviceId[];
GPU_CONFIG_EXPORT extern const char kGpuSubSystemId[];
GPU_CONFIG_EXPORT extern const char kGpuRevision[];
GPU_CONFIG_EXPORT extern const char kGpuDriverVersion[];
GPU_CONFIG_EXPORT extern const char kWebViewDrawFunctorUsesVulkan[];
GPU_CONFIG_EXPORT extern const char kEnableVulkanProtectedMemory[];
GPU_CONFIG_EXPORT extern const char kDisableVulkanFallbackToGLForTesting[];
GPU_CONFIG_EXPORT extern const char kVulkanHeapMemoryLimitMb[];
GPU_CONFIG_EXPORT extern const char kVulkanSyncCpuMemoryLimitMb[];
GPU_CONFIG_EXPORT extern const char kForceBrowserCrashOnGpuCrash[];
GPU_CONFIG_EXPORT extern const char kGpuWatchdogTimeoutSeconds[];
GPU_CONFIG_EXPORT extern const char kForceSeparateEGLDisplayForWebGLTesting[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackend[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendD3D11[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendD3D12[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendMetal[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendOpenGLES[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendSwiftshader[];
GPU_CONFIG_EXPORT extern const char kSkiaGraphiteDawnBackendVulkan[];
GPU_CONFIG_EXPORT extern const char kDisableSkiaGraphite[];
GPU_CONFIG_EXPORT extern const char kEnableSkiaGraphite[];
GPU_CONFIG_EXPORT extern const char kDisableSkiaGraphitePrecompilation[];
GPU_CONFIG_EXPORT extern const char kEnableSkiaGraphitePrecompilation[];
GPU_CONFIG_EXPORT extern const char kEnableGpuMainTimeKeeperMetrics[];
GPU_CONFIG_EXPORT extern const char kSuppressPerformanceLogs[];
=======
#include "build/build_config.h"
#include "gpu/gpu_export.h"

namespace switches {

GPU_EXPORT extern const char kDisableGpuRasterization[];
GPU_EXPORT extern const char kDisableMipmapGeneration[];
GPU_EXPORT extern const char kEnableGpuRasterization[];
GPU_EXPORT extern const char kGpuBlocklistTestGroup[];
GPU_EXPORT extern const char kGpuDriverBugListTestGroup[];
GPU_EXPORT extern const char kGpuPreferences[];
GPU_EXPORT extern const char kIgnoreGpuBlocklist[];
GPU_EXPORT extern const char kGpuDiskCacheSizeKB[];
#if BUILDFLAG(IS_COBALT)
GPU_EXPORT extern const char kGpuShaderDiskCacheGrOnly[];
GPU_EXPORT extern const char kDisablePreloadGpuShaderDiskCache[];
#endif  // BUILDFLAG(IS_COBALT)
GPU_EXPORT extern const char kDisableGpuProcessForDX12InfoCollection[];
GPU_EXPORT extern const char kEnableUnsafeWebGPU[];
GPU_EXPORT extern const char kForceHighPerformanceGPU[];
GPU_EXPORT extern const char kEnableWebGPUDeveloperFeatures[];
GPU_EXPORT extern const char kEnableDawnBackendValidation[];
GPU_EXPORT extern const char kUseWebGPUAdapter[];
GPU_EXPORT extern const char kUseWebGPUPowerPreference[];
GPU_EXPORT extern const char kForceWebGPUCompat[];
GPU_EXPORT extern const char kEnableDawnFeatures[];
GPU_EXPORT extern const char kDisableDawnFeatures[];
GPU_EXPORT extern const char kCollectDawnInfoEagerly[];
GPU_EXPORT extern const char kNoDelayForDX12VulkanInfoCollection[];
GPU_EXPORT extern const char kEnableGpuBlockedTime[];
GPU_EXPORT extern const char kGpuVendorId[];
GPU_EXPORT extern const char kGpuDeviceId[];
GPU_EXPORT extern const char kGpuSubSystemId[];
GPU_EXPORT extern const char kGpuRevision[];
GPU_EXPORT extern const char kGpuDriverVersion[];
GPU_EXPORT extern const char kWebViewDrawFunctorUsesVulkan[];
GPU_EXPORT extern const char kEnableVulkanProtectedMemory[];
GPU_EXPORT extern const char kDisableVulkanFallbackToGLForTesting[];
GPU_EXPORT extern const char kVulkanHeapMemoryLimitMb[];
GPU_EXPORT extern const char kVulkanSyncCpuMemoryLimitMb[];
GPU_EXPORT extern const char kForceBrowserCrashOnGpuCrash[];
GPU_EXPORT extern const char kGpuWatchdogTimeoutSeconds[];
GPU_EXPORT extern const char kForceSeparateEGLDisplayForWebGLTesting[];
GPU_EXPORT extern const char kSkiaGraphiteBackend[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawn[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawnD3D11[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawnD3D12[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawnMetal[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawnSwiftshader[];
GPU_EXPORT extern const char kSkiaGraphiteBackendDawnVulkan[];
GPU_EXPORT extern const char kSkiaGraphiteBackendMetal[];
GPU_EXPORT extern const char kDisableSkiaGraphite[];
GPU_EXPORT extern const char kEnableSkiaGraphite[];
GPU_EXPORT extern const char kDisableSkiaGraphitePrecompilation[];
GPU_EXPORT extern const char kEnableSkiaGraphitePrecompilation[];
GPU_EXPORT extern const char kUseRedistributableDirectML[];
GPU_EXPORT extern const char kEnableGpuMainTimeKeeperMetrics[];
GPU_EXPORT extern const char kSuppressPerformanceLogs[];
>>>>>>> 0521d9ddb45 (cobalt: Enable GrShaderCache on Starboard for preload launches (#13596))

}  // namespace switches

#endif  // GPU_CONFIG_GPU_SWITCHES_H_
