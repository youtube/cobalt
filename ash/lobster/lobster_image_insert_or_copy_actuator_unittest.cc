// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_image_insert_or_copy_actuator.h"

#include <string>

#include "ash/public/cpp/system/toast_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/base64.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/ime/ash/input_method_ash.h"
#include "ui/base/ime/fake_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "url/gurl.h"
namespace ash {

namespace {

std::u16string ReadHTMLFromClipboard(ui::Clipboard* clipboard) {
  std::u16string markup;
  std::string url;
  uint32_t start, end;

  clipboard->ReadHTML(ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/nullptr,
                      &markup, &url, &start, &end);
  return markup;
}

class LobsterImageInsertOrCopyActuatorTest : public AshTestBase {
 public:
  LobsterImageInsertOrCopyActuatorTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ui::InputMethod& ime() { return ime_; }

 private:
  InputMethodAsh ime_{nullptr};
};

TEST_F(LobsterImageInsertOrCopyActuatorTest,
       CanInsertImageIntoEligibleTextInputField) {
  ui::FakeTextInputClient text_input_client(&ime(), {.can_insert_image = true});

  EXPECT_TRUE(InsertImageOrCopyToClipboard(&text_input_client,
                                           /*image_bytes=*/"a1b2c3"));

  EXPECT_EQ(text_input_client.last_inserted_image_url(),
            GURL(base::StrCat(
                {"data:image/jpeg;base64,", base::Base64Encode("a1b2c3")})));
  EXPECT_FALSE(ash::ToastManager::Get()->IsToastShown("lobster_toast"));
}

TEST_F(LobsterImageInsertOrCopyActuatorTest,
       FallbackToCopyToClipboardInIneligibleTextInputField) {
  ui::FakeTextInputClient text_input_client(&ime(),
                                            {.can_insert_image = false});

  EXPECT_FALSE(InsertImageOrCopyToClipboard(&text_input_client,
                                            /*image_bytes=*/"a1b2c3"));

  EXPECT_EQ(text_input_client.last_inserted_image_url(), std::nullopt);
  EXPECT_EQ(
      ReadHTMLFromClipboard(ui::Clipboard::GetForCurrentThread()),
      base::StrCat({u"<img src=\"data:image/jpeg;base64,",
                    base::UTF8ToUTF16(base::Base64Encode("a1b2c3")), u"\">"}));
  EXPECT_TRUE(ash::ToastManager::Get()->IsToastShown("lobster_toast"));
}

}  // namespace

}  // namespace ash
