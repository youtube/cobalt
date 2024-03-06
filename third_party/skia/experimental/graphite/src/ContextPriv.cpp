/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "experimental/graphite/src/ContextPriv.h"

#include "experimental/graphite/src/GlobalCache.h"
#include "experimental/graphite/src/Gpu.h"

namespace skgpu {

Gpu* ContextPriv::gpu() {
    return fContext->fGpu.get();
}

const Gpu* ContextPriv::gpu() const {
    return fContext->fGpu.get();
}

SkShaderCodeDictionary* ContextPriv::shaderCodeDictionary() {
    return fContext->fGlobalCache->shaderCodeDictionary();
}

} // namespace skgpu
