// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/pen_event_processor.h"

#include <combaseapi.h>
#include <windows.devices.input.h>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_winrt_initializer.h"
#include "components/stylus_handwriting/win/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/win/stylus_handwriting_properties_win.h"
#include "ui/gfx/sequential_id_generator.h"

namespace {

Microsoft::WRL::ComPtr<ABI::Windows::Devices::Input::IPenDeviceStatics>
GetNullPenDeviceStatics() {
  return nullptr;
}

}  // namespace

namespace views {

class PenProcessorTest : public ::testing::Test {
 public:
  PenProcessorTest() = default;
  ~PenProcessorTest() override = default;

  // testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

  // Enables Stylus Handwriting feature.
  void EnableStylusHandwriting();

 private:
  base::win::ScopedWinrtInitializer scoped_winrt_initializer_;
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  views::PenIdHandler::ScopedPenIdStaticsForTesting pen_id_statics_scoper_{
      &GetNullPenDeviceStatics};
};

void PenProcessorTest::SetUp() {
  ASSERT_TRUE(scoped_winrt_initializer_.Succeeded());
}

void PenProcessorTest::TearDown() {
  scoped_feature_list_.Reset();
}

void PenProcessorTest::EnableStylusHandwriting() {
  scoped_feature_list_.InitAndEnableFeature(
      stylus_handwriting::win::kStylusHandwritingWin);
}

TEST_F(PenProcessorTest, TypicalCaseDMDisabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ false);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERENTER, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseEntered, event->AsMouseEvent()->type());

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  event = processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMousePressed, event->AsMouseEvent()->type());
  EXPECT_EQ(1, event->AsMouseEvent()->GetClickCount());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseDragged, event->AsMouseEvent()->type());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_INCONTACT;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseReleased, event->AsMouseEvent()->type());
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseMoved, event->AsMouseEvent()->type());

  event = processor.GenerateEvent(WM_POINTERLEAVE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseExited, event->AsMouseEvent()->type());
}

TEST_F(PenProcessorTest, TypicalCaseDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  // Set up the modifier state that shift is down so we can test
  // modifiers are propagated for mouse and touch events.
  BYTE restore_key_state[256];
  GetKeyboardState(restore_key_state);
  BYTE shift_key_state[256];
  memset(shift_key_state, 0, sizeof(shift_key_state));
  // Mask high order bit on indicating it is down.
  // See MSDN GetKeyState().
  shift_key_state[VK_SHIFT] |= 0x80;
  SetKeyboardState(shift_key_state);

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERENTER, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseEntered, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_SHIFT_DOWN);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  event = processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_SHIFT_DOWN);

  // Restore the keyboard state back to what it was in the beginning.
  SetKeyboardState(restore_key_state);

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchMoved, event->AsTouchEvent()->type());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event = processor.GenerateEvent(WM_POINTERUPDATE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseMoved, event->type());

  event = processor.GenerateEvent(WM_POINTERLEAVE, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseExited, event->AsMouseEvent()->type());
}

TEST_F(PenProcessorTest, UnpairedPointerDownTouchDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  EXPECT_EQ(nullptr, event.get());
}

TEST_F(PenProcessorTest, UnpairedPointerDownMouseDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  EXPECT_EQ(nullptr, event.get());
}

TEST_F(PenProcessorTest, TouchFlagDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_UP;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  EXPECT_FALSE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
}

TEST_F(PenProcessorTest, MouseFlagDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMousePressed, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_EQ(ui::EventType::kMouseReleased, event->AsMouseEvent()->type());
  EXPECT_TRUE(event->flags() & ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_EQ(ui::EF_LEFT_MOUSE_BUTTON,
            event->AsMouseEvent()->changed_button_flags());
}

TEST_F(PenProcessorTest, PenEraserFlagDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  gfx::Point point(100, 100);

  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;
  pen_info.penFlags = PEN_FLAG_ERASER;

  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  EXPECT_EQ(ui::EventPointerType::kEraser,
            event->AsTouchEvent()->pointer_details().pointer_type);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_UP;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

  event = processor.GenerateEvent(WM_POINTERUP, 0, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  EXPECT_EQ(ui::EventPointerType::kEraser,
            event->AsTouchEvent()->pointer_details().pointer_type);
}

TEST_F(PenProcessorTest, MultiPenDMEnabled) {
  ui::SequentialIDGenerator id_generator(0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled*/ true);

  std::array<POINTER_PEN_INFO, 3> pen_info;
  for (auto& i : pen_info) {
    memset(&i, 0, sizeof(POINTER_PEN_INFO));
  }

  gfx::Point point(100, 100);

  for (size_t i = 0; i < pen_info.size(); i++) {
    pen_info[i].pointerInfo.pointerFlags =
        POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
    pen_info[i].pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

    size_t pointer_id = i;
    std::unique_ptr<ui::Event> event =
        processor.GenerateEvent(WM_POINTERDOWN, pointer_id, pen_info[i], point);
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());
    EXPECT_EQ(ui::EventType::kTouchPressed, event->AsTouchEvent()->type());
  }

  for (size_t i = 0; i < pen_info.size(); i++) {
    pen_info[i].pointerInfo.pointerFlags = POINTER_FLAG_UP;
    pen_info[i].pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;

    size_t pointer_id = i;
    std::unique_ptr<ui::Event> event =
        processor.GenerateEvent(WM_POINTERUP, pointer_id, pen_info[i], point);
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());
    EXPECT_EQ(ui::EventType::kTouchReleased, event->AsTouchEvent()->type());
  }
}

TEST_F(PenProcessorTest, StylusHandwritingPropertiesDMEnabled) {
  EnableStylusHandwriting();
  ui::SequentialIDGenerator id_generator(/*min_id=*/0);
  PenEventProcessor processor(&id_generator,
                              /*direct_manipulation_enabled=*/true);
  const uint32_t pointer_id = 1;
  POINTER_PEN_INFO pen_info;
  memset(&pen_info, 0, sizeof(POINTER_PEN_INFO));
  pen_info.pointerInfo.pointerFlags =
      POINTER_FLAG_INCONTACT | POINTER_FLAG_FIRSTBUTTON;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_DOWN;

  const gfx::Point point(100, 100);
  std::unique_ptr<ui::Event> event =
      processor.GenerateEvent(WM_POINTERDOWN, pointer_id, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  ASSERT_TRUE(event->AsTouchEvent()->properties());
  EXPECT_FALSE(event->AsTouchEvent()->properties()->empty());

  const std::optional<ui::StylusHandwritingPropertiesWin> properties =
      ui::GetStylusHandwritingProperties(*event);
  ASSERT_TRUE(properties.has_value());
  EXPECT_EQ(properties->handwriting_pointer_id, pointer_id);
  EXPECT_EQ(properties->handwriting_stroke_id, 0U);

  pen_info.pointerInfo.pointerFlags = POINTER_FLAG_NONE;
  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_FIRSTBUTTON_UP;
  event = processor.GenerateEvent(WM_POINTERUP, pointer_id, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsTouchEvent());
  EXPECT_FALSE(event->AsTouchEvent()->properties());

  pen_info.pointerInfo.ButtonChangeType = POINTER_CHANGE_NONE;
  event =
      processor.GenerateEvent(WM_POINTERUPDATE, pointer_id, pen_info, point);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  EXPECT_FALSE(event->AsMouseEvent()->properties());
}

}  // namespace views
