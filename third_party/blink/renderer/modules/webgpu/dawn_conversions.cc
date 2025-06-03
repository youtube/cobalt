// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"

#include <dawn/webgpu.h>

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_color_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_copy_texture.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_data_layout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_index_format.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_programmable_stage.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_doublesequence_gpucolordict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuautolayoutmode_gpupipelinelayout.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuextent3ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuorigin2ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_union_gpuorigin3ddict_unsignedlongenforcerangesequence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_pipeline_layout.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

bool ConvertToDawn(const V8GPUColor* in,
                   WGPUColor* out,
                   ExceptionState& exception_state) {
  switch (in->GetContentType()) {
    case V8GPUColor::ContentType::kGPUColorDict: {
      const GPUColorDict* dict = in->GetAsGPUColorDict();
      *out = {dict->r(), dict->g(), dict->b(), dict->a()};
      return true;
    }

    case V8GPUColor::ContentType::kDoubleSequence: {
      const Vector<double>& sequence = in->GetAsDoubleSequence();
      if (sequence.size() != 4) {
        exception_state.ThrowTypeError(
            "A sequence of number used as a GPUColor must have exactly 4 "
            "elements.");
        return false;
      }
      *out = {sequence[0], sequence[1], sequence[2], sequence[3]};
      return true;
    }
  }
}

bool ConvertToDawn(const V8GPUExtent3D* in,
                   WGPUExtent3D* out,
                   GPUDevice* device,
                   ExceptionState& exception_state) {
  switch (in->GetContentType()) {
    case V8GPUExtent3D::ContentType::kGPUExtent3DDict: {
      const GPUExtent3DDict* dict = in->GetAsGPUExtent3DDict();
      *out = {dict->width(), dict->height(), dict->depthOrArrayLayers()};
      if (dict->hasDepth()) {
        device->AddSingletonWarning(GPUSingletonWarning::kDepthKey);
      }
      return true;
    }

    case V8GPUExtent3D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& sequence =
          in->GetAsUnsignedLongEnforceRangeSequence();
      // The WebGPU spec states that height and depthOrArrayLayers default to 1
      // when the sequence isn't big enough.
      switch (sequence.size()) {
        case 1:
          *out = {sequence[0], 1, 1};
          return true;
        case 2:
          *out = {sequence[0], sequence[1], 1};
          return true;
        case 3:
          *out = {sequence[0], sequence[1], sequence[2]};
          return true;
        default:
          exception_state.ThrowTypeError(
              "A sequence of number used as a GPUExtent3D must have between 1 "
              "and 3 elements.");
          return false;
      }
    }
  }
}

bool ConvertToDawn(const V8GPUOrigin3D* in,
                   WGPUOrigin3D* out,
                   ExceptionState& exception_state) {
  switch (in->GetContentType()) {
    case V8GPUOrigin3D::ContentType::kGPUOrigin3DDict: {
      const GPUOrigin3DDict* dict = in->GetAsGPUOrigin3DDict();
      *out = {dict->x(), dict->y(), dict->z()};
      return true;
    }

    case V8GPUOrigin3D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& sequence =
          in->GetAsUnsignedLongEnforceRangeSequence();
      // The WebGPU spec states that coordinates default to 0 when the sequence
      // isn't big enough.
      switch (sequence.size()) {
        case 0:
          *out = {0, 0, 0};
          return true;
        case 1:
          *out = {sequence[0], 0, 0};
          return true;
        case 2:
          *out = {sequence[0], sequence[1], 0};
          return true;
        case 3:
          *out = {sequence[0], sequence[1], sequence[2]};
          return true;
        default:
          exception_state.ThrowTypeError(
              "A sequence of number used as a GPUOrigin3D must have at most 3 "
              "elements.");
          return false;
      }
    }
  }
}

bool ConvertToDawn(const V8GPUOrigin2D* in,
                   WGPUOrigin2D* out,
                   ExceptionState& exception_state) {
  switch (in->GetContentType()) {
    case V8GPUOrigin2D::ContentType::kGPUOrigin2DDict: {
      const GPUOrigin2DDict* dict = in->GetAsGPUOrigin2DDict();
      *out = {dict->x(), dict->y()};
      return true;
    }

    case V8GPUOrigin2D::ContentType::kUnsignedLongEnforceRangeSequence: {
      const Vector<uint32_t>& sequence =
          in->GetAsUnsignedLongEnforceRangeSequence();
      // The WebGPU spec states that coordinates default to 0 when the sequence
      // isn't big enough.
      switch (sequence.size()) {
        case 0:
          *out = {0, 0};
          return true;
        case 1:
          *out = {sequence[0], 0};
          return true;
        case 2:
          *out = {sequence[0], sequence[1]};
          return true;
        default:
          exception_state.ThrowTypeError(
              "A sequence of number used as a GPUOrigin2D must have at most 2 "
              "elements.");
          return false;
      }
    }
  }
}

bool ConvertToDawn(const GPUImageCopyTexture* in,
                   WGPUImageCopyTexture* out,
                   ExceptionState& exception_state) {
  DCHECK(in);
  DCHECK(in->texture());

  *out = {};
  out->texture = in->texture()->GetHandle();
  out->mipLevel = in->mipLevel();
  out->aspect = AsDawnEnum(in->aspect());
  return ConvertToDawn(in->origin(), &out->origin, exception_state);
}

// Dawn represents `undefined` as the special uint32_t value
// WGPU_COPY_STRIDE_UNDEFINED (0xFFFF'FFFF). Blink must make sure that an
// actual value of 0xFFFF'FFFF coming in from JS is not treated as
// WGPU_COPY_STRIDE_UNDEFINED, so it injects an error in that case.
const char* ValidateTextureDataLayout(const GPUImageDataLayout* webgpu_layout,
                                      WGPUTextureDataLayout* dawn_layout) {
  DCHECK(webgpu_layout);

  uint32_t bytesPerRow = 0;
  if (webgpu_layout->hasBytesPerRow()) {
    bytesPerRow = webgpu_layout->bytesPerRow();
    if (bytesPerRow == WGPU_COPY_STRIDE_UNDEFINED) {
      return "bytesPerRow must be a multiple of 256";
    }
  } else {
    bytesPerRow = WGPU_COPY_STRIDE_UNDEFINED;
  }

  uint32_t rowsPerImage = 0;
  if (webgpu_layout->hasRowsPerImage()) {
    rowsPerImage = webgpu_layout->rowsPerImage();
    if (rowsPerImage == WGPU_COPY_STRIDE_UNDEFINED) {
      return "rowsPerImage is too large";
    }
  } else {
    rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED;
  }

  *dawn_layout = {};
  dawn_layout->offset = webgpu_layout->offset();
  dawn_layout->bytesPerRow = bytesPerRow;
  dawn_layout->rowsPerImage = rowsPerImage;

  return nullptr;
}

WGPUTextureFormat AsDawnType(SkColorType color_type) {
  switch (color_type) {
    case SkColorType::kRGBA_8888_SkColorType:
      return WGPUTextureFormat_RGBA8Unorm;
    case SkColorType::kBGRA_8888_SkColorType:
      return WGPUTextureFormat_BGRA8Unorm;
    case SkColorType::kRGBA_1010102_SkColorType:
      return WGPUTextureFormat_RGB10A2Unorm;
    case SkColorType::kRGBA_F16_SkColorType:
      return WGPUTextureFormat_RGBA16Float;
    case SkColorType::kRGBA_F32_SkColorType:
      return WGPUTextureFormat_RGBA32Float;
    case SkColorType::kR8G8_unorm_SkColorType:
      return WGPUTextureFormat_RG8Unorm;
    case SkColorType::kR16G16_float_SkColorType:
      return WGPUTextureFormat_RG16Float;
    default:
      return WGPUTextureFormat_Undefined;
  }
}

WGPUPipelineLayout AsDawnType(
    V8UnionGPUAutoLayoutModeOrGPUPipelineLayout* webgpu_layout) {
  DCHECK(webgpu_layout);

  switch (webgpu_layout->GetContentType()) {
    case V8UnionGPUAutoLayoutModeOrGPUPipelineLayout::ContentType::
        kGPUPipelineLayout:
      return AsDawnType(webgpu_layout->GetAsGPUPipelineLayout());
    case V8UnionGPUAutoLayoutModeOrGPUPipelineLayout::ContentType::
        kGPUAutoLayoutMode:
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace blink
