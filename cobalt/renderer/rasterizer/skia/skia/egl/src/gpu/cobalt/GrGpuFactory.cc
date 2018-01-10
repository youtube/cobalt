// Copyright 2014 Google Inc. All Rights Reserved.
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

#if 0
// TODO: Delete this file.
// Skia's GrGpuFactory.cpp seems to behave as required.

#include "base/logging.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "third_party/skia/include/gpu/gl/GrGLConfig.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "third_party/skia/src/gpu/gl/GrGpuGL.h"
#include "third_party/skia/src/gpu/GrGpu.h"

GrGpu* GrGpu::Create(GrBackend backend, GrBackendContext backend_context,
                     GrContext* context) {
  DCHECK_EQ(kCobalt_GrBackend, backend);

  SkAutoTUnref<const GrGLInterface> gl_interface(GrGLCreateNativeInterface());
  DCHECK(gl_interface.get());

  GrGLContext ctx(gl_interface.get());
  if (ctx.isInitialized()) {
    return SkNEW_ARGS(GrGpuGL, (ctx, context));
  }

  return NULL;
}
#endif
