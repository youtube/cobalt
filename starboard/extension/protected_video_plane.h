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

#ifndef STARBOARD_EXTENSION_PROTECTED_VIDEO_PLANE_H_
#define STARBOARD_EXTENSION_PROTECTED_VIDEO_PLANE_H_

#include <stdbool.h>
#include <stdint.h>

#include "starboard/decode_target.h"

#ifdef __cplusplus
extern "C" {
#endif

// Lets the embedder render decode-to-texture frames from secure decoders,
// whose output stays in protected memory, with a protected GL context and
// present the result on the punch-out video plane.
//
// The platform has a single punch-out video plane, so there is at most one
// protected presenter at a time.

#define kStarboardExtensionProtectedVideoPlaneName \
  "dev.starboard.extension.ProtectedVideoPlane"

// Texture hooks through which decode targets of one graphics context provider
// are created in the embedder's GL implementation (ANGLE) rather than the
// system GL. They run on the provider's GLES context runner with the
// embedder's protected context current, and receive |context| as their first
// argument.
typedef struct StarboardProtectedDecodeTargetTextureHooks {
  void* context;

  // Creates a GL_TEXTURE_EXTERNAL_OES texture. |out_texture| is the embedder's
  // name (stored in SbDecodeTargetInfo); |out_native_texture| is the driver's
  // name, given to SurfaceTexture.
  bool (*create_texture)(void* context,
                         uint32_t* out_texture,
                         uint32_t* out_native_texture);

  // Deletes a texture created by |create_texture|. Textures still alive when
  // the hooks are removed or replaced are the embedder's to delete; they are
  // not passed back afterwards.
  void (*delete_texture)(void* context, uint32_t texture);

  // Called after SurfaceTexture.updateTexImage() changed the native binding
  // behind the embedder's back. |width| x |height| is the coded size of the
  // new image, which the embedder's GL cannot learn from the driver.
  void (*after_update_tex_image)(void* context,
                                 uint32_t texture,
                                 int width,
                                 int height);
} StarboardProtectedDecodeTargetTextureHooks;

typedef struct StarboardExtensionProtectedVideoPlaneApi {
  // Name should be the string |kStarboardExtensionProtectedVideoPlaneName|.
  const char* name;

  // Version of the API (1).
  uint32_t version;

  // Process-wide capability: when true, the embedder can render decode
  // targets of secure decoders, so players that require a secure decoder may
  // use kSbPlayerOutputModeDecodeToTexture, and
  // SbMediaCanPlayMimeAndKeySystem() may support `decode-to-texture=protected`.
  // The embedder sets it once its GPU is initialized, from the GPU's actual
  // protected-content support; it is false until then.
  void (*SetProtectedDecodeToTextureEnabled)(bool enabled);

  // Returns the ANativeWindow* of the punch-out video plane with a reference
  // added, and blocks Starboard's own users of that window until released.
  // Returns NULL if there is no window or another holder has it.
  void* (*AcquireVideoWindowForPresenter)(void);

  // Releases a window returned by AcquireVideoWindowForPresenter().
  void (*ReleaseVideoWindowForPresenter)(void* window);

  // Called after the embedder created its protected EGL window surface on
  // |window| (from AcquireVideoWindowForPresenter()) and before it draws the
  // first frame, so the platform can adjust how the window's buffers are
  // allocated. For example, MediaTek marks secure video buffers with a vendor
  // usage bit that its composer checks.
  void (*PrepareVideoWindowSurface)(void* window);

  // Installs |hooks| (copied) for decode targets that secure decoders create
  // with |provider|, or removes them when |hooks| is NULL. Decode targets of
  // other providers, and of clear decoders, keep the system-GL texture. After
  // removal returns, no hook of the removed set runs.
  void (*SetDecodeTargetTextureHooks)(
      SbDecodeTargetGraphicsContextProvider* provider,
      const StarboardProtectedDecodeTargetTextureHooks* hooks);
} StarboardExtensionProtectedVideoPlaneApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_PROTECTED_VIDEO_PLANE_H_
