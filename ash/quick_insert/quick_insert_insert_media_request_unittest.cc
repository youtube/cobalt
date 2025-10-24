// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_insert_media_request.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/quick_insert/quick_insert_rich_media.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "url/gurl.h"

namespace ash {
namespace {

// Any arbitrary insertion timeout.
constexpr base::TimeDelta kInsertionTimeout = base::Seconds(1);

class TestCase {
 public:
  virtual ~TestCase() = default;

  // The media to insert.
  virtual const QuickInsertRichMedia& media_to_insert() = 0;

  // The expected text in the input field if the insertion was successful.
  virtual std::u16string_view expected_text() = 0;

  // The expected image in the input field if the insertion was successful.
  // Can be nullptr.
  virtual std::optional<GURL> expected_image_url() = 0;
};

// Both `testing::Values` and `testing::ValuesIn` do not support move-only
// types. Work around this by passing `RepeatingCallbacks`, which are copyable.
using TestCaseCallback = base::RepeatingCallback<std::unique_ptr<TestCase>()>;

class BasicTestCase : public TestCase {
 public:
  BasicTestCase(QuickInsertRichMedia media_to_insert,
                std::u16string expected_text)
      : media_to_insert_(std::move(media_to_insert)),
        expected_text_(std::move(expected_text)) {}

  BasicTestCase(QuickInsertRichMedia media_to_insert, GURL expected_image_url)
      : media_to_insert_(std::move(media_to_insert)),
        expected_image_url_(std::move(expected_image_url)) {}

  TestCaseCallback ToCallback() && {
    return base::BindRepeating(
        [](const BasicTestCase& test_case) -> std::unique_ptr<TestCase> {
          return std::make_unique<BasicTestCase>(test_case);
        },
        std::move(*this));
  }

  const QuickInsertRichMedia& media_to_insert() override {
    return media_to_insert_;
  }

  std::u16string_view expected_text() override { return expected_text_; }

  std::optional<GURL> expected_image_url() override {
    return expected_image_url_;
  }

 private:
  QuickInsertRichMedia media_to_insert_;
  std::u16string expected_text_;
  std::optional<GURL> expected_image_url_;
};

class LocalImageTestCase : public TestCase {
 public:
  using EncodeCallback =
      base::RepeatingCallback<std::optional<std::vector<uint8_t>>(
          const SkBitmap& bitmap)>;

  LocalImageTestCase(std::string format, EncodeCallback encode)
      : format_(std::move(format)),
        media_(QuickInsertLocalFileMedia(base::FilePath())) {
    CHECK(temp_dir_.CreateUniqueTempDir()) << "Could not create temp dir";

    SkBitmap bitmap = gfx::test::CreateBitmap(1);
    std::optional<std::vector<uint8_t>> encode_bytes = encode.Run(bitmap);
    CHECK(encode_bytes) << "Encoding bitmap failed";
    image_bytes_ = std::move(encode_bytes).value();

    base::FilePath path = temp_dir_.GetPath().Append(
        base::FilePath("test_image").AddExtensionASCII(format_));
    base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    CHECK(file.WriteAndCheck(0, image_bytes_))
        << "Writing to " << path << " failed";

    CHECK_DEREF(std::get_if<QuickInsertLocalFileMedia>(&media_)).path = path;
  }

  const QuickInsertRichMedia& media_to_insert() override { return media_; }

  std::u16string_view expected_text() override { return u""; }

  std::optional<GURL> expected_image_url() override {
    return GURL(base::StrCat({"data:image/", format_, ";base64,",
                              base::Base64Encode(image_bytes_)}));
  }

 private:
  std::string format_;

  base::ScopedTempDir temp_dir_;
  std::vector<uint8_t> image_bytes_;

  QuickInsertRichMedia media_;
};

TestCaseCallback MakeLocalImageTestCaseCallback(
    std::string format,
    LocalImageTestCase::EncodeCallback encode) {
  return base::BindRepeating(
      [](const std::string& format, LocalImageTestCase::EncodeCallback encode)
          -> std::unique_ptr<TestCase> {
        return std::make_unique<LocalImageTestCase>(format, encode);
      },
      std::move(format), std::move(encode));
}

class QuickInsertInsertMediaRequestTest
    : public testing::TestWithParam<TestCaseCallback> {
 protected:
  QuickInsertInsertMediaRequestTest() : test_case_(GetParam().Run()) {}

  std::unique_ptr<TestCase>& test_case() { return test_case_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  std::unique_ptr<TestCase> test_case_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

class QuickInsertInsertMediaRequestImageTest
    : public QuickInsertInsertMediaRequestTest {};

const TestCaseCallback kTextTestCases[] = {
    BasicTestCase(
        /*media_to_insert=*/QuickInsertTextMedia(u"hello"),
        /*expected_text=*/u"hello")
        .ToCallback(),
    BasicTestCase(
        /*media_to_insert=*/QuickInsertLinkMedia(GURL("http://foo.com"), "Foo"),
        /*expected_text=*/u"http://foo.com/")
        .ToCallback(),
};

const TestCaseCallback kImageTestCases[] = {
    BasicTestCase(
        /*media_to_insert=*/QuickInsertImageMedia(
            GURL("http://foo.com/fake.jpg"),
            gfx::Size(10, 10)),
        /*expected_image_url=*/GURL("http://foo.com/fake.jpg"))
        .ToCallback(),
    MakeLocalImageTestCaseCallback(
        "png",
        base::BindRepeating([](const SkBitmap& bitmap) {
          return gfx::PNGCodec::EncodeBGRASkBitmap(
              bitmap, /*discard_transparency=*/false);
        })),
    MakeLocalImageTestCaseCallback(
        "jpeg",
        base::BindRepeating([](const SkBitmap& bitmap) {
          return gfx::JPEGCodec::Encode(bitmap, /*quality=*/80);
        })),
    MakeLocalImageTestCaseCallback(
        "webp",
        base::BindRepeating([](const SkBitmap& bitmap) {
          return gfx::WebpCodec::Encode(bitmap, /*quality=*/80);
        }))};

INSTANTIATE_TEST_SUITE_P(Text,
                         QuickInsertInsertMediaRequestTest,
                         testing::ValuesIn(kTextTestCases));

INSTANTIATE_TEST_SUITE_P(Image,
                         QuickInsertInsertMediaRequestTest,
                         testing::ValuesIn(kImageTestCases));

INSTANTIATE_TEST_SUITE_P(,
                         QuickInsertInsertMediaRequestImageTest,
                         testing::ValuesIn(kImageTestCases));

TEST_P(QuickInsertInsertMediaRequestTest, DoesNotInsertWhenBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
  EXPECT_EQ(client.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertsWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  EXPECT_EQ(client.text(), test_case()->expected_text());
  EXPECT_EQ(client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest,
       InsertsOnNextFocusBeforeTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Milliseconds(999));
  EXPECT_EQ(client.text(), test_case()->expected_text());
  EXPECT_EQ(client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest,
       DoesNotInsertAfterTimeoutWhileBlurred) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
  EXPECT_EQ(client.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertsOnNextFocusWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), test_case()->expected_text());
  EXPECT_EQ(next_client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest,
       InsertsOnNextFocusBeforeTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  task_environment().FastForwardBy(base::Milliseconds(999));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Milliseconds(999));
  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), test_case()->expected_text());
  EXPECT_EQ(next_client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest,
       DoesNotInsertOnNextFocusAfterTimeoutWhileFocused) {
  ui::FakeTextInputClient prev_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient next_client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);
  input_method.SetFocusedTextInputClient(&prev_client);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  task_environment().FastForwardBy(base::Seconds(1));
  input_method.SetFocusedTextInputClient(&next_client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
  EXPECT_EQ(prev_client.text(), u"");
  EXPECT_EQ(next_client.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertIsCancelledUponDestruction) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  {
    QuickInsertInsertMediaRequest request(
        &input_method, test_case()->media_to_insert(), kInsertionTimeout);
    // TODO: b/328655564 - Call `on_complete_callback` if the request was
    // cancelled.
  }
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(client.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, DoesNotInsertInInputTypeNone) {
  ui::FakeTextInputClient client_none(ui::TEXT_INPUT_TYPE_NONE);
  ui::FakeTextInputClient client_text(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  input_method.SetFocusedTextInputClient(&client_none);
  input_method.SetFocusedTextInputClient(&client_text);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  EXPECT_EQ(client_none.text(), u"");
  EXPECT_EQ(client_text.text(), test_case()->expected_text());
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertsOnlyOnceWithMultipleFocus) {
  ui::FakeTextInputClient client1(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  ui::FakeTextInputClient client2(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  input_method.SetFocusedTextInputClient(&client1);
  input_method.SetFocusedTextInputClient(&client2);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  EXPECT_EQ(client1.text(), test_case()->expected_text());
  EXPECT_EQ(client2.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertsOnlyOnceWithTimeout) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  task_environment().FastForwardBy(base::Seconds(1));

  EXPECT_EQ(client.text(), test_case()->expected_text());
  EXPECT_EQ(client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest, InsertsOnlyOnceWithDestruction) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  InputMethodAsh input_method(nullptr);

  {
    base::test::TestFuture<QuickInsertInsertMediaRequest::Result>
        complete_future;
    base::TimeTicks before_insert = task_environment().NowTicks();

    QuickInsertInsertMediaRequest request(
        &input_method, test_case()->media_to_insert(), kInsertionTimeout,
        /*get_web_paste_target=*/{}, complete_future.GetCallback());
    input_method.SetFocusedTextInputClient(&client);

    EXPECT_EQ(complete_future.Take(),
              QuickInsertInsertMediaRequest::Result::kSuccess);
    base::TimeDelta insert_duration =
        task_environment().NowTicks() - before_insert;
    EXPECT_EQ(insert_duration, base::Seconds(0));
  }

  EXPECT_EQ(client.text(), test_case()->expected_text());
  EXPECT_EQ(client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestTest,
       DoesNotInsertWhenInputMethodIsDestroyed) {
  ui::FakeTextInputClient client(
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});
  auto old_input_method = std::make_unique<InputMethodAsh>(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      old_input_method.get(), test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  old_input_method.reset();
  InputMethodAsh new_input_method(nullptr);
  new_input_method.SetFocusedTextInputClient(&client);

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
  EXPECT_EQ(client.text(), u"");
}

TEST_P(QuickInsertInsertMediaRequestTest, CallsCallbackOnSuccess) {
  InputMethodAsh input_method(nullptr);
  ui::FakeTextInputClient client(
      &input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  client.Focus();

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
}

TEST_P(QuickInsertInsertMediaRequestTest, CallsFailureCallbackOnTimeout) {
  InputMethodAsh input_method(nullptr);

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
}

TEST_P(QuickInsertInsertMediaRequestImageTest,
       InsertingImageIgnoresUnsupportedClients) {
  InputMethodAsh input_method(nullptr);
  ui::FakeTextInputClient unsupported_client(
      &input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});
  ui::FakeTextInputClient supported_client(
      &input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = true});

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(), kInsertionTimeout,
      /*get_web_paste_target=*/{}, complete_future.GetCallback());
  unsupported_client.Focus();
  supported_client.Focus();

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kSuccess);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(0));
  EXPECT_EQ(unsupported_client.last_inserted_image_url(), std::nullopt);
  EXPECT_EQ(supported_client.last_inserted_image_url(),
            test_case()->expected_image_url());
}

TEST_P(QuickInsertInsertMediaRequestImageTest,
       InsertingUnsupportedImageFailsAfterTimeout) {
  InputMethodAsh input_method(nullptr);
  ui::FakeTextInputClient client(
      &input_method,
      {.type = ui::TEXT_INPUT_TYPE_TEXT, .can_insert_image = false});

  base::test::TestFuture<QuickInsertInsertMediaRequest::Result> complete_future;
  base::TimeTicks before_insert = task_environment().NowTicks();
  QuickInsertInsertMediaRequest request(
      &input_method, test_case()->media_to_insert(),
      /*insert_timeout=*/base::Seconds(1), /*get_web_paste_target=*/{},
      complete_future.GetCallback());
  client.Focus();

  EXPECT_EQ(complete_future.Take(),
            QuickInsertInsertMediaRequest::Result::kTimeout);
  base::TimeDelta insert_duration =
      task_environment().NowTicks() - before_insert;
  EXPECT_EQ(insert_duration, base::Seconds(1));
  EXPECT_EQ(client.last_inserted_image_url(), std::nullopt);
}

}  // namespace
}  // namespace ash
