// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/media_galleries/fileapi/media_file_system_backend.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "storage/browser/file_system/copy_or_move_file_validator.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const char kOrigin[] = "http://foo";

const char kValidImage[] = "RIFF0\0\0\0WEBPVP8 $\0\0\0\xB2\x02\0\x9D\x01\x2A"
                           "\x01\0\x01\0\x2F\x9D\xCE\xE7s\xA8((((\x01\x9CK(\0"
                           "\x05\xCE\xB3l\0\0\xFE\xD8\x80\0\0";

const char kInvalidMediaFile[] = "Not a media file";

const int64_t kNoFileSize = -1;

void HandleCheckFileResult(int64_t expected_size,
                           base::OnceCallback<void(bool success)> callback,
                           base::File::Error result,
                           const base::File::Info& file_info) {
  if (result == base::File::FILE_OK) {
    if (!file_info.is_directory && expected_size != kNoFileSize &&
        file_info.size == expected_size) {
      std::move(callback).Run(true);
      return;
    }
  }

  std::move(callback).Run(expected_size == kNoFileSize);
}

base::FilePath GetMediaTestDir() {
  base::FilePath test_file;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &test_file))
    return base::FilePath();
  return test_file.AppendASCII("media").AppendASCII("test").AppendASCII("data");
}

}  // namespace

class MediaFileValidatorTest : public InProcessBrowserTest {
 public:
  MediaFileValidatorTest() : test_file_size_(0) {}

  MediaFileValidatorTest(const MediaFileValidatorTest&) = delete;
  MediaFileValidatorTest& operator=(const MediaFileValidatorTest&) = delete;

  ~MediaFileValidatorTest() override = default;

  // Write |content| into |filename| in a test file system and try to move
  // it into a media file system.  The result is compared to |expected_result|.
  void MoveTest(const std::string& filename, const std::string& content,
                bool expected_result) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MediaFileValidatorTest::SetupBlocking,
                       base::Unretained(this), filename, content,
                       expected_result));
    run_loop.Run();
  }

  // Write |source| into |filename| in a test file system and try to move it
  // into a media file system.  The result is compared to |expected_result|.
  void MoveTestFromFile(const std::string& filename,
                        const base::FilePath& source, bool expected_result) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&MediaFileValidatorTest::SetupFromFileBlocking,
                       base::Unretained(this), filename, source,
                       expected_result));
    run_loop.Run();
  }

 private:
  // BrowserTestBase interface.
  void PostRunTestOnMainThread() override {
    // Trigger release of the FileSystemContext before the IO thread is gone,
    // so it can teardown there correctly.
    file_system_context_ = nullptr;
    InProcessBrowserTest::PostRunTestOnMainThread();
  }

  // Create the test files, filesystem objects, etc.
  void SetupBlocking(const std::string& filename,
                     const std::string& content,
                     bool expected_result) {
    ASSERT_TRUE(base_dir_.CreateUniqueTempDir());
    base::FilePath base = base_dir_.GetPath();
    base::FilePath src_path = base.AppendASCII("src_fs");
    ASSERT_TRUE(base::CreateDirectory(src_path));

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    file_system_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});
    additional_providers.push_back(
        std::make_unique<storage::TestFileSystemBackend>(
            file_system_runner_.get(), src_path));
    additional_providers.push_back(
        std::make_unique<MediaFileSystemBackend>(base, base::NullCallback()));
    file_system_context_ =
        storage::CreateFileSystemContextWithAdditionalProvidersForTesting(
            content::GetIOThreadTaskRunner({}), file_system_runner_,
            /*quota_manager_proxy=*/nullptr, std::move(additional_providers),
            base);

    move_src_ = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting(kOrigin),
        storage::kFileSystemTypeTest, base::FilePath::FromUTF8Unsafe(filename));

    test_file_size_ = content.size();
    base::FilePath test_file = src_path.AppendASCII(filename);
    ASSERT_TRUE(base::WriteFile(test_file, content));

    base::FilePath dest_path = base.AppendASCII("dest_fs");
    ASSERT_TRUE(base::CreateDirectory(dest_path));
    dest_fs_ =
        storage::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
            storage::kFileSystemTypeLocalMedia, std::string(), dest_path,
            nullptr);

    size_t extension_index = filename.find_last_of(".");
    ASSERT_NE(std::string::npos, extension_index);
    std::string extension = filename.substr(extension_index);
    std::string dest_root_fs_url = storage::GetIsolatedFileSystemRootURIString(
        GURL(kOrigin), dest_fs_.id(), "dest_fs/");
    const GURL crack_url = GURL(dest_root_fs_url + "move_dest" + extension);
    move_dest_ = file_system_context_->CrackURLInFirstPartyContext(crack_url);

    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaFileValidatorTest::CheckFiles, base::Unretained(this), true,
            base::BindOnce(&MediaFileValidatorTest::OnTestFilesReady,
                           base::Unretained(this), expected_result)));
  }

  void SetupFromFileBlocking(const std::string& filename,
                             const base::FilePath& source,
                             bool expected_result) {
    std::string content;
    ASSERT_TRUE(base::ReadFileToString(source, &content));
    SetupBlocking(filename, content, expected_result);
  }

  // Check that exactly one of |move_src_| and |move_dest_| exists.
  // |src_expected| indicates which one should exist.  When complete,
  // |callback| is called with success/failure.
  void CheckFiles(bool src_expected,
                  base::OnceCallback<void(bool success)> callback) {
    CheckFile(move_src_, src_expected ? test_file_size_ : kNoFileSize,
              base::BindOnce(&MediaFileValidatorTest::OnCheckFilesFirstResult,
                             base::Unretained(this), !src_expected,
                             std::move(callback)));
  }

  // Helper that checks a file has the |expected_size|, which may be
  // |kNoFileSize| if the file should not exist.  |callback| is called
  // with success/failure.
  void CheckFile(storage::FileSystemURL url,
                 int64_t expected_size,
                 base::OnceCallback<void(bool success)> callback) {
    operation_runner()->GetMetadata(
        url, storage::FileSystemOperation::GET_METADATA_FIELD_SIZE,
        base::BindOnce(&HandleCheckFileResult, expected_size,
                       std::move(callback)));
  }

  // Helper that checks the result of |move_src_| lookup and then checks
  // |move_dest_| if all is as expected.
  void OnCheckFilesFirstResult(bool dest_expected,
                               base::OnceCallback<void(bool)> callback,
                               bool src_result) {
    EXPECT_TRUE(src_result);
    if (!src_result) {
      std::move(callback).Run(false);
      return;
    }
    CheckFile(move_dest_, dest_expected ? test_file_size_ : kNoFileSize,
              std::move(callback));
  }

  // Assert |test_files_ready| and then do the actual test of moving
  // |move_src_| to |move_dest_|.
  void OnTestFilesReady(bool expected_result, bool test_files_ready) {
    ASSERT_TRUE(test_files_ready);
    operation_runner()->Move(
        move_src_, move_dest_,
        storage::FileSystemOperation::CopyOrMoveOptionSet(),
        storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
        std::make_unique<storage::CopyOrMoveHookDelegate>(),
        base::BindOnce(&MediaFileValidatorTest::OnMoveResult,
                       base::Unretained(this), expected_result));
  }

  // Check that the move succeeded/failed based on expectation and then
  // check that the right file exists.
  void OnMoveResult(bool expected_result, base::File::Error result) {
    if (expected_result)
      EXPECT_EQ(base::File::FILE_OK, result);
    else
      EXPECT_EQ(base::File::FILE_ERROR_SECURITY, result);
    CheckFiles(!expected_result,
               base::BindOnce(&MediaFileValidatorTest::OnTestFilesCheckResult,
                              base::Unretained(this)));
  }

  // Check that the correct test file exists and then allow the main-thread
  // RunLoop to quit.
  void OnTestFilesCheckResult(bool result) {
    EXPECT_TRUE(result);
    std::move(quit_closure_).Run();
  }

  storage::FileSystemOperationRunner* operation_runner() {
    return file_system_context_->operation_runner();
  }

  base::ScopedTempDir base_dir_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  int test_file_size_;

  storage::FileSystemURL move_src_;
  storage::FileSystemURL move_dest_;
  storage::IsolatedContext::ScopedFSHandle dest_fs_;

  base::OnceClosure quit_closure_;
  scoped_refptr<base::SequencedTaskRunner> file_system_runner_;
};

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, UnsupportedExtension) {
  MoveTest("a.txt", std::string(kValidImage, std::size(kValidImage)), false);
}

// TODO(crbug.com/1169640): Re-enable. Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_ValidImage DISABLED_ValidImage
#else
#define MAYBE_ValidImage ValidImage
#endif
IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, MAYBE_ValidImage) {
  MoveTest("a.webp", std::string(kValidImage, std::size(kValidImage)), true);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidImage) {
  MoveTest("a.webp",
           std::string(kInvalidMediaFile, std::size(kInvalidMediaFile)), false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidAudio) {
  MoveTest("a.ogg",
           std::string(kInvalidMediaFile, std::size(kInvalidMediaFile)), false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, ValidAudio) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("sfx.ogg");
  MoveTestFromFile("sfx.ogg", test_file, true);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, InvalidVideo) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("no_streams.webm");
  MoveTestFromFile("no_streams.webm", test_file, false);
}

IN_PROC_BROWSER_TEST_F(MediaFileValidatorTest, ValidVideo) {
  base::FilePath test_file = GetMediaTestDir();
  ASSERT_FALSE(test_file.empty());
  test_file = test_file.AppendASCII("bear-320x240-multitrack.webm");
  MoveTestFromFile("multitrack.webm", test_file, true);
}
