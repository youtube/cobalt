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

#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper_android.h"
#elif BUILDFLAG(IS_STARBOARD)
#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper_starboard.h"
#elif BUILDFLAG(IS_IOS_TVOS)
#include "cobalt/browser/h5vcc_accessibility/platform_text_to_speech_helper_tvos.h"
#endif

namespace h5vcc_accessibility {

PlatformTextToSpeechHelper::PlatformTextToSpeechHelper(
    base::WeakPtr<Client> client)
    : client_(client) {}

PlatformTextToSpeechHelper::~PlatformTextToSpeechHelper() = default;

// static
std::unique_ptr<PlatformTextToSpeechHelper> PlatformTextToSpeechHelper::Create(
    base::WeakPtr<Client> client) {
#if BUILDFLAG(IS_ANDROIDTV)
  return std::make_unique<PlatformTextToSpeechHelperAndroid>(client);
#elif BUILDFLAG(IS_STARBOARD)
  return std::make_unique<PlatformTextToSpeechHelperStarboard>(client);
#elif BUILDFLAG(IS_IOS_TVOS)
  return std::make_unique<PlatformTextToSpeechHelperTvos>(client);
#else
#error "Unsupported platform."
#endif
}

void PlatformTextToSpeechHelper::NotifyTextToSpeechChange() {
  if (client_) {
    client_->NotifyTextToSpeechChange();
  }
}

}  // namespace h5vcc_accessibility
