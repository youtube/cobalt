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

#include "cobalt/renderer/rasterizer/skia/skia/src/effects/SkNV122RGBShader.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkWriteBuffer.h"
#include "third_party/skia/src/core/SkReadBuffer.h"
#include "third_party/skia/src/gpu/effects/GrYUVtoRGBEffect.h"

SkNV122RGBShader::SkNV122RGBShader(SkYUVColorSpace color_space,
                                   const SkBitmap& y_bitmap,
                                   const SkMatrix& y_matrix,
                                   const SkBitmap& uv_bitmap,
                                   const SkMatrix& uv_matrix)
    : color_space_(color_space),
      y_bitmap_(y_bitmap),
      y_matrix_(y_matrix),
      uv_bitmap_(uv_bitmap),
      uv_matrix_(uv_matrix) {
  DCHECK(!y_bitmap_.isNull());
  DCHECK(!uv_bitmap_.isNull());
  InitializeShaders();
}

void SkNV122RGBShader::InitializeShaders() {
  y_shader_ =
      SkShader::CreateBitmapShader(y_bitmap_, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &y_matrix_);
  DCHECK(y_shader_);

  uv_shader_ =
      SkShader::CreateBitmapShader(uv_bitmap_, SkShader::kClamp_TileMode,
                                   SkShader::kClamp_TileMode, &uv_matrix_);
  DCHECK(uv_shader_);
}

#ifdef SK_SUPPORT_LEGACY_DEEPFLATTENING
SkNV122RGBShader::SkNV122RGBShader(SkReadBuffer& buffer) : INHERITED(buffer) {
  color_space_ = static_cast<SkYUVColorSpace>(buffer.readInt());
  buffer.readBitmap(&y_bitmap_);
  buffer.readMatrix(&y_matrix_);
  buffer.readBitmap(&uv_bitmap_);
  buffer.readMatrix(&uv_matrix_);
  InitializeShaders();
}
#endif

SkNV122RGBShader::~SkNV122RGBShader() {
  y_shader_->unref();
  uv_shader_->unref();
}

SkFlattenable* SkNV122RGBShader::CreateProc(SkReadBuffer& buffer) {
  SkYUVColorSpace color_space = static_cast<SkYUVColorSpace>(buffer.readInt());

  SkBitmap y_bitmap;
  if (!buffer.readBitmap(&y_bitmap)) {
    return NULL;
  }
  y_bitmap.setImmutable();
  SkMatrix y_matrix;
  buffer.readMatrix(&y_matrix);

  SkBitmap uv_bitmap;
  if (!buffer.readBitmap(&uv_bitmap)) {
    return NULL;
  }
  uv_bitmap.setImmutable();
  SkMatrix uv_matrix;
  buffer.readMatrix(&uv_matrix);

  return SkNEW_ARGS(SkNV122RGBShader,
                    (color_space, y_bitmap, y_matrix, uv_bitmap, uv_matrix));
}

void SkNV122RGBShader::flatten(SkWriteBuffer& buffer) const {
  buffer.writeInt(color_space_);
  buffer.writeBitmap(y_bitmap_);
  buffer.writeMatrix(y_matrix_);
  buffer.writeBitmap(uv_bitmap_);
  buffer.writeMatrix(uv_matrix_);
}

uint32_t SkNV122RGBShader::NV122RGBShaderContext::getFlags() const { return 0; }

SkShader::Context* SkNV122RGBShader::onCreateContext(const ContextRec& rec,
                                                     void* storage) const {
  char* shaderContextStorage =
      static_cast<char*>(storage) + sizeof(NV122RGBShaderContext);
  SkShader::Context* y_shader_context =
      y_shader_->createContext(rec, shaderContextStorage);
  DCHECK(y_shader_context);
  shaderContextStorage += y_shader_->contextSize();
  SkShader::Context* uv_shader_context =
      uv_shader_->createContext(rec, shaderContextStorage);
  DCHECK(uv_shader_context);

  return SkNEW_PLACEMENT_ARGS(
      storage, NV122RGBShaderContext,
      (color_space_, *this, y_shader_context, uv_shader_context, rec));
}

size_t SkNV122RGBShader::contextSize() const {
  return sizeof(NV122RGBShaderContext) + y_shader_->contextSize() +
         uv_shader_->contextSize();
}

SkNV122RGBShader::NV122RGBShaderContext::NV122RGBShaderContext(
    SkYUVColorSpace color_space, const SkNV122RGBShader& yuv2rgb_shader,
    SkShader::Context* y_shader_context, SkShader::Context* uv_shader_context,
    const ContextRec& rec)
    : INHERITED(yuv2rgb_shader, rec),
      color_space_(color_space),
      y_shader_context_(y_shader_context),
      uv_shader_context_(uv_shader_context) {}

SkNV122RGBShader::NV122RGBShaderContext::~NV122RGBShaderContext() {
  y_shader_context_->~Context();
  uv_shader_context_->~Context();
}

void SkNV122RGBShader::NV122RGBShaderContext::shadeSpan(int x, int y,
                                                        SkPMColor result[],
                                                        int count) {
  static const int kPixelCountPerSpan = 64;
  SkPMColor y_values[kPixelCountPerSpan];
  SkPMColor uv_values[kPixelCountPerSpan];

  DCHECK_EQ(kRec709_SkYUVColorSpace, color_space_)
      << "Currently we only support the BT.709 YUV colorspace.";

  do {
    int count_in_chunk =
        count > kPixelCountPerSpan ? kPixelCountPerSpan : count;

    y_shader_context_->shadeSpan(x, y, y_values, count_in_chunk);
    uv_shader_context_->shadeSpan(x, y, uv_values, count_in_chunk);

    for (int i = 0; i < count_in_chunk; ++i) {
      int32_t y_value = SkColorGetA(y_values[i]) - 16;
      int32_t u_value = SkColorGetB(uv_values[i]) - 128;
      int32_t v_value = SkColorGetA(uv_values[i]) - 128;

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

void SkNV122RGBShader::NV122RGBShaderContext::shadeSpan16(int x, int y,
                                                          uint16_t result[],
                                                          int count) {
  NOTREACHED();
}

#ifndef SK_IGNORE_TO_STRING
void SkNV122RGBShader::toString(SkString* str) const {
  str->append("SkNV122RGBShader: (");

  str->append("Y Shader: ");
  y_shader_->toString(str);
  str->append("UV Shader: ");
  uv_shader_->toString(str);

  this->INHERITED::toString(str);

  str->append(")");
}
#endif  // SK_IGNORE_TO_STRING

#if SK_SUPPORT_GPU

bool SkNV122RGBShader::asFragmentProcessor(GrContext*, const SkPaint& paint,
                                           const SkMatrix* localMatrix,
                                           GrColor*,
                                           GrFragmentProcessor** fp) const {
  // Code snippet taken from SkBitmapProcShader::asFragmentProcessor().
  SkMatrix matrix;
  matrix.setIDiv(y_bitmap_.width(), y_bitmap_.height());

  SkMatrix lmInverse;
  if (!y_shader_->getLocalMatrix().invert(&lmInverse)) {
    return false;
  }
  if (localMatrix) {
    SkMatrix inv;
    if (!localMatrix->invert(&inv)) {
      return false;
    }
    lmInverse.postConcat(inv);
  }
  matrix.preConcat(lmInverse);

  GrTextureParams::FilterMode filter_mode =
      paint.getFilterLevel() == SkPaint::kNone_FilterLevel
          ? GrTextureParams::kNone_FilterMode
          : GrTextureParams::kBilerp_FilterMode;
  GrTextureParams texture_params;
  texture_params.setFilterMode(filter_mode);

  *fp = GrYUVtoRGBEffect::Create(y_bitmap_.getTexture(),
                                 uv_bitmap_.getTexture(), NULL, matrix,
                                 texture_params, kRec709_SkYUVColorSpace, true);
  return true;
}

#endif  // SK_SUPPORT_GPU
