// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/audio/cobalt_audio_capturer_source.h"

#include "base/logging.h"

namespace media {

void CobaltAudioCapturerSource::Initialize(const AudioParameters& params,
                                           CaptureCallback* callback) {
  LOG(INFO) << "CobaltAudioCapturerSource::Initialize - called with following "
               "parameters:"
            << params.AsHumanReadableString();
  params_ = params;

  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = callback;
}

void CobaltAudioCapturerSource::Start() {
  LOG(INFO) << "YO THOR - CAOBALT AUDIO CAPTURE SOURCE START";
  DCHECK(callback_);
}

void CobaltAudioCapturerSource::Stop() {
  LOG(INFO) << "YO THOR - CAOBALT AUDIO CAPTURE SOURCE STAOP";
  callback_ = nullptr;
}

void CobaltAudioCapturerSource::SetVolume(double volume) {
  LOG(INFO) << "SetVolume - NOT IMPL";
  NOTIMPLEMENTED();
}

void CobaltAudioCapturerSource::SetAutomaticGainControl(bool enable) {
  LOG(INFO) << "SetAutomaticGainControl - NOT IMPL";
  NOTIMPLEMENTED();
}
void CobaltAudioCapturerSource::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  LOG(INFO) << "SetOutputDeviceForAec - NOT IMPL";
  NOTIMPLEMENTED();
}

}  // namespace media
