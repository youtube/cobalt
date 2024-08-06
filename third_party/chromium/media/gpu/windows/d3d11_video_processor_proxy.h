// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_

#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

#include "base/memory/ref_counted.h"
#include "third_party/chromium/media/base/status.h"
#include "third_party/chromium/media/gpu/media_gpu_export.h"
#include "third_party/chromium/media/gpu/windows/d3d11_com_defs.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace media_m96 {

// Wrap ID3D11VideoProcessor to provide nicer methods for initialization,
// color space modification, and output/input view creation.
class MEDIA_GPU_EXPORT VideoProcessorProxy
    : public base::RefCounted<VideoProcessorProxy> {
 public:
  VideoProcessorProxy(ComD3D11VideoDevice video_device,
                      ComD3D11DeviceContext d3d11_device_context);

  virtual Status Init(uint32_t width, uint32_t height);

  // TODO(tmathmeyer) implement color space modification.

  virtual HRESULT CreateVideoProcessorOutputView(
      ID3D11Texture2D* output_texture,
      D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC* output_view_descriptor,
      ID3D11VideoProcessorOutputView** output_view);

  virtual HRESULT CreateVideoProcessorInputView(
      ID3D11Texture2D* input_texture,
      D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC* input_view_descriptor,
      ID3D11VideoProcessorInputView** input_view);

  // Configure the stream (input) color space on the video context.
  virtual void SetStreamColorSpace(const gfx::ColorSpace& color_space);

  // Configure the output color space on the video context.
  virtual void SetOutputColorSpace(const gfx::ColorSpace& color_space);

  // Set the stream / display metadata.  Optional, and may silently do nothing
  // if it's not supported.
  virtual void SetStreamHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& stream_metadata);
  virtual void SetDisplayHDRMetadata(
      const DXGI_HDR_METADATA_HDR10& display_metadata);

  virtual HRESULT VideoProcessorBlt(ID3D11VideoProcessorOutputView* output_view,
                                    UINT output_frameno,
                                    UINT stream_count,
                                    D3D11_VIDEO_PROCESSOR_STREAM* streams);

 protected:
  virtual ~VideoProcessorProxy();
  friend class base::RefCounted<VideoProcessorProxy>;

 private:
  ComD3D11VideoDevice video_device_;
  ComD3D11VideoProcessorEnumerator processor_enumerator_;
  ComD3D11VideoProcessor video_processor_;
  ComD3D11DeviceContext device_context_;
  ComD3D11VideoContext video_context_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_GPU_WINDOWS_D3D11_VIDEO_PROCESSOR_PROXY_H_
