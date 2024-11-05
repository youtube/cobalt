// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "ui/ozone/platform/starboard/gl_surface_starboard.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/starboard/gl_ozone_egl_starboard.h"


namespace {
base::TimeDelta GetVSyncInterval() {
  return base::Seconds(2) / 59.94;
}

}  // namespace

namespace ui {

GLSurfaceStarboard::GLSurfaceStarboard(gl::GLDisplayEGL* display,
                             gfx::AcceleratedWidget widget,
                             GLOzoneEglStarboard* parent)
    : NativeViewGLSurfaceEGL(
          display,
          parent->GetNativeWindow(),
          std::make_unique<gfx::FixedVSyncProvider>(base::TimeTicks(),
                                                    GetVSyncInterval())),
      widget_(widget),
      parent_(parent),
      uses_triple_buffering_(false) {
  DCHECK(parent_);
}

bool GLSurfaceStarboard::Resize(const gfx::Size& size,
                           float scale_factor,
                           const gfx::ColorSpace& color_space,
                           bool has_alpha) {
  return parent_->ResizeDisplay(size) &&
         NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                        has_alpha);
}

EGLConfig GLSurfaceStarboard::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetEGLDisplay(), config_attribs);
  }
  return config_;
}

int GLSurfaceStarboard::GetBufferCount() const {
  return uses_triple_buffering_ ? 3 : 2;
}

GLSurfaceStarboard::~GLSurfaceStarboard() {
  Destroy();
}

}  // namespace ui
