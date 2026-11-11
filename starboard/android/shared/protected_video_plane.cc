// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/android/shared/protected_video_plane.h"

#include <atomic>
#include <map>
#include <mutex>

#include "starboard/common/log.h"
#include "starboard/common/no_destructor.h"
#include "starboard/shared/starboard/media/mime_supportability_cache.h"

namespace starboard {

namespace {

std::atomic<bool> g_protected_dtt_enabled{false};

// Hooks per graphics context provider. Hooks are called with the mutex held,
// so that SetDecodeTargetTextureHooks(provider, nullptr) waits for running
// hooks and the embedder can free |context| right after it returns.
struct HookRegistry {
  struct Entry {
    StarboardProtectedDecodeTargetTextureHooks hooks;
    uint64_t id;
  };

  // The installation |hooks_id| of |provider|, or nullptr.
  const StarboardProtectedDecodeTargetTextureHooks* Find(
      SbDecodeTargetGraphicsContextProvider* provider,
      uint64_t hooks_id) {
    auto it = entries.find(provider);
    if (it == entries.end() || it->second.id != hooks_id) {
      return nullptr;
    }
    return &it->second.hooks;
  }

  std::mutex mutex;
  std::map<SbDecodeTargetGraphicsContextProvider*, Entry> entries;
  uint64_t last_id = 0;
};

HookRegistry& GetHookRegistry() {
  static NoDestructor<HookRegistry> registry;
  return *registry;
}

void SetProtectedDecodeToTextureEnabled(bool enabled) {
  SB_LOG(INFO) << "Protected decode-to-texture "
               << (enabled ? "enabled" : "disabled");
  if (g_protected_dtt_enabled.exchange(enabled) != enabled) {
    // Answers to `decode-to-texture=protected` depend on this.
    MimeSupportabilityCache::GetInstance()->ClearCachedMimeSupportabilities();
  }
}

void SetDecodeTargetTextureHooks(
    SbDecodeTargetGraphicsContextProvider* provider,
    const StarboardProtectedDecodeTargetTextureHooks* hooks) {
  SB_CHECK(provider);
  HookRegistry& registry = GetHookRegistry();
  std::lock_guard lock(registry.mutex);
  if (hooks) {
    registry.entries[provider] = {*hooks, ++registry.last_id};
  } else {
    registry.entries.erase(provider);
  }
}

const StarboardExtensionProtectedVideoPlaneApi kProtectedVideoPlaneApi = {
    kStarboardExtensionProtectedVideoPlaneName,
    1,
    &SetProtectedDecodeToTextureEnabled,
    &AcquireVideoWindowForPresenter,
    &ReleaseVideoWindowForPresenter,
    &PrepareVideoWindowSurface,
    &SetDecodeTargetTextureHooks,
};

}  // namespace

const void* GetProtectedVideoPlaneApi() {
  return &kProtectedVideoPlaneApi;
}

bool IsProtectedDecodeToTextureEnabled() {
  return g_protected_dtt_enabled.load();
}

uint64_t GetProtectedDecodeTargetTextureHooksId(
    SbDecodeTargetGraphicsContextProvider* provider) {
  HookRegistry& registry = GetHookRegistry();
  std::lock_guard lock(registry.mutex);
  auto it = registry.entries.find(provider);
  return it == registry.entries.end() ? 0 : it->second.id;
}

bool CreateProtectedDecodeTargetTexture(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t* out_texture,
    uint32_t* out_native_texture) {
  HookRegistry& registry = GetHookRegistry();
  std::lock_guard lock(registry.mutex);
  const auto* hooks = registry.Find(provider, hooks_id);
  return hooks &&
         hooks->create_texture(hooks->context, out_texture, out_native_texture);
}

void DeleteProtectedDecodeTargetTexture(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t texture) {
  HookRegistry& registry = GetHookRegistry();
  std::lock_guard lock(registry.mutex);
  // Without its installation, the embedder has already deleted the texture
  // along with the others of that installation.
  if (const auto* hooks = registry.Find(provider, hooks_id)) {
    hooks->delete_texture(hooks->context, texture);
  }
}

void NotifyProtectedDecodeTargetUpdated(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t texture,
    int width,
    int height) {
  HookRegistry& registry = GetHookRegistry();
  std::lock_guard lock(registry.mutex);
  if (const auto* hooks = registry.Find(provider, hooks_id)) {
    hooks->after_update_tex_image(hooks->context, texture, width, height);
  }
}

}  // namespace starboard
