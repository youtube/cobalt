// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/api/braille_display_private/stub_braille_controller.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

namespace extensions {
namespace api {
namespace braille_display_private {

namespace {

constexpr char kTestUserEmail[] = "testuser@gmail.com";

// Used to make ReadKeys return an error.
brlapi_keyCode_t kErrorKeyCode = BRLAPI_KEY_MAX;

}  // namespace

// Data maintained by the mock BrlapiConnection.  This data lives throughout
// a test, while the api implementation takes ownership of the connection
// itself.
struct MockBrlapiConnectionData {
  bool connected;
  size_t display_columns;
  size_t display_rows;
  size_t cell_size;
  brlapi_error_t error;
  std::vector<std::string> written_content;
  // List of brlapi key codes.  A negative number makes the connection mock
  // return an error from ReadKey.
  base::circular_deque<brlapi_keyCode_t> pending_keys;
  // Causes a new display to appear to appear on disconnect, that is the
  // display size doubles and the controller gets notified of a brltty
  // restart.
  bool reappear_on_disconnect;
};

class MockBrlapiConnection : public BrlapiConnection {
 public:
  explicit MockBrlapiConnection(MockBrlapiConnectionData* data)
      : data_(data) {}

  ConnectResult Connect(OnDataReadyCallback on_data_ready) override {
    data_->connected = true;
    on_data_ready_ = std::move(on_data_ready);
    if (!data_->pending_keys.empty()) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&MockBrlapiConnection::NotifyDataReady,
                                    base::Unretained(this)));
    }
    return CONNECT_SUCCESS;
  }

  void Disconnect() override {
    data_->connected = false;
    if (data_->reappear_on_disconnect) {
      data_->display_columns *= 2;
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              &BrailleControllerImpl::PokeSocketDirForTesting,
              base::Unretained(BrailleControllerImpl::GetInstance())));
    }
  }

  bool Connected() override { return data_->connected; }

  brlapi_error_t* BrlapiError() override { return &data_->error; }

  std::string BrlapiStrError() override {
    return data_->error.brlerrno != BRLAPI_ERROR_SUCCESS ? "Error" : "Success";
  }

  bool GetDisplaySize(unsigned int* columns, unsigned int* rows) override {
    *columns = data_->display_columns;
    *rows = data_->display_rows;
    return true;
  }

  bool WriteDots(const std::vector<unsigned char>& cells) override {
    std::string written(
        cells.begin(),
        cells.begin() + data_->display_rows * data_->display_columns);
    data_->written_content.push_back(written);
    return true;
  }

  int ReadKey(brlapi_keyCode_t* key_code) override {
    if (!data_->pending_keys.empty()) {
      brlapi_keyCode_t queued_key_code = data_->pending_keys.front();
      data_->pending_keys.pop_front();
      if (queued_key_code == kErrorKeyCode) {
        data_->error.brlerrno = BRLAPI_ERROR_EOF;
        return -1;  // Signal error.
      }
      *key_code = queued_key_code;
      return 1;
    } else {
      return 0;
    }
  }

  bool GetCellSize(unsigned int* cell_size) override {
    *cell_size = data_->cell_size;
    return true;
  }

 private:
  void NotifyDataReady() {
    on_data_ready_.Run();
    if (!data_->pending_keys.empty()) {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&MockBrlapiConnection::NotifyDataReady,
                                    base::Unretained(this)));
    }
  }

  raw_ptr<MockBrlapiConnectionData, LeakedDanglingUntriaged | ExperimentalAsh>
      data_;
  OnDataReadyCallback on_data_ready_;
};

class BrailleDisplayPrivateApiTest : public ExtensionApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    connection_data_.connected = false;
    connection_data_.display_rows = 0;
    connection_data_.display_columns = 0;
    connection_data_.error.brlerrno = BRLAPI_ERROR_SUCCESS;
    connection_data_.reappear_on_disconnect = false;
    BrailleControllerImpl::GetInstance()->use_self_in_tests_ = true;
    BrailleControllerImpl::GetInstance()->SetCreateBrlapiConnectionForTesting(
        base::BindOnce(&BrailleDisplayPrivateApiTest::CreateBrlapiConnection,
                       base::Unretained(this)));
    BrailleControllerImpl::GetInstance()->skip_libbrlapi_so_load_ = true;
    DisableAccessibilityManagerBraille();
  }

 protected:
  MockBrlapiConnectionData connection_data_;

  // By default, don't let the accessibility manager interfere and
  // steal events.  Some tests override this to keep the normal behaviour
  // of the accessibility manager.
  virtual void DisableAccessibilityManagerBraille() {
    ash::AccessibilityManager::SetBrailleControllerForTest(
        &stub_braille_controller_);
  }

 private:
  std::unique_ptr<BrlapiConnection> CreateBrlapiConnection() {
    return std::unique_ptr<BrlapiConnection>(
        new MockBrlapiConnection(&connection_data_));
  }

  StubBrailleController stub_braille_controller_;
};

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, WriteDots) {
  connection_data_.display_columns = 11;
  connection_data_.display_rows = 1;
  connection_data_.cell_size = 6;
  ASSERT_TRUE(RunExtensionTest("braille_display_private/write_dots", {},
                               {.load_as_component = true}))
      << message_;
  ASSERT_EQ(4U, connection_data_.written_content.size());

  // testWriteEmptyCells.
  EXPECT_EQ(std::string(11, 0), connection_data_.written_content[0]);

  // testWriteOversizedCells.
  EXPECT_EQ(std::string(11, 1), connection_data_.written_content[1]);
  EXPECT_EQ(std::string(11, 2), connection_data_.written_content[2]);

  // testWriteUndersizedCellsNoCrash.
  EXPECT_EQ(std::string(9, 3) + std::string(2, 0),
            connection_data_.written_content[3]);
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, WriteDotsMultiLine) {
  connection_data_.display_columns = 20;
  connection_data_.display_rows = 7;
  connection_data_.cell_size = 6;
  ASSERT_TRUE(RunExtensionTest("braille_display_private/write_dots_multi_line",
                               {}, {.load_as_component = true}))
      << message_;
  ASSERT_EQ(6U, connection_data_.written_content.size());

  // testWriteEmptyCells.
  EXPECT_EQ(std::string(140, 0), connection_data_.written_content[0]);
  EXPECT_EQ(std::string(140, 0), connection_data_.written_content[1]);
  EXPECT_EQ(std::string(140, 0), connection_data_.written_content[2]);

  // testWriteOversizedCells.

  // The test passes a grid of cells 19x9 on a display of 20x7. (cols x rows).
  // Thus, the last two rows get truncated, and the last column is unfilled
  // (0s).
  std::string grid;
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  grid += std::string(19, 1) + std::string(1, 0);
  EXPECT_EQ(grid, connection_data_.written_content[3]);

  // This one is 21x8, so only the last row is truncated.
  EXPECT_EQ(std::string(140, 2), connection_data_.written_content[4]);

  // testWriteUndersizedCellsNoCrash.

  // 10x2.
  std::string grid2 = std::string(10, 3) + std::string(10, 0) +
                      std::string(10, 3) + std::string(10, 0) +
                      std::string(100, 0);
  EXPECT_EQ(grid2, connection_data_.written_content[5]);
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, KeyEvents) {
  connection_data_.display_columns = 11;
  connection_data_.display_rows = 1;
  connection_data_.cell_size = 6;

  // Braille navigation commands.
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_LNUP);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_LNDN);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_FWINLT);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_FWINRT);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_TOP);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_BOT);
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                          BRLAPI_KEY_CMD_ROUTE | 5);

  // Braille display standard keyboard emulation.

  // An ascii character.
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_SYM | 'A');
  // A non-ascii 'latin1' character.  Small letter a with ring above.
  connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_SYM | 0xE5);
  // A non-latin1 Unicode character.  LATIN SMALL LETTER A WITH MACRON.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_SYM_UNICODE | 0x100);
  // A Unicode character outside the BMP.  CAT FACE WITH TEARS OF JOY.
  // With anticipation for the first emoji-enabled braille display.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_SYM_UNICODE | 0x1F639);
  // Invalid Unicode character.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_SYM_UNICODE | 0x110000);

  // Non-alphanumeric function keys.

  // Backspace.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_SYM_BACKSPACE);
  // Shift+Tab.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_FLG_SHIFT | BRLAPI_KEY_SYM_TAB);
  // Alt+F3. (0-based).
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_SYM | BRLAPI_KEY_FLG_META |
      (BRLAPI_KEY_SYM_FUNCTION + 2));

  // ctrl+dot1+dot2.
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_FLG_CONTROL | BRLAPI_KEY_CMD_PASSDOTS |
      BRLAPI_DOT1 | BRLAPI_DOT2);

  // Braille dot keys, all combinations including space (0).
  for (int i = 0; i < 256; ++i) {
    connection_data_.pending_keys.push_back(BRLAPI_KEY_TYPE_CMD |
                                            BRLAPI_KEY_CMD_PASSDOTS | i);
  }

  ASSERT_TRUE(RunExtensionTest("braille_display_private/key_events", {},
                               {.load_as_component = true}));
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, DisplayStateChanges) {
  connection_data_.display_columns = 11;
  connection_data_.display_rows = 1;
  connection_data_.cell_size = 6;
  connection_data_.pending_keys.push_back(kErrorKeyCode);
  connection_data_.reappear_on_disconnect = true;
  ASSERT_TRUE(RunExtensionTest("braille_display_private/display_state_changes",
                               {}, {.load_as_component = true}));
}

class BrailleDisplayPrivateAPIUserTest : public BrailleDisplayPrivateApiTest {
 public:
  class MockEventDelegate : public BrailleDisplayPrivateAPI::EventDelegate {
   public:
    MockEventDelegate() = default;
    MockEventDelegate(const MockEventDelegate&) = delete;
    MockEventDelegate& operator=(const MockEventDelegate&) = delete;
    ~MockEventDelegate() override = default;

    int GetEventCount() const { return event_count_; }

    // BrailleDisplayPrivateAPI::EventDelegate:
    void BroadcastEvent(std::unique_ptr<Event> event) override {
      ++event_count_;
    }
    bool HasListener() override { return true; }

   private:
    int event_count_ = 0;
  };

  BrailleDisplayPrivateAPIUserTest() = default;
  BrailleDisplayPrivateAPIUserTest(const BrailleDisplayPrivateAPIUserTest&) =
      delete;
  BrailleDisplayPrivateAPIUserTest& operator=(
      const BrailleDisplayPrivateAPIUserTest&) = delete;
  ~BrailleDisplayPrivateAPIUserTest() override = default;

  MockEventDelegate* SetMockEventDelegate(BrailleDisplayPrivateAPI* api) {
    MockEventDelegate* delegate = new MockEventDelegate();
    api->SetEventDelegateForTest(
        std::unique_ptr<BrailleDisplayPrivateAPI::EventDelegate>(delegate));
    return delegate;
  }

  // BrailleDisplayPrivateApiTest:
  void SetUpInProcessBrowserTestFixture() override {
    BrailleDisplayPrivateApiTest::SetUpInProcessBrowserTestFixture();
    zero_duration_mode_ =
        std::make_unique<ui::ScopedAnimationDurationScaleMode>(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  }

  // BrailleDisplayPrivateApiTest:
  void DisableAccessibilityManagerBraille() override {
    // Let the accessibility manager behave as usual for these tests.
  }

 protected:
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;
};

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateAPIUserTest, KeyEventOnLockScreen) {
  ash::ScreenLockerTester tester;

  // Make sure the signin profile and active profile are different.
  Profile* signin_profile = ash::ProfileHelper::GetSigninProfile();
  Profile* user_profile = ProfileManager::GetActiveUserProfile();
  ASSERT_FALSE(signin_profile->IsSameOrParent(user_profile))
      << signin_profile->GetDebugName() << " vs "
      << user_profile->GetDebugName();

  // Create API and event delegate for the signin profile.
  BrailleDisplayPrivateAPI signin_api(signin_profile);
  MockEventDelegate* signin_delegate = SetMockEventDelegate(&signin_api);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  // Create API and delegate for the user profile.
  BrailleDisplayPrivateAPI user_api(user_profile);
  MockEventDelegate* user_delegate = SetMockEventDelegate(&user_api);

  // Send key event to both profiles.
  KeyEvent key_event;
  key_event.command = KeyCommand::kLineUp;
  signin_api.OnBrailleKeyEvent(key_event);
  user_api.OnBrailleKeyEvent(key_event);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  EXPECT_EQ(1, user_delegate->GetEventCount());

  // Lock screen, and make sure that the key event goes to the signin profile.
  tester.Lock();
  signin_api.OnBrailleKeyEvent(key_event);
  user_api.OnBrailleKeyEvent(key_event);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  EXPECT_EQ(2, user_delegate->GetEventCount());

  // Unlock screen, making sure key events go to the user profile again.
  tester.SetUnlockPassword(AccountId::FromUserEmail(kTestUserEmail), "pass");
  tester.UnlockWithPassword(AccountId::FromUserEmail(kTestUserEmail), "pass");
  signin_api.OnBrailleKeyEvent(key_event);
  user_api.OnBrailleKeyEvent(key_event);
  EXPECT_EQ(0, signin_delegate->GetEventCount());
  EXPECT_EQ(3, user_delegate->GetEventCount());
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
