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

#include "starboard/shared/win32/video_transform.h"

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/shared/win32/error_utils.h"
#include "starboard/shared/win32/media_common.h"
#include "starboard/shared/win32/media_foundation_utils.h"
#include "starboard/shared/win32/media_transform.h"

namespace starboard {
namespace shared {
namespace win32 {

using Microsoft::WRL::ComPtr;

namespace {

GUID CLSID_VideoDecoderVPX() {
  GUID output = {0xE3AAF548, 0xC9A4, 0x4C6E, 0x23, 0x4D, 0x5A,
                 0xDA,       0x37,   0x4B,   0x00, 0x00};
  return output;
}

// CLSID_CMSVideoDecoderMFT {62CE7E72-4C71-4D20-B15D-452831A87D9D}
GUID CLSID_VideoDecoderH264() {
  GUID output = {0x62CE7E72, 0x4C71, 0x4d20, 0xB1, 0x5D, 0x45,
                 0x28,       0x31,   0xA8,   0x7D, 0x9D};
  return output;
}

ComPtr<IMFMediaType> CreateVideoMediaType(const GUID& media_subtype) {
  ComPtr<IMFMediaType> input_type;
  HRESULT hr = MFCreateMediaType(&input_type);
  CheckResult(hr);
  hr = input_type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  CheckResult(hr);
  hr = input_type->SetGUID(MF_MT_SUBTYPE, media_subtype);
  CheckResult(hr);
  return input_type;
}

void ApplyOutputFormat(VideoFormat video_fmt, MediaTransform* target) {
  switch (video_fmt) {
    case kVideoFormat_YV12: {
      target->SetOutputTypeBySubType(MFVideoFormat_YV12);
      break;
    }
  }
}

}  // namespace.

scoped_ptr<MediaTransform> CreateH264Transform(VideoFormat video_fmt) {
  ComPtr<IMFTransform> transform;
  HRESULT hr = CreateDecoderTransform(CLSID_VideoDecoderH264(), &transform);
  CheckResult(hr);
  ComPtr<IMFMediaType> input_type = CreateVideoMediaType(MFVideoFormat_H264);
  hr = input_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_Progressive);
  CheckResult(hr);
  hr = transform->SetInputType(MediaTransform::kStreamId, input_type.Get(), 0);
  CheckResult(hr);
  scoped_ptr<MediaTransform> out(new MediaTransform(transform));
  ApplyOutputFormat(video_fmt, out.get());
  return out.Pass();
}

scoped_ptr<MediaTransform> TryCreateVP9Transform(VideoFormat video_fmt,
                                                 int width,
                                                 int height) {
  ComPtr<IMFTransform> transform;
  HRESULT hr = CreateDecoderTransform(CLSID_VideoDecoderVPX(), &transform);
  // VpX decoder could not be created at all.
  if (!transform) {
    return scoped_ptr<MediaTransform>(nullptr);
  }
  ComPtr<IMFMediaType> input_type = CreateVideoMediaType(MFVideoFormat_VP90);
  hr = input_type->SetUINT32(MF_MT_INTERLACE_MODE,
                             MFVideoInterlace_Progressive);
  CheckResult(hr);
  hr = MFSetAttributeSize(input_type.Get(), MF_MT_FRAME_SIZE, width, height);
  CheckResult(hr);
  hr = transform->SetInputType(MediaTransform::kStreamId, input_type.Get(), 0);
  if (!SUCCEEDED(hr)) {
    return scoped_ptr<MediaTransform>(nullptr);
  }
  scoped_ptr<MediaTransform> out(new MediaTransform(transform));
  ApplyOutputFormat(video_fmt, out.get());
  return out.Pass();
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard
