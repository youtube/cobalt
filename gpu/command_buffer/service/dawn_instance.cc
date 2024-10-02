// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/dawn_instance.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "build/buildflag.h"
#include "gpu/config/gpu_preferences.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/bundle_locations.h"
#include "base/mac/foundation_util.h"
#endif

namespace gpu::webgpu {

// static
std::unique_ptr<DawnInstance> DawnInstance::Create(
    dawn::platform::Platform* platform,
    const GpuPreferences& gpu_preferences) {
  std::string dawn_search_path;
  base::FilePath module_path;
#if BUILDFLAG(IS_MAC)
  if (base::mac::AmIBundled()) {
    dawn_search_path = base::mac::FrameworkBundlePath()
                           .Append("Libraries")
                           .AsEndingWithSeparator()
                           .MaybeAsASCII();
  }
  if (dawn_search_path.empty())
#endif
  {
    if (base::PathService::Get(base::DIR_MODULE, &module_path)) {
      dawn_search_path = module_path.AsEndingWithSeparator().MaybeAsASCII();
    }
  }
  const char* dawn_search_path_c_str = dawn_search_path.c_str();

  std::vector<const char*> require_instance_enabled_toggles;
  std::vector<const char*> require_instance_disabled_toggles;

  // Create instance with all user-required toggles, those which are not
  // instance toggles will be ignored by Dawn.
  for (const std::string& toggles :
       gpu_preferences.enabled_dawn_features_list) {
    require_instance_enabled_toggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles :
       gpu_preferences.disabled_dawn_features_list) {
    require_instance_disabled_toggles.push_back(toggles.c_str());
  }

  WGPUDawnTogglesDescriptor dawn_toggles_desc = {
      .chain = {.sType = WGPUSType_DawnTogglesDescriptor},
      .enabledTogglesCount =
          static_cast<uint32_t>(require_instance_enabled_toggles.size()),
      .enabledToggles = require_instance_enabled_toggles.data(),
      .disabledTogglesCount =
          static_cast<uint32_t>(require_instance_disabled_toggles.size()),
      .disabledToggles = require_instance_disabled_toggles.data(),
  };
  WGPUDawnInstanceDescriptor dawn_instance_desc = {
      .chain =
          {
              .next = &dawn_toggles_desc.chain,
              .sType = WGPUSType_DawnInstanceDescriptor,
          },
      .additionalRuntimeSearchPathsCount = dawn_search_path.empty() ? 0u : 1u,
      .additionalRuntimeSearchPaths = &dawn_search_path_c_str,
  };
  WGPUInstanceDescriptor instance_desc = {
      .nextInChain = &dawn_instance_desc.chain,
  };

  auto instance = std::make_unique<DawnInstance>(&instance_desc);
  instance->SetPlatform(platform);

  switch (gpu_preferences.enable_dawn_backend_validation) {
    case DawnBackendValidationLevel::kDisabled:
      break;
    case DawnBackendValidationLevel::kPartial:
      instance->SetBackendValidationLevel(
          dawn::native::BackendValidationLevel::Partial);
      break;
    case DawnBackendValidationLevel::kFull:
      instance->SetBackendValidationLevel(
          dawn::native::BackendValidationLevel::Full);
      break;
  }

  return instance;
}

}  // namespace gpu::webgpu