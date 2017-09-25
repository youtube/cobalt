// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/shared/win32/video_texture.h"

#include "starboard/shared/win32/error_utils.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {

// Makes a a video output texture for use with VideoProcessorBlt.
ComPtr<ID3D11Texture2D> MakeOutputTexture(
    const ComPtr<ID3D11Device>& dx_device, int width, int height) {
  ComPtr<ID3D11Texture2D> output_texture;
  D3D11_TEXTURE2D_DESC out_texture_desc = {};
  out_texture_desc.Width = width;
  out_texture_desc.Height = height;
  out_texture_desc.MipLevels = 1;
  out_texture_desc.ArraySize = 1;
  out_texture_desc.Format = DXGI_FORMAT_NV12;
  out_texture_desc.SampleDesc.Count = 1;
  out_texture_desc.SampleDesc.Quality = 0;
  // Per https://msdn.microsoft.com/en-us/library/windows/desktop/hh447791(v=vs.85).aspx
  // This must be USAGE_DEFAULT and BIND_RENDER_TARGET
  // BIND_SHADER_RESOURCE is required for subsequent ANGLE use.
  out_texture_desc.Usage = D3D11_USAGE_DEFAULT;
  out_texture_desc.BindFlags =
      D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

  HRESULT hr = dx_device->CreateTexture2D(
      &out_texture_desc, nullptr, &output_texture);
  CheckResult(hr);

  return output_texture;
}
class VideoTextureImpl : public VideoTexture {
 public:
  VideoTextureImpl(
      const ComPtr<IMFSample>& sample, const RECT& display_aperture,
      const VideoBltInterfaces& interfaces)
      : sample_(sample), display_aperture_(display_aperture),
        video_blt_interfaces_(interfaces) {
  }

  ComPtr<ID3D11Texture2D> GetTexture() override {
    // TODO check this is only called on the renderer thread

    // In cases where we display the same frame multiple times,
    // only VideoProcessorBlt once.
    if (texture_) {
      return texture_;
    }
    ComPtr<IMFMediaBuffer> media_buffer;
    HRESULT hr = sample_->GetBufferByIndex(0, &media_buffer);
    CheckResult(hr);

    ComPtr<IMFDXGIBuffer> dxgi_buffer;
    hr = media_buffer.As(&dxgi_buffer);
    CheckResult(hr);

    ComPtr<ID3D11Texture2D> input_texture;
    hr = dxgi_buffer->GetResource(IID_PPV_ARGS(&input_texture));
    CheckResult(hr);

    // The VideoProcessor needs to know what subset of the decoded
    // frame contains active pixels that should be displayed to the user.
    video_blt_interfaces_.video_context_->VideoProcessorSetStreamSourceRect(
        video_blt_interfaces_.video_processor_.Get(), 0, TRUE,
        &display_aperture_);

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_desc = {};
    input_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    input_desc.Texture2D.MipSlice = 0;
    // The DXGI subresource index is the texture array index.
    dxgi_buffer->GetSubresourceIndex(&input_desc.Texture2D.ArraySlice);

    ComPtr<ID3D11VideoProcessorInputView> input_view;
    hr = video_blt_interfaces_.video_device_->CreateVideoProcessorInputView(
        input_texture.Get(), video_blt_interfaces_.video_processor_enum_.Get(),
        &input_desc, &input_view);
    CheckResult(hr);

    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_desc = {};
    output_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    output_desc.Texture2D.MipSlice = 0;

    texture_ = MakeOutputTexture(
        video_blt_interfaces_.dx_device_,
        display_aperture_.right,
        display_aperture_.bottom);

    ComPtr<ID3D11VideoProcessorOutputView> output_view;
    hr = video_blt_interfaces_.video_device_->CreateVideoProcessorOutputView(
        texture_.Get(), video_blt_interfaces_.video_processor_enum_.Get(),
        &output_desc, &output_view);
    CheckResult(hr);

    // We have a single video stream, which is enabled for display.
    D3D11_VIDEO_PROCESSOR_STREAM stream_info = {};
    stream_info.Enable = TRUE;
    stream_info.pInputSurface = input_view.Get();

    hr = video_blt_interfaces_.video_context_->VideoProcessorBlt(
        video_blt_interfaces_.video_processor_.Get(), output_view.Get(),
        0, 1, &stream_info);
    CheckResult(hr);

    return texture_;
  }

 private:
  ComPtr<IMFSample> sample_;
  RECT display_aperture_;
  VideoBltInterfaces video_blt_interfaces_;
  ComPtr<ID3D11Texture2D> texture_;
};

}  // namespace

VideoFramePtr VideoFrameFactory::Construct(
    ComPtr<IMFSample> sample,
    const RECT& display_aperture,
    const VideoBltInterfaces& interfaces) {
  LONGLONG win32_timestamp = 0;
  HRESULT hr = sample->GetSampleTime(&win32_timestamp);
  CheckResult(hr);

  VideoFramePtr out(
      new VideoFrame(display_aperture.right, display_aperture.bottom,
                     ConvertToMediaTime(win32_timestamp),
                     new VideoTextureImpl(sample,
                                          display_aperture,
                                          interfaces),
                     nullptr, DeleteVideoTextureImpl));
  frames_in_flight_.increment();
  return out;
}

int VideoFrameFactory::frames_in_flight() {
  return frames_in_flight_.load();
}

void VideoFrameFactory::DeleteVideoTextureImpl(void* context, void* arg) {
  SB_UNREFERENCED_PARAMETER(context);
  VideoTextureImpl* texture = static_cast<VideoTextureImpl*>(arg);
  delete texture;
  frames_in_flight_.decrement();
}

atomic_int32_t VideoFrameFactory::frames_in_flight_;

}  // namespace win32
}  // namespace shared
}  // namespace starboard
