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

#ifndef COBALT_TESTING_BROWSER_TESTS_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_
#define COBALT_TESTING_BROWSER_TESTS_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_

#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"

namespace content {

// CertVerifierBrowserTest allows tests to force certificate verification
// results for requests made with any profile's main request context (such as
// navigations). To do so, tests can use the MockCertVerifier exposed via
// CertVerifierBrowserTest::mock_cert_verifier().
class CertVerifierBrowserTest : public ContentBrowserTest {
 public:
  CertVerifierBrowserTest();

  CertVerifierBrowserTest(const CertVerifierBrowserTest&) = delete;
  CertVerifierBrowserTest& operator=(const CertVerifierBrowserTest&) = delete;

  ~CertVerifierBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

  void disable_mock_cert_verifier() { mock_cert_verifier_disabled_ = true; }

 private:
  bool mock_cert_verifier_disabled_ = false;
  ContentMockCertVerifier mock_cert_verifier_;
};

}  // namespace content

#endif  // COBALT_TESTING_BROWSER_TESTS_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_
