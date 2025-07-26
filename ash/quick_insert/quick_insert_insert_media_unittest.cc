// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_insert_media.h"

#include <optional>
#include <string>

#include "ash/quick_insert/quick_insert_rich_media.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/fake_text_input_client.h"

namespace ash {
namespace {

class ScopedTestFile {
 public:
  bool Create(std::string_view file_name, std::string_view contents) {
    if (!temp_dir_.CreateUniqueTempDir()) {
      return false;
    }
    path_ = temp_dir_.GetPath().Append(file_name);
    if (!base::WriteFile(path_, contents)) {
      return false;
    }
    return true;
  }

  const base::FilePath& path() const { return path_; }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath path_;
};

struct TestCase {
  // The media to insert.
  QuickInsertRichMedia media_to_insert;

  // The expected text in the input field if the insertion was successful.
  std::u16string expected_text;

  // The expected image in the input field if the insertion was successful.
  std::optional<GURL> expected_image_url;
};

using QuickInsertInsertMediaTest = testing::TestWithParam<TestCase>;

TEST_P(QuickInsertInsertMediaTest, SupportsInsertingMedia) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  EXPECT_TRUE(
      InputFieldSupportsInsertingMedia(GetParam().media_to_insert, client));
}

TEST_P(QuickInsertInsertMediaTest, InsertsMediaWithNoError) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(GetParam().media_to_insert, client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kSuccess);
  EXPECT_EQ(client.text(), GetParam().expected_text);
  EXPECT_EQ(client.last_inserted_image_url(), GetParam().expected_image_url);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    QuickInsertInsertMediaTest,
    testing::Values(
        TestCase{
            .media_to_insert = QuickInsertTextMedia(u"hello"),
            .expected_text = u"hello",
        },
        TestCase{
            .media_to_insert =
                QuickInsertImageMedia(GURL("http://foo.com/fake.jpg"),
                                      gfx::Size(10, 10)),
            .expected_image_url = GURL("http://foo.com/fake.jpg"),
        },
        TestCase{
            .media_to_insert = QuickInsertLinkMedia(GURL("http://foo.com"),
                                                    "foo"),
            .expected_text = u"http://foo.com/",
        }));

TEST(QuickInsertInsertImageMediaTest, UnsupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  EXPECT_FALSE(InputFieldSupportsInsertingMedia(
      QuickInsertImageMedia(GURL("http://foo.com"), gfx::Size(10, 10)),
      client));
}

TEST(QuickInsertInsertImageMediaTest,
     InsertingUnsupportedInputFieldFailsAsynchronously) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(
      QuickInsertImageMedia(GURL("http://foo.com"), gfx::Size(10, 10)), client,
      /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(QuickInsertInsertLocalFileMediaTest, SupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  EXPECT_TRUE(InputFieldSupportsInsertingMedia(
      QuickInsertLocalFileMedia(base::FilePath("foo.txt")), client));
}

TEST(QuickInsertInsertLocalFileMediaTest, UnsupportedInputField) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  EXPECT_FALSE(InputFieldSupportsInsertingMedia(
      QuickInsertLocalFileMedia(base::FilePath("foo.txt")), client));
}

TEST(QuickInsertInsertLocalFileMediaTest, InsertsAsynchronously) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.png", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(QuickInsertLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kSuccess);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(),
            GURL("data:image/png;base64,VGVzdCBkYXRh"));
}

TEST(QuickInsertInsertLocalFileMediaTest,
     InsertingInUnsupportedClientReturnsError) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.png", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(QuickInsertLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(QuickInsertInsertLocalFileMediaTest,
     InsertingUnsupportedMediaTypeReturnsError) {
  base::test::TaskEnvironment task_environment;
  ScopedTestFile file;
  ASSERT_TRUE(file.Create("foo.meow", "Test data"));
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(QuickInsertLocalFileMedia(file.path()), client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kUnsupported);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

TEST(QuickInsertInsertLocalFileMediaTest,
     InsertingNonExistentFileReturnsError) {
  base::test::TaskEnvironment task_environment;
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<InsertMediaResult> future;
  InsertMediaToInputField(QuickInsertLocalFileMedia(base::FilePath("foo.txt")),
                          client,
                          /*get_web_paste_target=*/{}, future.GetCallback());

  EXPECT_EQ(future.Get(), InsertMediaResult::kNotFound);
  EXPECT_EQ(client.text(), u"");
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

}  // namespace
}  // namespace ash
