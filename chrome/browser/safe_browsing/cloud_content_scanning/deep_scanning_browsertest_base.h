// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"

namespace safe_browsing {

// Base test class for deep scanning browser tests. Common utility functions
// used by browser tests should be added to this class.
class DeepScanningBrowserTestBase : public InProcessBrowserTest {
 public:
  DeepScanningBrowserTestBase();
  ~DeepScanningBrowserTestBase() override;

  void TearDownOnMainThread() override;

  // Sets up a FakeContentAnalysisDelegate to use this class's StatusCallback
  // and EncryptionStatusCallback. Also sets up a test DM token.
  void SetUpDelegate();

  // Sets up a FakeContentAnalysisDelegate that never returns responses.
  void SetUpUnresponsiveDelegate();

  // Set up a quit closure to be called by the test. This is useful to control
  // when the test ends.
  void SetQuitClosure(base::RepeatingClosure quit_closure);
  void CallQuitClosure();

  // Set what StatusCallback returns.
  void SetStatusCallbackResponse(
      enterprise_connectors::ContentAnalysisResponse response);

  // Callbacks used to set up the fake delegate factory.
  enterprise_connectors::ContentAnalysisResponse StatusCallback(
      const std::string& contents,
      const base::FilePath& path);

  // Creates temporary files for testing in |temp_dir_|, and add them to |data|.
  void CreateFilesForTest(
      const std::vector<std::string>& paths,
      const std::vector<std::string>& contents,
      enterprise_connectors::ContentAnalysisDelegate::Data* data);

  const std::vector<base::FilePath>& created_file_paths() const;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  base::RepeatingClosure quit_closure_;
  enterprise_connectors::ContentAnalysisResponse
      connector_status_callback_response_;
  base::ScopedTempDir temp_dir_;
  std::vector<base::FilePath> created_file_paths_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_BROWSERTEST_BASE_H_
