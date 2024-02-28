/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/gpu/gl/GrGLInterface.h"
#include "include/gpu/gl/glx/GrGLMakeGLXInterface.h"

sk_sp<const GrGLInterface> GrGLMakeNativeInterface() { return GrGLMakeGLXInterface(); }

const GrGLInterface* GrGLCreateNativeInterface() { return GrGLMakeNativeInterface().release(); }
