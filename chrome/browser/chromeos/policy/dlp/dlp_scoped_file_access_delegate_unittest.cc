// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/dbus/dlp/fake_dlp_client.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DlpScopedFileAccessDelegateTest : public testing::Test {
 public:
  DlpScopedFileAccessDelegateTest() = default;
  ~DlpScopedFileAccessDelegateTest() override = default;

  DlpScopedFileAccessDelegateTest(const DlpScopedFileAccessDelegateTest&) =
      delete;
  DlpScopedFileAccessDelegateTest& operator=(
      const DlpScopedFileAccessDelegateTest&) = delete;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  chromeos::FakeDlpClient fake_dlp_client_;
  std::unique_ptr<DlpScopedFileAccessDelegate> delegate_{
      new DlpScopedFileAccessDelegate(&fake_dlp_client_)};
};

TEST_F(DlpScopedFileAccessDelegateTest, TestNoSingleton) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  delegate_->RequestFilesAccess({file_path}, GURL("example.com"),
                                future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate_->RequestFilesAccess({file_path}, GURL("example.com"),
                                future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, TestFileAccessSingletonForUrl) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccess({file_path}, GURL("example.com"),
                               future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());

  fake_dlp_client_.SetFileAccessAllowed(false);
  base::test::TestFuture<file_access::ScopedFileAccess> future2;
  delegate->RequestFilesAccess({file_path}, GURL("example.com"),
                               future2.GetCallback());
  EXPECT_FALSE(future2.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest,
       TestFileAccessSingletonForSystemComponent) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);

  base::test::TestFuture<file_access::ScopedFileAccess> future1;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  delegate->RequestFilesAccessForSystem({file_path}, future1.GetCallback());
  EXPECT_TRUE(future1.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, CreateFileAccessCallbackAllowTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  fake_dlp_client_.SetFileAccessAllowed(true);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, CreateFileAccessCallbackDenyTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  fake_dlp_client_.SetFileAccessAllowed(false);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  cb.Run({file_path}, future.GetCallback());
  EXPECT_FALSE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest,
       CreateFileAccessCallbackLostInstanceTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  fake_dlp_client_.SetFileAccessAllowed(false);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->CreateFileAccessCallback(GURL("https://google.com"));
  delegate_.reset();
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, GetCallbackSystemTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);

  // Post a task on IO thread to sync with to be sure the IO task setting
  // `request_files_access_for_system_io_callback_` has run.
  base::RunLoop init;
  auto io_thread = content::GetIOThreadTaskRunner({});
  io_thread->PostTask(
      FROM_HERE, base::BindOnce(&base::RunLoop::Quit, base::Unretained(&init)));
  init.Run();

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->GetCallbackForSystem();
  EXPECT_TRUE(cb);
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, GetCallbackSystemNoSingeltonTest) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);

  base::test::TestFuture<file_access::ScopedFileAccess> future;
  auto* delegate = file_access::ScopedFileAccessDelegate::Get();
  auto cb = delegate->GetCallbackForSystem();
  EXPECT_TRUE(cb);
  cb.Run({file_path}, future.GetCallback());
  EXPECT_TRUE(future.Get<0>().is_allowed());
}

TEST_F(DlpScopedFileAccessDelegateTest, TestMultipleInstances) {
  DlpScopedFileAccessDelegate::Initialize(nullptr);
  EXPECT_NO_FATAL_FAILURE(DlpScopedFileAccessDelegate::Initialize(nullptr));
}

class DlpScopedFileAccessDelegateTaskTest : public testing::Test {
 public:
  content::BrowserTaskEnvironment browser_task_environment_{
      content::BrowserTaskEnvironment::REAL_IO_THREAD};
  base::RunLoop run_loop_;
  chromeos::FakeDlpClient fake_dlp_client_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_ =
      content::GetUIThreadTaskRunner({});
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_ =
      content::GetIOThreadTaskRunner({});

  void SetUp() override {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    browser_task_environment_.RunUntilIdle();
  }

  void TestPreInit() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    ui_thread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Init,
                                  base::Unretained(this)));
  }

  void Init() {
    DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostInit,
                       base::Unretained(this)));
  }

  void TestPostInit() {
    EXPECT_TRUE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    ui_thread_->PostTask(
        FROM_HERE, base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::Delete,
                                  base::Unretained(this)));
  }

  void Delete() {
    file_access::ScopedFileAccessDelegate::DeleteInstance();
    io_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPostDelete,
                       base::Unretained(this)));
  }

  void TestPostDelete() {
    EXPECT_FALSE(
        file_access::FileAccessCopyOrMoveDelegateFactory::HasInstance());
    run_loop_.Quit();
  }
};

TEST_F(DlpScopedFileAccessDelegateTaskTest, TestSync) {
  io_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(&DlpScopedFileAccessDelegateTaskTest::TestPreInit,
                     base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestGetFilesAccessForSystemIONoInstance) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

// This test should simulate calling RequestFilesAccessForSystemIO with existing
// callback for the IO thread but destructed DlpScopedFileAccessDelegate on the
// UI thread.
TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestGetFilesAccessForSystemIODestroyedInstance) {
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  // Dlp would disallow but missing ScopedFileAccessDelegate should fall back to
  // allow.
  fake_dlp_client_.SetFileAccessAllowed(false);

  // Post a task on IO thread to sync with to be sure the IO task setting
  // `request_files_access_for_system_io_callback_` has run.
  base::RunLoop init;
  io_thread_->PostTask(
      FROM_HERE, base::BindOnce(&base::RunLoop::Quit, base::Unretained(&init)));
  init.Run();

  // Callback that calls the original
  // request_files_access_for_system_io_callback_ after destructing
  // DlpScopedFileAccessDelegate.
  file_access::ScopedFileAccessDelegate::
      ScopedRequestFilesAccessCallbackForTesting file_access_callback(
          base::BindLambdaForTesting(
              [this, &file_access_callback](
                  const std::vector<base::FilePath>& path,
                  base::OnceCallback<void(file_access::ScopedFileAccess)>
                      callback) {
                ui_thread_->PostTask(
                    FROM_HERE,
                    base::BindOnce(&file_access::ScopedFileAccessDelegate::
                                       DeleteInstance));
                file_access_callback.RunOriginalCallback(path,
                                                         std::move(callback));
              }),
          false /* = restore_original_callback*/);
  // The request for file access should be granted as that is the default
  // behaviour for no running dlp (no rules).
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestGetFilesAccessForSystemIOAllow) {
  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  fake_dlp_client_.SetFileAccessAllowed(true);
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_TRUE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

TEST_F(DlpScopedFileAccessDelegateTaskTest,
       TestGetFilesAccessForSystemIODisallow) {
  DlpScopedFileAccessDelegate::Initialize(&fake_dlp_client_);
  base::FilePath file_path;
  base::CreateTemporaryFile(&file_path);
  fake_dlp_client_.SetFileAccessAllowed(false);
  io_thread_->PostTask(
      FROM_HERE, base::BindLambdaForTesting([this, &file_path]() {
        file_access::ScopedFileAccessDelegate::RequestFilesAccessForSystemIO(
            {file_path}, base::BindLambdaForTesting(
                             [this](file_access::ScopedFileAccess file_access) {
                               DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
                               EXPECT_FALSE(file_access.is_allowed());
                               run_loop_.Quit();
                             }));
      }));
  run_loop_.Run();
}

}  // namespace policy
