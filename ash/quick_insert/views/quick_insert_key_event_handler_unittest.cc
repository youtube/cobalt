// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/views/quick_insert_key_event_handler.h"

#include "ash/quick_insert/views/quick_insert_pseudo_focus_handler.h"
#include "base/i18n/rtl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

using ::testing::Return;

ui::KeyEvent CreateKeyEvent(ui::KeyboardCode key_code,
                            int flags = ui::EF_NONE) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, key_code, ui::DomCode::NONE,
                      flags, ui::EventTimeForNow());
}

class MockPseudoFocusHandler : public QuickInsertPseudoFocusHandler {
 public:
  MockPseudoFocusHandler() = default;
  MockPseudoFocusHandler(const MockPseudoFocusHandler&) = delete;
  MockPseudoFocusHandler& operator=(const MockPseudoFocusHandler&) = delete;
  ~MockPseudoFocusHandler() override = default;

  // QuickInsertPseudoFocusHandler:
  bool DoPseudoFocusedAction() override { return true; }
  bool MovePseudoFocusUp() override { return true; }
  bool MovePseudoFocusDown() override { return true; }
  MOCK_METHOD(bool, MovePseudoFocusLeft, (), (override));
  MOCK_METHOD(bool, MovePseudoFocusRight, (), (override));
  bool AdvancePseudoFocus(QuickInsertPseudoFocusDirection direction) override {
    return true;
  }
};

TEST(QuickInsertKeyEventHandlerTest,
     DoesNotHandleKeyEventyWithoutPseudoFocusHandler) {
  QuickInsertKeyEventHandler key_event_handler;

  EXPECT_FALSE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesKeyEventWithPseudoFocusHandler) {
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesUnmodifedArrowKeyEvent) {
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_UP)));
}

TEST(QuickInsertKeyEventHandlerTest, DoesNotHandleModifiedArrowKeyEvent) {
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_FALSE(key_event_handler.HandleKeyEvent(
      CreateKeyEvent(ui::VKEY_UP, ui::EF_SHIFT_DOWN)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesTabKeyEvent) {
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_TAB)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesShiftTabKeyEvent) {
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(
      CreateKeyEvent(ui::VKEY_TAB, ui::EF_SHIFT_DOWN)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesLeftArrowLTR) {
  base::i18n::SetRTLForTesting(false);
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_CALL(pseudo_focus_handler, MovePseudoFocusLeft())
      .WillOnce(Return(true));

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_LEFT)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesRightArrowLTR) {
  base::i18n::SetRTLForTesting(false);
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_CALL(pseudo_focus_handler, MovePseudoFocusRight())
      .WillOnce(Return(true));

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RIGHT)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesLeftArrowRTL) {
  base::i18n::SetRTLForTesting(true);
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_CALL(pseudo_focus_handler, MovePseudoFocusRight())
      .WillOnce(Return(true));

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_LEFT)));
}

TEST(QuickInsertKeyEventHandlerTest, HandlesRightArrowRTL) {
  base::i18n::SetRTLForTesting(true);
  QuickInsertKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_CALL(pseudo_focus_handler, MovePseudoFocusLeft())
      .WillOnce(Return(true));

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RIGHT)));
}

}  // namespace
}  // namespace ash
