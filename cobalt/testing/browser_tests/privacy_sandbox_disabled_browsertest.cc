// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

using PrivacySandboxDisabledBrowserTest = content::ContentBrowserTest;

// Verifies that Privacy Sandbox features (Fenced Frames, Private State Tokens /
// Trust Tokens) are disabled in Cobalt builds.
IN_PROC_BROWSER_TEST_F(PrivacySandboxDisabledBrowserTest,
                       VerifyPrivacySandboxApisDisabled) {
  // 1. Verify buildflag is disabled for Cobalt.
  EXPECT_FALSE(BUILDFLAG(ENABLE_PRIVACY_SANDBOX_APIS));

  // 2. Start test server and navigate to a page.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  // 3. Verify Fenced Frame APIs are not exposed to JavaScript.
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'HTMLFencedFrameElement' in window"));

  // 4. Verify Private State Tokens / Trust Tokens APIs are not exposed.
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'hasPrivateToken' in document"));
  EXPECT_EQ(false, content::EvalJs(shell()->web_contents(),
                                   "'hasTrustToken' in document"));
  EXPECT_EQ(false,
            content::EvalJs(shell()->web_contents(),
                            "'privateToken' in HTMLIFrameElement.prototype"));
}

}  // namespace cobalt
