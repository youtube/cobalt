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

#include "cobalt/testing/browser_tests/content_cert_verifier_browser_test.h"

namespace content {

CertVerifierBrowserTest::CertVerifierBrowserTest() = default;

CertVerifierBrowserTest::~CertVerifierBrowserTest() = default;

void CertVerifierBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  if (mock_cert_verifier_disabled_) {
    return;
  }

  mock_cert_verifier_.SetUpCommandLine(command_line);
}

void CertVerifierBrowserTest::SetUpInProcessBrowserTestFixture() {
  if (mock_cert_verifier_disabled_) {
    return;
  }

  mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void CertVerifierBrowserTest::TearDownInProcessBrowserTestFixture() {
  if (mock_cert_verifier_disabled_) {
    return;
  }

  mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
}

}  // namespace content
