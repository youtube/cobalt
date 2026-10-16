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

// Process-wide registry of Wasm video surfaces, keyed by unguessable tokens.
// Thread-safe.
//
// A host registers a token per surface. A `<video>` whose `src` is a `blob:`
// URL ending in `#sb-surface=<token>` binds its compositor geometry to that
// surface. Depends only on //base so content/renderer can use it.

#ifndef COBALT_STARBOARD_WASM_PLAYER_SURFACE_REGISTRY_H_
#define COBALT_STARBOARD_WASM_PLAYER_SURFACE_REGISTRY_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "base/synchronization/lock.h"

namespace cobalt {
namespace starboard_wasm {

struct PlaneRect {
  int32_t x = 0;
  int32_t y = 0;
  int32_t width = 0;
  int32_t height = 0;

  bool operator==(const PlaneRect& other) const {
    return x == other.x && y == other.y && width == other.width &&
           height == other.height;
  }
};

class SurfaceRegistryObserver {
 public:
  virtual void OnSurfaceBoundsChanged(const std::string& token) = 0;

 protected:
  virtual ~SurfaceRegistryObserver() = default;
};

class SurfaceRegistry {
 public:
  static SurfaceRegistry& Get();

  // Host side.
  void Register(const std::string& token);
  void Unregister(const std::string& token);
  // Returns true and fills `out` if an element is bound to `token`.
  bool Bounds(const std::string& token, PlaneRect* out) const;
  void SetVideoSize(const std::string& token, int32_t width, int32_t height);

  // Element side. Calls for unregistered tokens are ignored.
  void SetBounds(const std::string& token, const PlaneRect& rect);
  void ClearBounds(const std::string& token);
  bool VideoSize(const std::string& token,
                 int32_t* width,
                 int32_t* height) const;

  void AddObserver(SurfaceRegistryObserver* observer);
  void RemoveObserver(SurfaceRegistryObserver* observer);

 private:
  struct Entry {
    bool bound = false;
    PlaneRect rect;
    int32_t video_width = 0;
    int32_t video_height = 0;
  };

  friend class base::NoDestructor<SurfaceRegistry>;

  SurfaceRegistry() = default;

  void Notify(const std::string& token);

  mutable base::Lock mu_;
  // Held while notifying, so that once RemoveObserver() returns the observer
  // is never called again. Acquired before `mu_`.
  base::Lock notify_mu_;
  std::map<std::string, Entry> entries_;
  std::vector<SurfaceRegistryObserver*> observers_;
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_PLAYER_SURFACE_REGISTRY_H_
