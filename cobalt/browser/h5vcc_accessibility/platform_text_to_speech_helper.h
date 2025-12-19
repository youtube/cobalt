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

#ifndef COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_H_
#define COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace h5vcc_accessibility {

class PlatformTextToSpeechHelper {
 public:
  class Client {
   public:
    virtual void NotifyTextToSpeechChange() = 0;

   protected:
    virtual ~Client() = default;
  };

  virtual ~PlatformTextToSpeechHelper();

  static std::unique_ptr<PlatformTextToSpeechHelper> Create(
      base::WeakPtr<Client> client);

  PlatformTextToSpeechHelper(const PlatformTextToSpeechHelper&) = delete;
  PlatformTextToSpeechHelper& operator=(const PlatformTextToSpeechHelper&) =
      delete;

  virtual bool IsTextToSpeechEnabled() = 0;

 protected:
  explicit PlatformTextToSpeechHelper(base::WeakPtr<Client> client);

  void NotifyTextToSpeechChange();

 private:
  base::WeakPtr<Client> client_;
};

}  // namespace h5vcc_accessibility

#endif  // COBALT_BROWSER_H5VCC_ACCESSIBILITY_PLATFORM_TEXT_TO_SPEECH_HELPER_H_
