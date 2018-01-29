// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_

#include "third_party/skia/src/shaders/SkShaderBase.h"

#include "third_party/skia/src/shaders/SkImageShader.h"

class SkYUV2RGBShader : public SkShaderBase {
 public:
  SkYUV2RGBShader(SkYUVColorSpace color_space, const sk_sp<SkImage>& y_image,
                  const SkMatrix& y_matrix, const sk_sp<SkImage>& u_image,
                  const SkMatrix& u_matrix, const sk_sp<SkImage>& v_image,
                  const SkMatrix& v_matrix);
  virtual ~SkYUV2RGBShader() = default;

  class YUV2RGBShaderContext : public SkShaderBase::Context {
   public:
    // Takes ownership of shaderContext and calls its destructor.
    YUV2RGBShaderContext(SkYUVColorSpace color_space,
                         const SkYUV2RGBShader& yuv2rgb_shader,
                         SkShaderBase::Context* y_shader_context,
                         SkShaderBase::Context* u_shader_context,
                         SkShaderBase::Context* v_shader_context,
                         const ContextRec& rec);
    virtual ~YUV2RGBShaderContext();

    uint32_t getFlags() const override;

    void shadeSpan(int x, int y, SkPMColor[], int count) override;

    void set3DMask(const SkMask* mask) override {
        // forward to our proxy
        y_shader_context_->set3DMask(mask);
        u_shader_context_->set3DMask(mask);
        v_shader_context_->set3DMask(mask);
    }

   private:
    SkYUVColorSpace color_space_;

    SkShaderBase::Context* y_shader_context_;
    SkShaderBase::Context* u_shader_context_;
    SkShaderBase::Context* v_shader_context_;

    typedef SkShaderBase::Context INHERITED;
  };

#if SK_SUPPORT_GPU
  sk_sp<GrFragmentProcessor> asFragmentProcessor(const AsFPArgs&) const
      override;
#endif

  SK_TO_STRING_OVERRIDE()
  SK_DECLARE_PUBLIC_FLATTENABLE_DESERIALIZATION_PROCS(SkYUV2RGBShader)

 protected:
  void flatten(SkWriteBuffer&) const override;
  SkShaderBase::Context* onMakeContext(const ContextRec& rec,
                                       SkArenaAlloc* storage) const override;

 private:
  void InitializeShaders();

  SkYUVColorSpace color_space_;

  sk_sp<SkImage> y_image_;
  SkMatrix y_matrix_;
  sk_sp<SkImageShader> y_shader_;

  sk_sp<SkImage> u_image_;
  SkMatrix u_matrix_;
  sk_sp<SkImageShader> u_shader_;

  sk_sp<SkImage> v_image_;
  SkMatrix v_matrix_;
  sk_sp<SkImageShader> v_shader_;

  typedef SkShaderBase INHERITED;
};

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SKIA_SRC_EFFECTS_SKYUV2RGBSHADER_H_
