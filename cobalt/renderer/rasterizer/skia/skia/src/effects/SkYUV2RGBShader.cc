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

#include "cobalt/renderer/rasterizer/skia/skia/src/effects/SkYUV2RGBShader.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkWriteBuffer.h"
#include "third_party/skia/src/core/SkReadBuffer.h"
#include "third_party/skia/src/gpu/effects/GrYUVEffect.h"
#include "third_party/skia/src/gpu/GrFragmentProcessor.h"
#include "third_party/skia/src/shaders/SkImageShader.h"

SkYUV2RGBShader::SkYUV2RGBShader(SkYUVColorSpace color_space,
                                 const sk_sp<SkImage>& y_image,
                                 const SkMatrix& y_matrix,
                                 const sk_sp<SkImage>& u_image,
                                 const SkMatrix& u_matrix,
                                 const sk_sp<SkImage>& v_image,
                                 const SkMatrix& v_matrix)
    : color_space_(color_space),
      y_image_(y_image),
      y_matrix_(y_matrix),
      u_image_(u_image),
      u_matrix_(u_matrix),
      v_image_(v_image),
      v_matrix_(v_matrix) {
  DCHECK(y_image_);
  DCHECK(u_image_);
  DCHECK(v_image_);
  InitializeShaders();
}

void SkYUV2RGBShader::InitializeShaders() {
  y_shader_.reset(new SkImageShader(y_image_, SkShader::kClamp_TileMode,
                                    SkShader::kClamp_TileMode, &y_matrix_));
  DCHECK(y_shader_);

  u_shader_.reset(new SkImageShader(u_image_, SkShader::kClamp_TileMode,
                                    SkShader::kClamp_TileMode, &u_matrix_));
  DCHECK(u_shader_);

  v_shader_.reset(new SkImageShader(v_image_, SkShader::kClamp_TileMode,
                                    SkShader::kClamp_TileMode, &v_matrix_));
  DCHECK(v_shader_);
}

sk_sp<SkFlattenable> SkYUV2RGBShader::CreateProc(SkReadBuffer& buffer) {
  SkYUVColorSpace color_space = static_cast<SkYUVColorSpace>(buffer.readInt());

  sk_sp<SkImage> y_image = buffer.readBitmapAsImage();
  if (!y_image) {
    return nullptr;
  }
  SkMatrix y_matrix;
  buffer.readMatrix(&y_matrix);

  sk_sp<SkImage> u_image = buffer.readBitmapAsImage();
  if (!u_image) {
    return nullptr;
  }
  SkMatrix u_matrix;
  buffer.readMatrix(&u_matrix);

  sk_sp<SkImage> v_image = buffer.readBitmapAsImage();
  if (!v_image) {
    return nullptr;
  }
  SkMatrix v_matrix;
  buffer.readMatrix(&v_matrix);

  return sk_sp<SkFlattenable>(new SkYUV2RGBShader(
      color_space, y_image, y_matrix, u_image, u_matrix, v_image, v_matrix));
}

void SkYUV2RGBShader::flatten(SkWriteBuffer& buffer) const {
  buffer.writeInt(color_space_);
  buffer.writeImage(y_image_.get());
  buffer.writeMatrix(y_matrix_);
  buffer.writeImage(u_image_.get());
  buffer.writeMatrix(u_matrix_);
  buffer.writeImage(v_image_.get());
  buffer.writeMatrix(v_matrix_);
}

uint32_t SkYUV2RGBShader::YUV2RGBShaderContext::getFlags() const {
  return 0;
}

SkShaderBase::Context* SkYUV2RGBShader::onMakeContext(
    const ContextRec& rec, SkArenaAlloc* storage) const {
  return nullptr;
}
SkYUV2RGBShader::YUV2RGBShaderContext::YUV2RGBShaderContext(
    SkYUVColorSpace color_space, const SkYUV2RGBShader& yuv2rgb_shader,
    SkShaderBase::Context* y_shader_context,
    SkShaderBase::Context* u_shader_context,
    SkShaderBase::Context* v_shader_context, const ContextRec& rec)
    : INHERITED(yuv2rgb_shader, rec),
      color_space_(color_space),
      y_shader_context_(y_shader_context),
      u_shader_context_(u_shader_context),
      v_shader_context_(v_shader_context) {}

SkYUV2RGBShader::YUV2RGBShaderContext::~YUV2RGBShaderContext() {
  y_shader_context_->~Context();
  u_shader_context_->~Context();
  v_shader_context_->~Context();
}
void SkYUV2RGBShader::YUV2RGBShaderContext::shadeSpan(
    int x, int y, SkPMColor result[], int count) {
  static const int kPixelCountPerSpan = 64;
  SkPMColor y_values[kPixelCountPerSpan];
  SkPMColor u_values[kPixelCountPerSpan];
  SkPMColor v_values[kPixelCountPerSpan];

  DCHECK_EQ(kRec709_SkYUVColorSpace, color_space_) <<
      "Currently we only support the BT.709 YUV colorspace.";

  do {
    int count_in_chunk =
        count > kPixelCountPerSpan ? kPixelCountPerSpan : count;

    y_shader_context_->shadeSpan(x, y, y_values, count_in_chunk);
    u_shader_context_->shadeSpan(x, y, u_values, count_in_chunk);
    v_shader_context_->shadeSpan(x, y, v_values, count_in_chunk);

    for (int i = 0; i < count_in_chunk; ++i) {
      int32_t y_value = SkColorGetA(y_values[i]) - 16;
      int32_t u_value = SkColorGetA(u_values[i]) - 128;
      int32_t v_value = SkColorGetA(v_values[i]) - 128;

      const float kA = 1.164f;
      const float kB = -0.213f;
      const float kC = 2.112f;
      const float kD = 1.793f;
      const float kE = -0.533f;
      int32_t r_unclamped = static_cast<int32_t>(kA * y_value + kD * v_value);
      int32_t g_unclamped =
          static_cast<int32_t>(kA * y_value + kB * u_value + kE * v_value);
      int32_t b_unclamped = static_cast<int32_t>(kA * y_value + kC * u_value);

      int32_t r_clamped =
          std::min<int32_t>(255, std::max<int32_t>(0, r_unclamped));
      int32_t g_clamped =
          std::min<int32_t>(255, std::max<int32_t>(0, g_unclamped));
      int32_t b_clamped =
          std::min<int32_t>(255, std::max<int32_t>(0, b_unclamped));

      result[i] = SkPackARGB32NoCheck(255, r_clamped, g_clamped, b_clamped);
    }
    result += count_in_chunk;
    x += count_in_chunk;
    count -= count_in_chunk;
  } while (count > 0);
}

#ifndef SK_IGNORE_TO_STRING
void SkYUV2RGBShader::toString(SkString* string_argument) const {
  string_argument->append("SkYUV2RGBShader: (");

  string_argument->append("Y Shader: ");
  y_shader_->toString(string_argument);
  string_argument->append("U Shader: ");
  u_shader_->toString(string_argument);
  string_argument->append("V Shader: ");
  v_shader_->toString(string_argument);

  INHERITED::toString(string_argument);

  string_argument->append(")");
}
#endif  // SK_IGNORE_TO_STRING
#if SK_SUPPORT_GPU

sk_sp<GrFragmentProcessor> SkYUV2RGBShader::asFragmentProcessor(
    const AsFPArgs&) const {
  return nullptr;
}

#endif  // SK_SUPPORT_GPU
