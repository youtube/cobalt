// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_SURFACE_SOLID_COLOR_POOL_H_
#define UI_GL_DC_SURFACE_SOLID_COLOR_POOL_H_

#include <windows.h>

#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include <memory>

#include "base/types/expected.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gl/dc_commit_error.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/solid_color_pool_base.h"

namespace gl {

// `SolidColorPoolBase` implementation that produces recolorable
// `IDCompositionSurface` entries backed by an `ID3D11Device`.
class GL_EXPORT DCSurfaceSolidColorPool final : public SolidColorPoolBase {
 public:
  DCSurfaceSolidColorPool(
      Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
      Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device);
  ~DCSurfaceSolidColorPool() override;

  DCSurfaceSolidColorPool(const DCSurfaceSolidColorPool&) = delete;
  DCSurfaceSolidColorPool& operator=(const DCSurfaceSolidColorPool&) = delete;

 protected:
  // gl::SolidColorPoolBase:
  base::expected<void, CommitError> FillEntry(std::unique_ptr<Entry>& entry,
                                              const SkColor4f& color) override;

 private:
  // One pooled entry: a recolorable `IDCompositionSurface`.
  class Surface final : public Entry {
   public:
    Surface();
    ~Surface() override;

    IUnknown* GetContent() const override;

    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
  };

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice3> dcomp_device_;
};

GL_EXPORT SolidColorPoolFactory CreateDCSurfaceSolidColorPoolFactory(
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device);

}  // namespace gl

#endif  // UI_GL_DC_SURFACE_SOLID_COLOR_POOL_H_
