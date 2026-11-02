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

#ifndef STARBOARD_ANDROID_SHARED_PROTECTED_VIDEO_PLANE_H_
#define STARBOARD_ANDROID_SHARED_PROTECTED_VIDEO_PLANE_H_

#include "starboard/extension/protected_video_plane.h"

namespace starboard {

const void* GetProtectedVideoPlaneApi();

// Whether secure-decoder players may use decode-to-texture.
bool IsProtectedDecodeToTextureEnabled();

// Decode-target textures created through the embedder's hooks for
// |provider|. Each installation of hooks gets a new nonzero id; a texture is
// only passed back to the installation that created it, so a texture that
// outlives its installation (or a reused provider address) never reaches an
// unrelated embedder.
//
// GetProtectedDecodeTargetTextureHooksId() returns 0 when no hooks are
// installed for |provider|. CreateProtectedDecodeTargetTexture() returns false
// when |hooks_id| is no longer installed or the embedder cannot create the
// texture.
uint64_t GetProtectedDecodeTargetTextureHooksId(
    SbDecodeTargetGraphicsContextProvider* provider);
bool CreateProtectedDecodeTargetTexture(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t* out_texture,
    uint32_t* out_native_texture);
void DeleteProtectedDecodeTargetTexture(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t texture);
void NotifyProtectedDecodeTargetUpdated(
    SbDecodeTargetGraphicsContextProvider* provider,
    uint64_t hooks_id,
    uint32_t texture,
    int width,
    int height);

// Implemented in video_window.cc.
void* AcquireVideoWindowForPresenter();
void ReleaseVideoWindowForPresenter(void* window);
bool IsVideoWindowHeldByPresenter();
void PrepareVideoWindowSurface(void* window);

// Implemented in player_set_bounds.cc. While a protected presenter holds the
// video window the SurfaceView stays full screen (the presenter draws the hole
// itself; a non-fullscreen protected GPU layer is blanked by some HWCs).
void SetVideoSurfaceFullscreen();
void ReapplyVideoSurfaceBounds();

// Implemented in video_window.cc. Observers learn when the protected presenter
// releases the video window, e.g. so that protected decode-to-texture decoders
// can fall back to punch-out. Notified on the releasing thread; removal waits
// for a running notification.
class ProtectedPresenterObserver {
 public:
  virtual void OnProtectedPresenterReleased() = 0;

 protected:
  ~ProtectedPresenterObserver() = default;
};
void AddProtectedPresenterObserver(ProtectedPresenterObserver* observer);
void RemoveProtectedPresenterObserver(ProtectedPresenterObserver* observer);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_PROTECTED_VIDEO_PLANE_H_
