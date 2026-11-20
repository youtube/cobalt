// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/cpp/temporary_file_getter.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "base/files/file_enumerator.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_path_override.h"
#include "base/base_paths.h"
namespace safe_browsing {

class TemporaryFileGetterTest : public ::testing::Test {
 public:
  TemporaryFileGetterTest();

  content::BrowserTaskEnvironment task_environment_;
  base::File temp_file_;
  TemporaryFileGetter temp_file_getter_;
};

void UpdateTempFile(base::File* temp_file_, base::File temp_file) {
  *temp_file_ = std::move(temp_file);
}

TemporaryFileGetterTest::TemporaryFileGetterTest() = default;

TEST_F(TemporaryFileGetterTest, GetTempFileTest) {
  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(temp_file_.IsValid());
}

TEST_F(TemporaryFileGetterTest, ExceedFileLimitTest) {
  // Get the allowed 10 files from the TemporaryFileGetter.
  for (int i = 0; i < 10; i++) {
    auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
    temp_file_getter_.RequestTemporaryFile(std::move(callback));
    task_environment_.RunUntilIdle();
    EXPECT_TRUE(temp_file_.IsValid());
  }
  // After 10 files, the TemporaryFileGetter returns an invalid file.
  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(temp_file_.IsValid());
}

TEST_F(TemporaryFileGetterTest, GetTempFileWriteReadTest) {
  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_file_.IsValid());

  constexpr char kTestData[] = "Some test data to write to temporary file getter";
  std::optional<size_t> bytes_written =
      temp_file_.WriteAtCurrentPos(base::as_byte_span(std::string_view(kTestData)));
  ASSERT_TRUE(bytes_written.has_value());
  EXPECT_EQ(std::size(kTestData) - 1, bytes_written.value());

  ASSERT_TRUE(temp_file_.Seek(base::File::FROM_BEGIN, 0) == 0);

  char read_buffer[sizeof(kTestData)] = {0};
  std::optional<size_t> bytes_read =
      temp_file_.ReadAtCurrentPos(base::as_writable_byte_span(read_buffer).first(bytes_written.value()));
  ASSERT_TRUE(bytes_read.has_value());
  EXPECT_EQ(bytes_written.value(), bytes_read.value());
  EXPECT_EQ(std::string_view(kTestData), std::string_view(read_buffer, bytes_read.value()));
}

TEST_F(TemporaryFileGetterTest, TempFileDeletedWhenClosed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::ScopedPathOverride path_override(base::DIR_TEMP, temp_dir.GetPath());

  auto callback = base::BindOnce(&UpdateTempFile, &temp_file_);
  temp_file_getter_.RequestTemporaryFile(std::move(callback));
  task_environment_.RunUntilIdle();
  ASSERT_TRUE(temp_file_.IsValid());

  // Close the file. This should trigger deletion on Windows, and on POSIX it
  // was already deleted when created.
  temp_file_.Close();

  base::FileEnumerator enumerator(temp_dir.GetPath(), false,
                                  base::FileEnumerator::FILES);
  int file_count = 0;
  for (base::FilePath name = enumerator.Next(); !name.empty();
       name = enumerator.Next()) {
    file_count++;
  }
  EXPECT_EQ(0, file_count);
}

}  // namespace safe_browsing
