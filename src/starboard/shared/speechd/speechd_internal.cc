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

#include "starboard/shared/speechd/speechd_internal.h"

#include "starboard/once.h"
#include "starboard/shared/starboard/application.h"

namespace starboard {
namespace shared {
namespace speechd {
namespace {
SbOnceControl init_once = SB_ONCE_INITIALIZER;
}  // namespace

SpeechDispatcher* SpeechDispatcher::instance = NULL;

// static
SpeechDispatcher* SpeechDispatcher::Get() {
  bool result = SbOnce(&init_once, &SpeechDispatcher::Initialize);
  SB_DCHECK(result);
  SB_DCHECK(instance);
  return instance;
}

SpeechDispatcher::SpeechDispatcher() {
  const char* client_name = "starboard_application";
  connection_ = spd_open(client_name, NULL, NULL, SPD_MODE_THREADED);
  if (!connection_) {
    SB_DLOG(ERROR) << "Failed to initialize SpeechDispatcher.";
  }
  if (!SetLanguage(SbSystemGetLocaleId())) {
    SB_DLOG(ERROR) << "Unable to set language to current locale.";
  }
}

SpeechDispatcher::~SpeechDispatcher() {
  SB_DLOG(INFO) << "Closing Speech Dispatcher.";
  if (connection_) {
    spd_close(connection_);
  }
}

bool SpeechDispatcher::SetLanguage(const char* language) {
  ScopedLock lock(lock_);
  if (connection_) {
    int result = spd_set_language(connection_, language);
    return result == 0;
  }
  return false;
}

void SpeechDispatcher::Speak(const char* text) {
  ScopedLock lock(lock_);
  if (connection_ && text && *text) {
    // Priority SPD_MESSAGE will be queued with other text of same priority.
    int result = spd_say(connection_, SPD_MESSAGE, text);
    if (result < 0) {
      SB_DLOG(ERROR) << "Failed to speak: " << text;
    }
  }
}

void SpeechDispatcher::Cancel() {
  ScopedLock lock(lock_);
  if (connection_) {
    int result = spd_cancel(connection_);
    if (result != 0) {
      SB_DLOG(ERROR) << "Failed to cancel speech.";
    }
  }
}

// static
void SpeechDispatcher::Initialize() {
  starboard::Application* application = starboard::Application::Get();
  if (application) {
    instance = new SpeechDispatcher();
    application->RegisterTeardownCallback(SpeechDispatcher::Destroy);
  }
}

// static
void SpeechDispatcher::Destroy() {
  delete instance;
  instance = NULL;
}

}  // namespace speechd
}  // namespace shared
}  // namespace starboard
