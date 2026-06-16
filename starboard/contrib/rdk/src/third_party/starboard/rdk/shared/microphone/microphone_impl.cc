// Copyright 2026 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// SPDX-License-Identifier: Apache-2.0

#include "starboard/shared/starboard/microphone/microphone_internal.h"

#include "local_aows_server.h"
#include "starboard/common/log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

using namespace starboard::rdk::shared::microphone;

namespace {

const SbMicrophoneId kMicrophoneId = reinterpret_cast<SbMicrophoneId>(1);
const int kSampleRateInHz = 16000;
const int kMinReadSizeBytes = 320;
const char kMicrophoneLabel[] = "RDK Microphone";
const char logtag[] = "sbMicImpl";
class SbMicrophoneImpl : public SbMicrophonePrivate {
 public:
  explicit SbMicrophoneImpl(int buffer_size_bytes)
      : buffer_size_bytes_(buffer_size_bytes), state_(kClosed) {
    starboard::rdk::shared::microphone::EnsureLocalAowsServerStarted();
  }

  enum State {
    kOpened,
    kClosed,
  };

  bool Open() override {
    // Open() can be called repeatedly; clear unread buffered data on reopen.
    starboard::rdk::shared::microphone::ClearLocalAowsBufferedAudio();
    state_ = kOpened;
    return true;
  }

  bool Close() override {
    state_ = kClosed;
    starboard::rdk::shared::microphone::ClearLocalAowsBufferedAudio();
    SB_LOG(INFO) << logtag << ": Microphone closed.";
    AOWSLog(kInfo, "Microphone closed.\n");
    return true;
  }

  int Read(void* out_audio_data, int audio_data_size) override {
    if (state_ == kClosed) {
      SB_LOG(WARNING) << logtag << ": Read called on closed microphone.";
      AOWSLog(kWarning, "Read called on closed microphone.\n");
      return -1;
    }

    if (audio_data_size < 0) {
      SB_LOG(WARNING) << logtag << ": Read called with negative audio_data_size " << audio_data_size;
      AOWSLog(kWarning, "Read called with negative audio_data_size %d\n", audio_data_size);
      return -1;
    }

    if (!out_audio_data) {
      SB_LOG(WARNING) << logtag << ": Read called with null out_audio_data pointer.";
      AOWSLog(kWarning, "Read called with null out_audio_data pointer.\n");
      return 0;
    }

    if (audio_data_size == 0) {
      SB_LOG(WARNING) << logtag << ": Read called with zero audio_data_size.";
      AOWSLog(kWarning, "Read called with zero audio_data_size.\n");
      return 0;
    }

    if (audio_data_size < kMinReadSizeBytes) {
      SB_LOG(WARNING) << logtag << ": Read size " << audio_data_size
                      << " is less than minimum " << kMinReadSizeBytes
                      << ", clearing buffered audio.";
      AOWSLog(kWarning, "Read size %d is less than minimum %d, clearing buffered audio.\n", audio_data_size, kMinReadSizeBytes);
      starboard::rdk::shared::microphone::ClearLocalAowsBufferedAudio();
      return 0;
    }

    const int bytes_read = starboard::rdk::shared::microphone::ReadLocalAowsAudio(
        out_audio_data, std::min(audio_data_size, buffer_size_bytes_));
    if (bytes_read > 0) {
      SB_LOG(INFO) << logtag << ": Read " << bytes_read << " bytes from AOWS buffer.";
      AOWSLog(kInfo, "Read %d bytes from AOWS buffer.\n", bytes_read);
    }
    return bytes_read;
  }

 private:
  const int buffer_size_bytes_;
  State state_;
};

SbMicrophone s_microphone = kSbMicrophoneInvalid;

}  // namespace

int SbMicrophonePrivate::GetAvailableMicrophones(SbMicrophoneInfo* out_info_array,
                                                 int info_array_size) {
  SB_LOG(INFO) << logtag << ": GetAvailableMicrophones called!";
  AOWSLog(kInfo, "GetAvailableMicrophones called!\n");

  starboard::rdk::shared::microphone::EnsureLocalAowsServerStarted();

  if (out_info_array && info_array_size > 0) {
    out_info_array[0].id = kMicrophoneId;
    out_info_array[0].type = kSbMicrophoneUnknown;
    out_info_array[0].max_sample_rate_hz = kSampleRateInHz;
    out_info_array[0].min_read_size = kMinReadSizeBytes;
    std::strncpy(out_info_array[0].label, kMicrophoneLabel,
                 sizeof(out_info_array[0].label) - 1);
    out_info_array[0].label[sizeof(out_info_array[0].label) - 1] = '\0';
  }
  return 1;
}

bool SbMicrophonePrivate::IsMicrophoneSampleRateSupported(SbMicrophoneId id,
                                                          int sample_rate_in_hz) {
  starboard::rdk::shared::microphone::EnsureLocalAowsServerStarted();

  if (!SbMicrophoneIdIsValid(id) || id != kMicrophoneId) {
    SB_LOG(WARNING) << logtag << ": IsMicrophoneSampleRateSupported returning false for id " << id;
    AOWSLog(kWarning, "IsMicrophoneSampleRateSupported returning false for id %p\n", id);
    return false;
  }
  return sample_rate_in_hz == kSampleRateInHz;
}

SbMicrophone SbMicrophonePrivate::CreateMicrophone(SbMicrophoneId id,
                                                   int sample_rate_in_hz,
                                                   int buffer_size_bytes) {
  starboard::rdk::shared::microphone::EnsureLocalAowsServerStarted();

  if (!SbMicrophoneIdIsValid(id) || id != kMicrophoneId ||
      !IsMicrophoneSampleRateSupported(id, sample_rate_in_hz) ||
      buffer_size_bytes <= 0 || s_microphone != kSbMicrophoneInvalid) {
    SB_LOG(WARNING) << logtag << ": CreateMicrophone failed for id " << id
                    << ", sample_rate_in_hz " << sample_rate_in_hz
                    << ", buffer_size_bytes " << buffer_size_bytes
                    << ", existing microphone " << (s_microphone != kSbMicrophoneInvalid);
    AOWSLog(kWarning, "CreateMicrophone failed for id %p, sample_rate_in_hz %d, buffer_size_bytes %d, existing microphone %s\n",
            id, sample_rate_in_hz, buffer_size_bytes, (s_microphone != kSbMicrophoneInvalid) ? "true" : "false");
    return kSbMicrophoneInvalid;
  }

  s_microphone = new SbMicrophoneImpl(buffer_size_bytes);
  return s_microphone;
}

void SbMicrophonePrivate::DestroyMicrophone(SbMicrophone microphone) {
  SB_LOG(INFO) << logtag << ": DestroyMicrophone called for microphone " << microphone;
  AOWSLog(kInfo, "DestroyMicrophone called for microphone %p\n", microphone);
  if (!SbMicrophoneIsValid(microphone)) {
    return;
  }

  if (s_microphone == microphone) {
    s_microphone->Close();
    delete s_microphone;
    s_microphone = kSbMicrophoneInvalid;
  }
}
