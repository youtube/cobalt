// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/textured_element.h"

#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/skia_surface_provider.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

TexturedElement::TexturedElement() = default;

TexturedElement::~TexturedElement() = default;

void TexturedElement::Initialize(SkiaSurfaceProvider* provider) {
  TRACE_EVENT0("gpu", "TexturedElement::Initialize");
  DCHECK(provider);
  provider_ = provider;
  DCHECK(GetTexture());
  GetTexture()->OnInitialized();
  initialized_ = true;
}

bool TexturedElement::PrepareToDraw() {
  if (!GetTexture()->dirty() || !IsVisible())
    return false;

  texture_size_ = MeasureTextureSize();
  if (TextureDependsOnMeasurement())
    DCHECK(GetTexture()->measured());

  return true;
}

bool TexturedElement::HasDirtyTexture() const {
  DCHECK(IsVisible());  // We shouldn't be querying non visible elements.
  if (!GetTexture()->dirty())
    return false;
  // Elements can be dirtied by user input.  If the texture draw depends on
  // measurement, defer until the next frame.
  return !TextureDependsOnMeasurement() || GetTexture()->measured();
}

void TexturedElement::UpdateTexture() {
  if (!HasDirtyTexture())
    return;

  // If GL isn't ready yet, or we're in unit tests, pretend we drew a texture to
  // accurate articulate that we would have.  When GL is initialized, all
  // textures are dirtied again.
  if (!initialized_) {
    GetTexture()->DrawEmptyTexture();
    return;
  }

  if (!texture_size_.IsEmpty()) {
    surface_ = provider_->MakeSurface(texture_size_);
    DCHECK(surface_.get());
    GetTexture()->DrawTexture(surface_->getCanvas(), texture_size_);
    texture_handle_ = provider_->FlushSurface(surface_.get(), texture_handle_);
  } else {
    surface_.reset();
    GetTexture()->DrawEmptyTexture();
    texture_handle_ = 0;
  }
}

void TexturedElement::SetForegroundColor(SkColor color) {
  GetTexture()->SetForegroundColor(color);
}

void TexturedElement::SetBackgroundColor(SkColor color) {
  GetTexture()->SetBackgroundColor(color);
}

void TexturedElement::Render(UiElementRenderer* renderer,
                             const CameraModel& model) const {
  DCHECK(initialized_);

  // Zero-size elements, such as empty text, don't allocate textures.
  if (texture_handle_ <= 0)
    return;

  renderer->DrawTexturedQuad(texture_handle_, 0, kGlTextureLocationLocal,
                             model.view_proj_matrix * world_space_transform(),
                             GetClipRect(), computed_opacity(), size(),
                             corner_radius(), true /* blend */);
}

}  // namespace vr
