// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_

#include <stdint.h>

#include "base/macros.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"

namespace media {

class VaapiWebPDecoder : public VaapiImageDecoder {
 public:
  VaapiWebPDecoder();

  VaapiWebPDecoder(const VaapiWebPDecoder&) = delete;
  VaapiWebPDecoder& operator=(const VaapiWebPDecoder&) = delete;

  ~VaapiWebPDecoder() override;

  // VaapiImageDecoder implementation.
  gpu::ImageDecodeAcceleratorType GetType() const override;
  SkYUVColorSpace GetYUVColorSpace() const override;

 private:
  // VaapiImageDecoder implementation.
  VaapiImageDecodeStatus AllocateVASurfaceAndSubmitVABuffers(
      base::span<const uint8_t> encoded_image) override;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_
