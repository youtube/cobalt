// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DCOMP_MOCKS_H_
#define UI_GL_DCOMP_MOCKS_H_

#include <windows.h>

#include <d2d1.h>
#include <dcomp.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <wrl/implements.h>

#include "testing/gmock/include/gmock/gmock.h"

namespace gl {

class IDCompositionTextureMock
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IDCompositionTexture> {
 public:
  IDCompositionTextureMock();
  ~IDCompositionTextureMock() override;

  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetSourceRect,
                             HRESULT(const D2D_RECT_U&));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetColorSpace,
                             HRESULT(DXGI_COLOR_SPACE_TYPE));
  MOCK_METHOD1_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             SetAlphaMode,
                             HRESULT(DXGI_ALPHA_MODE));
  MOCK_METHOD3_WITH_CALLTYPE(STDMETHODCALLTYPE,
                             GetAvailableFence,
                             HRESULT(UINT64*, REFIID, void**));
};

}  // namespace gl

#endif  // UI_GL_DCOMP_MOCKS_H_
