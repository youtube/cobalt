// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_ANDROID_H_
#define CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_ANDROID_H_

#include "components/upload_list/text_log_upload_list.h"

namespace base {
class FilePath;
}

// An UploadList that retrieves the list of crash reports available on the
// client. This uses both the Breakpad text log format, as well as inspecting
// the un-uploaded Minidump directory, managed by the MinidumpUploadService.
class CrashUploadListAndroid : public TextLogUploadList {
 public:
  explicit CrashUploadListAndroid(const base::FilePath& upload_log_path);

  CrashUploadListAndroid(const CrashUploadListAndroid&) = delete;
  CrashUploadListAndroid& operator=(const CrashUploadListAndroid&) = delete;

  // Returns true if the browser crash metrics were initialized, only happens
  // when minidump service is started.
  static bool BrowserCrashMetricsInitialized();

  // Returns true if a browser crash was observed in recent sessions. Can be
  // called only when BrowserCrashMetricsInitialized() returns true.
  static bool DidBrowserCrashRecently();

 protected:
  ~CrashUploadListAndroid() override;

  std::vector<std::unique_ptr<UploadList::UploadInfo>> LoadUploadList()
      override;
  void RequestSingleUpload(const std::string& local_id) override;

 private:
  void LoadUnsuccessfulUploadList(
      std::vector<std::unique_ptr<UploadInfo>>* uploads);
};

#endif  // CHROME_BROWSER_CRASH_UPLOAD_LIST_CRASH_UPLOAD_LIST_ANDROID_H_
