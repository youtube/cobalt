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

#ifndef COBALT_STARBOARD_WASM_SURFACE_VIDEO_SIZE_H_
#define COBALT_STARBOARD_WASM_SURFACE_VIDEO_SIZE_H_

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/unguessable_token.h"
#include "ui/gfx/geometry/size.h"

namespace cobalt {
namespace starboard_wasm {

// The size of the video on each Wasm surface, from its players' stream info.
// The surface's <video> element reports it as its natural size, so the
// element's object-fit places the hole. Process-wide and thread-safe: the
// player host and the element can be on different threads.
class SurfaceVideoSize {
 public:
  using Callback = base::RepeatingCallback<void(const gfx::Size&)>;

  SurfaceVideoSize() = delete;

  // An empty `size` forgets the surface without notifying.
  static void Set(const base::UnguessableToken& surface, const gfx::Size& size);

  // Posts `callback` to the calling sequence with the current size, if any,
  // and with each change, until the returned runner is destroyed. A later
  // Watch() of the same surface replaces this one.
  [[nodiscard]] static base::ScopedClosureRunner Watch(
      const base::UnguessableToken& surface,
      Callback callback);
};

}  // namespace starboard_wasm
}  // namespace cobalt

#endif  // COBALT_STARBOARD_WASM_SURFACE_VIDEO_SIZE_H_
