// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  LOG(INFO) << "GLSurfaceStarboard::GLSurfaceStarboard";
  DCHECK(parent_);
}

bool GLSurfaceStarboard::Resize(const gfx::Size& size,
                           float scale_factor,
                           const gfx::ColorSpace& color_space,
                           bool has_alpha) {
  LOG(INFO) << "GLSurfaceStarboard::Resize";
  return parent_->ResizeDisplay(size) &&
         NativeViewGLSurfaceEGL::Resize(size, scale_factor, color_space,
                                        has_alpha);
}

EGLConfig GLSurfaceStarboard::GetConfig() {
  LOG(INFO) << "GLSurfaceStarboard::GetConfig";
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
  LOG(INFO) << "GLSurfaceStarboard::GetBufferCount";
  return uses_triple_buffering_ ? 3 : 2;
}

GLSurfaceStarboard::~GLSurfaceStarboard() {
  LOG(INFO) << "GLSurfaceStarboard::~GLSurfaceStarboard";
  Destroy();
}

}  // namespace ui
