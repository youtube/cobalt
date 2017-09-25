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

#ifndef STARBOARD_SHARED_WIN32_VIDEO_TEXTURE_H_
#define STARBOARD_SHARED_WIN32_VIDEO_TEXTURE_H_

#include <D3d11_1.h>
#include <wrl\client.h>  // For ComPtr.

#include "starboard/shared/win32/media_common.h"

namespace starboard {
namespace shared {
namespace win32 {

// The contents of VideoFrame->native_texture()
class VideoTexture {
 public:
  virtual ~VideoTexture() {}
  // Retrieves texture associated with video frame.
  // This method must be called on the rasterizer thread.
  virtual Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() = 0;
};

// Contains all ID3D11 interfaces needed to perform a VideoProcessorBlt.
// For use with passing to VideoTextureImpl instances.
struct VideoBltInterfaces {
  Microsoft::WRL::ComPtr<ID3D11Device> dx_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator> video_processor_enum_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice1> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
};

class VideoFrameFactory {
 public:
  static VideoFramePtr Construct(ComPtr<IMFSample> sample,
                                 const RECT& display_aperture,
                                 const VideoBltInterfaces& interfaces);
  static int frames_in_flight();

 private:
  static void DeleteVideoTextureImpl(void* context, void* arg);

  static atomic_int32_t frames_in_flight_;
};

}  // namespace win32
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_WIN32_VIDEO_TEXTURE_H_
