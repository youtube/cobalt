// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_

#include <stdint.h>

#include "base/macros.h"
#include "third_party/chromium/media/gpu/vaapi/vaapi_image_decoder.h"

namespace media_m96 {

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

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_VAAPI_VAAPI_WEBP_DECODER_H_
