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

#ifndef COBALT_TESTING_BROWSER_TESTS_BROWSER_TEST_SHELL_H_
#define COBALT_TESTING_BROWSER_TESTS_BROWSER_TEST_SHELL_H_

#include "cobalt/shell/browser/shell.h"

namespace content {

class TestShell : public Shell {
 public:
  ~TestShell() override;

  // Static method to create a new window. In tests, this should be used
  // instead of Shell::CreateNewWindow.
  static TestShell* CreateNewWindow(
      BrowserContext* browser_context,
      const GURL& url,
      const scoped_refptr<SiteInstance>& site_instance,
      const gfx::Size& initial_size);

  static void SetShellCreatedCallback(
      base::OnceCallback<void(TestShell*)> shell_created_callback);

  // WebContentsDelegate overrides
  void RegisterProtocolHandler(RenderFrameHost* requesting_frame,
                               const std::string& protocol,
                               const GURL& url,
                               bool user_gesture) override;
  bool DidAddMessageToConsole(WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const std::u16string& message,
                              int32_t line_no,
                              const std::u16string& source_id) override;
  PictureInPictureResult EnterPictureInPicture(
      WebContents* web_contents) override;
  void SetContentsBounds(WebContents* source, const gfx::Rect& bounds) override;

 private:
  TestShell(std::unique_ptr<WebContents> web_contents,
            bool should_set_delegate);

  // Create a new TestShell.
  static TestShell* CreateShell(std::unique_ptr<WebContents> web_contents,
                                const gfx::Size& initial_size,
                                bool should_set_delegate);

  static base::OnceCallback<void(TestShell*)> shell_created_callback_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_BROWSER_TEST_SHELL_H_
