// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_main_delegate.h"
#include "base/android/jni_android.h"
#include "cobalt/cobalt_content_browser_client.h"
#include "content/public/app/content_jni_onload.h"
#include "content/public/app/content_main.h"
#include "content/shell/app/shell_main_delegate.h"

namespace cobalt {

CobaltMainDelegate::CobaltMainDelegate(bool is_content_browsertests)
    : content::ShellMainDelegate(is_content_browsertests) {}

CobaltMainDelegate::~CobaltMainDelegate() {}

content::ContentBrowserClient*
CobaltMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<CobaltContentBrowserClient>();
  return browser_client_.get();
}

absl::optional<int> CobaltMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  absl::optional<int> return_value_parent =
      ShellMainDelegate::PostEarlyInitialization(invoked_in);
  JavaVM* vm = nullptr;
  base::android::InitVM(vm);
  if (!content::android::OnJNIOnLoadInit()) {
    return -1;
  }
  content::SetContentMainDelegate(new content::ShellMainDelegate());
}

}  // namespace cobalt
