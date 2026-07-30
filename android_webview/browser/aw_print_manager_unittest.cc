// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_print_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/web_contents_tester.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class PrintManagerPeer : public printing::PrintManager {
 public:
  static int GetCookie(printing::PrintManager* manager) {
    return static_cast<PrintManagerPeer*>(manager)->cookie();
  }
};

class AwPrintManagerTest : public testing::Test {
 public:
  AwPrintManagerTest() = default;
  ~AwPrintManagerTest() override = default;

  void SetUp() override {
    test_content_client_initializer_ =
        std::make_unique<content::TestContentClientInitializer>();
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        browser_context_.get(), nullptr);
    ASSERT_TRUE(web_contents_)
        << "WebContentsTester::CreateTestWebContents returned null!";
    AwPrintManager::CreateForWebContents(web_contents_.get());
  }

  void TearDown() override {
    web_contents_.reset();
    browser_context_.reset();
    test_content_client_initializer_.reset();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;

 private:
  std::unique_ptr<content::TestContentClientInitializer>
      test_content_client_initializer_;
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<content::WebContents> web_contents_;
};

TEST_F(AwPrintManagerTest, FdIsResetAfterSuccessfulPrint) {
  auto* print_manager = AwPrintManager::FromWebContents(web_contents());
  ASSERT_TRUE(print_manager);

  int fds[2];
  ASSERT_EQ(0, pipe2(fds, O_CLOEXEC));
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);

  auto settings = std::make_unique<printing::PrintSettings>();

  bool callback_called = false;
  print_manager->UpdateParam(
      std::move(settings), std::move(write_fd),
      base::BindLambdaForTesting([&callback_called](int page_count) {
        callback_called = true;
        EXPECT_EQ(1, page_count);
      }));

  auto params = printing::mojom::DidPrintDocumentParams::New();
  params->document_cookie = PrintManagerPeer::GetCookie(print_manager);

  auto content = printing::mojom::DidPrintContentParams::New();
  const uint8_t test_data[] = "test print data";
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(test_data));
  ASSERT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(test_data);

  content->metafile_data_region = std::move(mapped_region.region);
  params->content = std::move(content);

  auto* host = static_cast<printing::mojom::PrintManagerHost*>(print_manager);

  host->DidGetPrintedPagesCount(PrintManagerPeer::GetCookie(print_manager), 1);

  base::RunLoop run_loop;
  bool did_print_callback_called = false;

  host->DidPrintDocument(
      std::move(params),
      base::BindLambdaForTesting(
          [&did_print_callback_called,
           quit_closure = run_loop.QuitClosure()](bool success) mutable {
            did_print_callback_called = true;
            EXPECT_TRUE(success);
            std::move(quit_closure).Run();
          }));

  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(did_print_callback_called);

  // Verify that the data was written to the pipe successfully.
  std::string read_buf;
  char buf[128];
  ssize_t bytes_read;
  while ((bytes_read = read(read_fd.get(), buf, sizeof(buf))) > 0) {
    read_buf.append(buf, bytes_read);
  }
  EXPECT_EQ(read_buf, std::string(reinterpret_cast<const char*>(test_data),
                                  sizeof(test_data)));
}

TEST_F(AwPrintManagerTest,
       FdLifecycleManagedByBackgroundTaskEvenIfManagerDestroyed) {
  auto* print_manager = AwPrintManager::FromWebContents(web_contents());
  ASSERT_TRUE(print_manager);

  int fds[2];
  ASSERT_EQ(0, pipe2(fds, O_CLOEXEC));
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);
  ASSERT_TRUE(base::SetNonBlocking(read_fd.get()));

  auto settings = std::make_unique<printing::PrintSettings>();

  bool callback_called = false;
  print_manager->UpdateParam(
      std::move(settings), std::move(write_fd),
      base::BindLambdaForTesting(
          [&callback_called](int page_count) { callback_called = true; }));

  auto params = printing::mojom::DidPrintDocumentParams::New();
  params->document_cookie = PrintManagerPeer::GetCookie(print_manager);

  auto content = printing::mojom::DidPrintContentParams::New();
  const uint8_t test_data[] = "test data for lifecycle test";
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(sizeof(test_data));
  ASSERT_TRUE(mapped_region.IsValid());
  mapped_region.mapping.GetMemoryAsSpan<uint8_t>().copy_from(test_data);

  content->metafile_data_region = std::move(mapped_region.region);
  params->content = std::move(content);

  auto* host = static_cast<printing::mojom::PrintManagerHost*>(print_manager);

  host->DidGetPrintedPagesCount(PrintManagerPeer::GetCookie(print_manager), 1);

  host->DidPrintDocument(std::move(params),
                         base::BindLambdaForTesting([](bool success) {
                           ADD_FAILURE() << "Callback should not run";
                         }));

  // Destroy the print manager before running the background task.
  TearDown();

  // Since the print manager is destroyed, the completion callback will not run.
  // We use RunUntil to wait for the background file writing task to complete.
  // Note: RunUntilIdle() is a banned pattern and cannot be used here.
  std::string read_buf;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    char buf[128];
    ssize_t bytes_read = read(read_fd.get(), buf, sizeof(buf));
    if (bytes_read > 0) {
      read_buf.append(buf, bytes_read);
    }
    return read_buf == std::string(reinterpret_cast<const char*>(test_data),
                                   sizeof(test_data));
  }));
}

// Verifies that when DidPrintDocument fails early (e.g., due to a cookie
// mismatch), the file descriptor is properly closed. This also implicitly
// verifies that the print manager resets/extracts the fd_ when printing starts,
// because PdfWritingDone(0) CHECKs that fd_ is invalid.
TEST_F(AwPrintManagerTest, FdIsClosedOnCookieMismatch) {
  auto* print_manager = AwPrintManager::FromWebContents(web_contents());
  ASSERT_TRUE(print_manager);

  int fds[2];
  ASSERT_EQ(0, pipe2(fds, O_CLOEXEC));
  base::ScopedFD read_fd(fds[0]);
  base::ScopedFD write_fd(fds[1]);
  ASSERT_TRUE(base::SetNonBlocking(read_fd.get()));

  auto settings = std::make_unique<printing::PrintSettings>();

  bool callback_called = false;
  print_manager->UpdateParam(
      std::move(settings), std::move(write_fd),
      base::BindLambdaForTesting([&callback_called](int page_count) {
        callback_called = true;
        EXPECT_EQ(0, page_count);
      }));

  auto params = printing::mojom::DidPrintDocumentParams::New();
  params->document_cookie =
      PrintManagerPeer::GetCookie(print_manager) + 1;  // Wrong cookie
  params->content = printing::mojom::DidPrintContentParams::New();

  auto* host = static_cast<printing::mojom::PrintManagerHost*>(print_manager);

  bool did_print_callback_called = false;
  host->DidPrintDocument(
      std::move(params),
      base::BindLambdaForTesting([&did_print_callback_called](bool success) {
        did_print_callback_called = true;
        EXPECT_FALSE(success);
      }));

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(did_print_callback_called);

  // The write_fd should be closed now due to cookie mismatch, meaning a read
  // on read_fd should return 0 (EOF).
  char buf;
  EXPECT_EQ(0, read(read_fd.get(), &buf, 1));
}

}  // namespace android_webview
