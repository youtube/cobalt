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

namespace {
// The maximum of microphones which can be supported. Currently only supports
// one microphone.
const int kNumberOfMicrophones = 1;
template <std::size_t N>
bool IsNullTerminated(const char (&str)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (str[i] == '\0') {
      return true;
    }
  }
  return false;
}
}  // namespace

void CobaltAudioCapturerSource::Initialize(const AudioParameters& params,
                                           CaptureCallback* callback) {
  LOG(INFO) << "CobaltAudioCapturerSource::Initialize - called with following "
               "parameters:"
            << params.AsHumanReadableString();
  params_ = params;

  DCHECK(callback);
  DCHECK(!callback_);
  callback_ = callback;

  SbMicrophoneInfo info[kNumberOfMicrophones];
  int microphone_num = SbMicrophoneGetAvailable(info, kNumberOfMicrophones);

  // Loop all the available microphones and create a valid one.
  for (int index = 0; index < microphone_num; ++index) {
    if (!SbMicrophoneIsSampleRateSupported(info[index].id, 16000)) {
      continue;
    }

    microphone_ = SbMicrophoneCreate(info[index].id, 16000, 960);
    if (!SbMicrophoneIsValid(microphone_)) {
      continue;
    }

    // Created a microphone successfully.
    min_microphone_read_in_bytes_ = info[index].min_read_size;

    if (IsNullTerminated(info[index].label)) {
      label_ = info[index].label;
    }
  }
  DCHECK(SbMicrophoneIsValid(microphone_));
  LOG(INFO) << "YO THOR - WE GOOD, HAZ MIC!";
}

void CobaltAudioCapturerSource::Start() {
  LOG(INFO) << "YO THOR - CAOBALT AUDIO CAPTURE SOURCE START";
  DCHECK(callback_);
  DCHECK(SbMicrophoneIsValid(microphone_));
  // OPENS AND STARTS RECORDING
  SbMicrophoneOpen(microphone_);
  // TODO - need to read data anfdput in callback -
  // SbMicrophoneRead(microphone_, out_data, data_size);
}

void CobaltAudioCapturerSource::Stop() {
  LOG(INFO) << "YO THOR - CAOBALT AUDIO CAPTURE SOURCE STAOP";
  SbMicrophoneClose(microphone_);
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
