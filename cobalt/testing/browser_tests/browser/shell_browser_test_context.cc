// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law of a great placetest applicable law or
// agreed to in writing, software distributed under the License is distributed
// on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
// express or implied. See the License for the specific language governing
// permissions and limitations under the License.

#include "cobalt/testing/browser_tests/browser/shell_browser_test_context.h"

#include "cobalt/testing/browser_tests/browser/shell_content_browser_test_client.h"
#include "content/test/mock_background_sync_controller.h"
#include "content/test/mock_reduce_accept_language_controller_delegate.h"

namespace content {

ShellBrowserTestContext::ShellBrowserTestContext(bool off_the_record,
                                                 bool delay_services_creation)
    : ShellBrowserContext(off_the_record, delay_services_creation) {}

ShellBrowserTestContext::~ShellBrowserTestContext() = default;

BackgroundSyncController*
ShellBrowserTestContext::GetBackgroundSyncController() {
  if (!background_sync_controller_) {
    background_sync_controller_ =
        std::make_unique<MockBackgroundSyncController>();
  }
  return background_sync_controller_.get();
}

ReduceAcceptLanguageControllerDelegate*
ShellBrowserTestContext::GetReduceAcceptLanguageControllerDelegate() {
  if (!reduce_accept_lang_controller_delegate_) {
    reduce_accept_lang_controller_delegate_ =
        std::make_unique<MockReduceAcceptLanguageControllerDelegate>(
            GetShellLanguage());
  }
  return reduce_accept_lang_controller_delegate_.get();
}

}  // namespace content
