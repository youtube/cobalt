// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_IMPL_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_IMPL_H_

#include "third_party/chromium/media/audio/audio_debug_recording_session.h"
#include "third_party/chromium/media/base/media_export.h"

namespace base {
class FilePath;
}

namespace media_m96 {

class MEDIA_EXPORT AudioDebugRecordingSessionImpl
    : public AudioDebugRecordingSession {
 public:
  explicit AudioDebugRecordingSessionImpl(
      const base::FilePath& debug_recording_file_path);

  AudioDebugRecordingSessionImpl(const AudioDebugRecordingSessionImpl&) =
      delete;
  AudioDebugRecordingSessionImpl& operator=(
      const AudioDebugRecordingSessionImpl&) = delete;

  ~AudioDebugRecordingSessionImpl() override;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_AUDIO_AUDIO_DEBUG_RECORDING_SESSION_IMPL_H_
