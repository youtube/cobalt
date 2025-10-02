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

#include "cobalt/shell/browser/shell_speech_recognition_manager_delegate.h"

#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::OnceCallback;

namespace content {

void ShellSpeechRecognitionManagerDelegate::CheckRecognitionIsAllowed(
    int session_id,
    OnceCallback<void(bool ask_user, bool is_allowed)> callback) {
  // In content_shell, we expect speech recognition to happen when requested.
  // Therefore we simply authorize it by calling back with is_allowed=true. The
  // first parameter, ask_user, is set to false because we don't want to prompt
  // the user for permission with an infobar.
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), false, true));
}

SpeechRecognitionEventListener*
ShellSpeechRecognitionManagerDelegate::GetEventListener() {
  return nullptr;
}

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
void ShellSpeechRecognitionManagerDelegate::BindSpeechRecognitionContext(
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {}
#endif

}  // namespace content
