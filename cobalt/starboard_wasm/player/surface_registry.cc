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

#include "cobalt/starboard_wasm/player/surface_registry.h"

#include <algorithm>

namespace cobalt {
namespace starboard_wasm {

// static
SurfaceRegistry& SurfaceRegistry::Get() {
  static base::NoDestructor<SurfaceRegistry> instance;
  return *instance;
}

void SurfaceRegistry::Register(const std::string& token) {
  base::AutoLock lock(mu_);
  entries_.try_emplace(token);
}

void SurfaceRegistry::Unregister(const std::string& token) {
  base::AutoLock lock(mu_);
  entries_.erase(token);
}

bool SurfaceRegistry::Bounds(const std::string& token, PlaneRect* out) const {
  base::AutoLock lock(mu_);
  auto it = entries_.find(token);
  if (it == entries_.end() || !it->second.bound) {
    return false;
  }
  *out = it->second.rect;
  return true;
}

void SurfaceRegistry::SetVideoSize(const std::string& token,
                                   int32_t width,
                                   int32_t height) {
  base::AutoLock lock(mu_);
  auto it = entries_.find(token);
  if (it == entries_.end()) {
    return;
  }
  it->second.video_width = width;
  it->second.video_height = height;
}

void SurfaceRegistry::SetBounds(const std::string& token,
                                const PlaneRect& rect) {
  {
    base::AutoLock lock(mu_);
    auto it = entries_.find(token);
    if (it == entries_.end()) {
      return;
    }
    Entry& entry = it->second;
    if (entry.bound && entry.rect == rect) {
      return;
    }
    entry.bound = true;
    entry.rect = rect;
  }
  Notify(token);
}

void SurfaceRegistry::ClearBounds(const std::string& token) {
  {
    base::AutoLock lock(mu_);
    auto it = entries_.find(token);
    if (it == entries_.end() || !it->second.bound) {
      return;
    }
    it->second.bound = false;
  }
  Notify(token);
}

bool SurfaceRegistry::VideoSize(const std::string& token,
                                int32_t* width,
                                int32_t* height) const {
  base::AutoLock lock(mu_);
  auto it = entries_.find(token);
  if (it == entries_.end()) {
    return false;
  }
  *width = it->second.video_width;
  *height = it->second.video_height;
  return *width > 0 && *height > 0;
}

void SurfaceRegistry::AddObserver(SurfaceRegistryObserver* observer) {
  base::AutoLock lock(mu_);
  observers_.push_back(observer);
}

void SurfaceRegistry::RemoveObserver(SurfaceRegistryObserver* observer) {
  base::AutoLock notify_lock(notify_mu_);
  base::AutoLock lock(mu_);
  observers_.erase(std::remove(observers_.begin(), observers_.end(), observer),
                   observers_.end());
}

void SurfaceRegistry::Notify(const std::string& token) {
  base::AutoLock notify_lock(notify_mu_);
  std::vector<SurfaceRegistryObserver*> observers;
  {
    base::AutoLock lock(mu_);
    observers = observers_;
  }
  for (SurfaceRegistryObserver* observer : observers) {
    observer->OnSurfaceBoundsChanged(token);
  }
}

}  // namespace starboard_wasm
}  // namespace cobalt
