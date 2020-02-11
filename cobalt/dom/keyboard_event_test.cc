// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/keyboard_event.h"

#include "cobalt/base/tokens.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/dom/global_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class KeyboardEventTest : public ::testing::Test {
 protected:
  KeyboardEventTest();
  ~KeyboardEventTest() override;
};

KeyboardEventTest::KeyboardEventTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

KeyboardEventTest::~KeyboardEventTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

TEST_F(KeyboardEventTest, ShouldHaveBubblesAndCancelableSet) {
  scoped_refptr<KeyboardEvent> keyboard_event = new KeyboardEvent("keydown");
  EXPECT_TRUE(keyboard_event->bubbles());
  EXPECT_TRUE(keyboard_event->cancelable());
}

TEST_F(KeyboardEventTest, CanGetKeyLocation) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event->key_location(),
            KeyboardEvent::kDomKeyLocationStandard);

  init.set_location(KeyboardEvent::kDomKeyLocationLeft);
  scoped_refptr<KeyboardEvent> keyboard_event_l =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_l->key_location(),
            KeyboardEvent::kDomKeyLocationLeft);

  init.set_location(KeyboardEvent::kDomKeyLocationRight);
  scoped_refptr<KeyboardEvent> keyboard_event_r =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_r->key_location(),
            KeyboardEvent::kDomKeyLocationRight);
}

TEST_F(KeyboardEventTest, CanGetKeyIdentifierAndKeyAndCode) {
  KeyboardEventInit init;
  init.set_key_code(keycode::kA);
  scoped_refptr<KeyboardEvent> keyboard_event_a =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_a->key_identifier(), "a");
  EXPECT_EQ(keyboard_event_a->key(), "a");
  EXPECT_EQ(keyboard_event_a->code(), "KeyA");

  init.set_key_code(keycode::kA);
  init.set_ctrl_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_ca =
      new KeyboardEvent("keydown", init);
  init.set_ctrl_key(false);
  EXPECT_EQ(keyboard_event_ca->key_identifier(), "a");
  EXPECT_EQ(keyboard_event_ca->key(), "a");
  EXPECT_EQ(keyboard_event_ca->code(), "KeyA");

  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_sa =
      new KeyboardEvent("keydown", init);
  init.set_shift_key(false);
  EXPECT_EQ(keyboard_event_sa->key_identifier(), "A");
  EXPECT_EQ(keyboard_event_sa->key(), "A");
  EXPECT_EQ(keyboard_event_sa->code(), "KeyA");

  init.set_key_code(keycode::k1);
  scoped_refptr<KeyboardEvent> keyboard_event_1 =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_1->key_identifier(), "1");
  EXPECT_EQ(keyboard_event_1->key(), "1");
  EXPECT_EQ(keyboard_event_1->code(), "Digit1");

  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_s1 =
      new KeyboardEvent("keydown", init);
  init.set_shift_key(false);
  EXPECT_EQ(keyboard_event_s1->key_identifier(), "!");
  EXPECT_EQ(keyboard_event_s1->key(), "!");
  EXPECT_EQ(keyboard_event_1->code(), "Digit1");

  init.set_key_code(keycode::kLeft);
  scoped_refptr<KeyboardEvent> keyboard_event_left =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_left->key_identifier(), "ArrowLeft");
  EXPECT_EQ(keyboard_event_left->key(), "ArrowLeft");
  EXPECT_EQ(keyboard_event_left->code(), "ArrowLeft");

  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_sleft =
      new KeyboardEvent("keydown", init);
  init.set_shift_key(false);
  EXPECT_EQ(keyboard_event_sleft->key_identifier(), "ArrowLeft");
  EXPECT_EQ(keyboard_event_sleft->key(), "ArrowLeft");
  EXPECT_EQ(keyboard_event_sleft->code(), "ArrowLeft");

  init.set_key_code(keycode::kNumpad5);
  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_num5 =
      new KeyboardEvent("keydown", init);
  init.set_shift_key(false);
  EXPECT_EQ(keyboard_event_num5->key_identifier(), "5");
  EXPECT_EQ(keyboard_event_num5->key(), "5");
  EXPECT_EQ(keyboard_event_num5->code(), "Numpad5");

  init.set_key_code(keycode::kSpace);
  scoped_refptr<KeyboardEvent> keyboard_event_space =
      new KeyboardEvent("keydown", init);
  EXPECT_EQ(keyboard_event_space->key_identifier(), " ");
  EXPECT_EQ(keyboard_event_space->key(), " ");
  EXPECT_EQ(keyboard_event_space->code(), "Space");
}

TEST_F(KeyboardEventTest, CanGetAltKey) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->alt_key());

  init.set_alt_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_a =
      new KeyboardEvent("keydown", init);
  init.set_alt_key(false);
  EXPECT_TRUE(keyboard_event_a->alt_key());
}

TEST_F(KeyboardEventTest, CanGetCtrlKey) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->ctrl_key());

  init.set_ctrl_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_c =
      new KeyboardEvent("keydown", init);
  init.set_ctrl_key(false);
  EXPECT_TRUE(keyboard_event_c->ctrl_key());
}

TEST_F(KeyboardEventTest, CanGetMetaKey) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->meta_key());

  init.set_meta_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_m =
      new KeyboardEvent("keydown", init);
  init.set_meta_key(false);
  EXPECT_TRUE(keyboard_event_m->meta_key());
}

TEST_F(KeyboardEventTest, CanGetShiftKey) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->shift_key());

  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_s =
      new KeyboardEvent("keydown", init);
  init.set_shift_key(false);
  EXPECT_TRUE(keyboard_event_s->shift_key());
}

TEST_F(KeyboardEventTest, CanGetModifierState) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->GetModifierState("Alt"));
  EXPECT_FALSE(keyboard_event->GetModifierState("Control"));
  EXPECT_FALSE(keyboard_event->GetModifierState("Meta"));
  EXPECT_FALSE(keyboard_event->GetModifierState("Shift"));

  init.set_alt_key(true);
  init.set_ctrl_key(true);
  init.set_meta_key(true);
  init.set_shift_key(true);
  scoped_refptr<KeyboardEvent> keyboard_event_m =
      new KeyboardEvent("keydown", init);
  EXPECT_TRUE(keyboard_event_m->GetModifierState("Alt"));
  EXPECT_TRUE(keyboard_event_m->GetModifierState("Control"));
  EXPECT_TRUE(keyboard_event_m->GetModifierState("Meta"));
  EXPECT_TRUE(keyboard_event_m->GetModifierState("Shift"));
}

TEST_F(KeyboardEventTest, CanGetRepeat) {
  KeyboardEventInit init;
  scoped_refptr<KeyboardEvent> keyboard_event =
      new KeyboardEvent("keydown", init);
  EXPECT_FALSE(keyboard_event->repeat());

  init.set_repeat(true);
  scoped_refptr<KeyboardEvent> keyboard_event_r =
      new KeyboardEvent("keydown", init);
  EXPECT_TRUE(keyboard_event_r->repeat());
}
}  // namespace dom
}  // namespace cobalt
