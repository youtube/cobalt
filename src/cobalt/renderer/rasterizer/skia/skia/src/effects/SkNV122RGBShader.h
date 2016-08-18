/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKNV122RGBSHADER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKNV122RGBSHADER_H_

#include "SkShader.h"

class SkNV122RGBShader : public SkShader {
 public:
  SkNV122RGBShader(SkYUVColorSpace color_space, const SkBitmap& y_bitmap,
                   const SkMatrix& y_matrix, const SkBitmap& uv_bitmap,
                   const SkMatrix& uv_matrix);
  virtual ~SkNV122RGBShader();

  virtual size_t contextSize() const SK_OVERRIDE;

  class NV122RGBShaderContext : public SkShader::Context {
   public:
    // Takes ownership of shaderContext and calls its destructor.
    NV122RGBShaderContext(SkYUVColorSpace color_space,
                          const SkNV122RGBShader& yuv2rgb_shader,
                          SkShader::Context* y_shader_context,
                          SkShader::Context* uv_shader_context,
                          const ContextRec& rec);
    virtual ~NV122RGBShaderContext();

    virtual uint32_t getFlags() const SK_OVERRIDE;

    virtual void shadeSpan(int x, int y, SkPMColor[], int count) SK_OVERRIDE;
    virtual void shadeSpan16(int x, int y, uint16_t[], int count) SK_OVERRIDE;

    virtual void set3DMask(const SkMask* mask) SK_OVERRIDE {
      // forward to our proxy
      y_shader_context_->set3DMask(mask);
      uv_shader_context_->set3DMask(mask);
    }

   private:
    SkYUVColorSpace color_space_;

    SkShader::Context* y_shader_context_;
    SkShader::Context* uv_shader_context_;

    typedef SkShader::Context INHERITED;
  };

#if SK_SUPPORT_GPU
  bool asFragmentProcessor(GrContext*, const SkPaint&, const SkMatrix*,
                           GrColor*, GrFragmentProcessor**) const SK_OVERRIDE;
#endif

  SK_TO_STRING_OVERRIDE()
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(SkNV122RGBShader)

 protected:
#ifdef SK_SUPPORT_LEGACY_DEEPFLATTENING
  explicit SkNV122RGBShader(SkReadBuffer&);
#endif
  virtual void flatten(SkWriteBuffer&) const SK_OVERRIDE;
  virtual Context* onCreateContext(const ContextRec&,
                                   void* storage) const SK_OVERRIDE;

 private:
  void InitializeShaders();

  SkYUVColorSpace color_space_;

  SkBitmap y_bitmap_;
  SkMatrix y_matrix_;
  SkShader* y_shader_;

  SkBitmap uv_bitmap_;
  SkMatrix uv_matrix_;
  SkShader* uv_shader_;

  typedef SkShader INHERITED;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKNV122RGBSHADER_H_
