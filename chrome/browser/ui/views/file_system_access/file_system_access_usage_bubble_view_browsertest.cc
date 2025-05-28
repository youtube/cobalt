// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_system_access/file_system_access_usage_bubble_view.h"

#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/test/browser_test.h"

class FileSystemAccessUsageBubbleViewTest : public DialogBrowserTest {
 public:
  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    FileSystemAccessUsageBubbleView::Usage usage;
    url::Origin origin = kTestOrigin;
    if (name == "SingleWritableFile") {
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
    } else if (name == "TwoWritableFiles") {
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("/bla/README.txt"));
    } else if (name == "SingleReadableFile") {
      usage.readable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
    } else if (name == "MultipleReadableFiles") {
      usage.readable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
      usage.readable_files.emplace_back(FILE_PATH_LITERAL("/bla/README.txt"));
    } else if (name == "SingleWritableFolder") {
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    } else if (name == "MultipleWritableFolders") {
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/baz/My Project"));
      usage.writable_directories.emplace_back(FILE_PATH_LITERAL("/baz/Assets"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/la/asdf/Processing"));
    } else if (name == "WritableFilesAndFolders") {
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("/bla/README.txt"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    } else if (name == "ReadableFilesAndFolders") {
      usage.readable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
      usage.readable_files.emplace_back(FILE_PATH_LITERAL("/bla/README.txt"));
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    } else if (name == "SingleReadableFolder") {
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Images"));
    } else if (name == "MultipleReadableFolders") {
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Images"));
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/baz/My Project"));
      usage.readable_directories.emplace_back(FILE_PATH_LITERAL("/baz/Assets"));
    } else if (name == "ReadableAndWritableFolders") {
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Images"));
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/baz/My Project"));
      usage.readable_directories.emplace_back(FILE_PATH_LITERAL("/baz/Assets"));
      usage.writable_directories.emplace_back(FILE_PATH_LITERAL("/baz/Assets"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/la/asdf/Processing"));
      usage.writable_directories.emplace_back(FILE_PATH_LITERAL("/baz/Images"));
    } else if (name == "LongOrigin") {
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Shapes.sketch"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("/bla/README.txt"));
      origin = url::Origin::Create(GURL(
          "https://"
          "some-really-long-origin-chrome-test-foo-bar-sample.appspot.com"));
    } else {
      CHECK_EQ(name, "default");
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/home/me/Images"));
      usage.readable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Processing"));
      usage.readable_directories.emplace_back(FILE_PATH_LITERAL("Assets"));
      usage.writable_files.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/index.html"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("view.js"));
      usage.writable_files.emplace_back(FILE_PATH_LITERAL("README.md"));
      usage.writable_directories.emplace_back(
          FILE_PATH_LITERAL("/foo/bar/Code"));
    }

    FileSystemAccessUsageBubbleView::ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), origin,
        std::move(usage));
  }

 protected:
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://example.com"));
};

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest, InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_SingleWritableFile) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_TwoWritableFiles) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_SingleReadableFile) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_MultipleReadableFiles) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_SingleWritableFolder) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_MultipleWritableFolders) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_WritableFilesAndFolders) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_ReadableFilesAndFolders) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_SingleReadableFolder) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_MultipleReadableFolders) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_ReadableAndWritableFolders) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FileSystemAccessUsageBubbleViewTest,
                       InvokeUi_LongOrigin) {
  ShowAndVerifyUi();
}
