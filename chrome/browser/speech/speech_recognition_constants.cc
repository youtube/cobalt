// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_constants.h"
#include "base/notreached.h"

namespace speech {

extensions::api::speech_recognition_private::SpeechRecognitionType
SpeechRecognitionTypeToApiType(SpeechRecognitionType type) {
  switch (type) {
    case SpeechRecognitionType::kNetwork:
      return extensions::api::speech_recognition_private::
          SpeechRecognitionType::SPEECH_RECOGNITION_TYPE_NETWORK;
    case SpeechRecognitionType::kOnDevice:
      return extensions::api::speech_recognition_private::
          SpeechRecognitionType::SPEECH_RECOGNITION_TYPE_ONDEVICE;
  }

  NOTREACHED();
}

}  // namespace speech
