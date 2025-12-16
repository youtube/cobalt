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

#ifndef COBALT_TESTING_BROWSER_TESTS_WEB_CONTENTS_DELEGATE_STUB_H_
#define COBALT_TESTING_BROWSER_TESTS_WEB_CONTENTS_DELEGATE_STUB_H_

#include "content/public/browser/web_contents_delegate.h"

namespace cobalt {
namespace browser_tests {

class WebContentsDelegateStub : public content::WebContentsDelegate {
 public:
  WebContentsDelegateStub();
  ~WebContentsDelegateStub() override;
};

}  // namespace browser_tests
}  // namespace cobalt

#endif  // COBALT_TESTING_BROWSER_TESTS_WEB_CONTENTS_DELEGATE_STUB_H_
