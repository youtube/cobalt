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

#include "cobalt/testing/browser_tests/browser/shell_test_platform_delegate.h"

#include "ui/views/test/desktop_test_views_delegate.h"

namespace content {

// The overridden method provides the test-specific ViewsDelegate.
// The base class's Initialize() method will call this version.
std::unique_ptr<views::ViewsDelegate>
ShellTestPlatformDelegate::CreateViewsDelegate() {
  return std::make_unique<views::DesktopTestViewsDelegate>();
}

}  // namespace content
