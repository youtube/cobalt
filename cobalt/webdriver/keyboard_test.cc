/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <vector>

#include "cobalt/dom/keyboard_event.h"
#include "cobalt/dom/keycode.h"
#include "cobalt/webdriver/keyboard.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Each;
using testing::ElementsAre;
using testing::Eq;

namespace cobalt {
namespace webdriver {
namespace {

int32 GetKeyCode(const scoped_refptr<dom::KeyboardEvent>& event) {
  DCHECK(event);
  return event->key_code();
}

int32 GetCharCode(const scoped_refptr<dom::KeyboardEvent>& event) {
  return event->char_code();
}

uint32 GetModifierBitfield(const scoped_refptr<dom::KeyboardEvent>& event) {
  return event->modifiers();
}

std::string GetType(const scoped_refptr<dom::KeyboardEvent>& event) {
  return event->type().c_str();
}

int GetLocation(const scoped_refptr<dom::KeyboardEvent>& event) {
  return event->location();
}

class KeyboardTest : public ::testing::Test {
 protected:
  // Less verbose redefinitions.
  const int kNoModifier = dom::UIEventWithKeyState::kNoModifier;
  const int kShift = dom::UIEventWithKeyState::kShiftKey;
  const int kAlt = dom::UIEventWithKeyState::kAltKey;
  const int kAltAndShift = kAlt | kShift;

  const int kLocationStandard = dom::KeyboardEvent::kDomKeyLocationStandard;
  const int kLocationLeft = dom::KeyboardEvent::kDomKeyLocationLeft;
  std::vector<int32> GetKeyCodes() {
    std::vector<int32> key_codes(events_.size());
    std::transform(events_.begin(), events_.end(), key_codes.begin(),
                   GetKeyCode);
    return key_codes;
  }
  std::vector<int32> GetCharCodes() {
    std::vector<int32> char_codes(events_.size());
    std::transform(events_.begin(), events_.end(), char_codes.begin(),
                   GetCharCode);
    return char_codes;
  }
  std::vector<uint32> GetModifiers() {
    std::vector<uint32> modifiers(events_.size());
    std::transform(events_.begin(), events_.end(), modifiers.begin(),
                   GetModifierBitfield);
    return modifiers;
  }
  std::vector<int> GetLocations() {
    std::vector<int> locations(events_.size());
    std::transform(events_.begin(), events_.end(), locations.begin(),
                   GetLocation);
    return locations;
  }
  std::vector<std::string> GetTypes() {
    std::vector<std::string> types(events_.size());
    std::transform(events_.begin(), events_.end(), types.begin(), GetType);
    return types;
  }
  Keyboard::KeyboardEventVector events_;
};
}  // namespace

TEST_F(KeyboardTest, LowerCaseCharacter) {
  std::string keys = "r";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  ASSERT_EQ(events_.size(), 3);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keypress", "keyup"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre('R', 0, 'R'));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 'r', 0));
  EXPECT_THAT(GetModifiers(), Each(Eq(kNoModifier)));
  EXPECT_THAT(GetLocations(), Each(Eq(kLocationStandard)));
}

TEST_F(KeyboardTest, UpperCaseCharacter) {
  std::string keys = "R";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  // keydown(shift)
  //   keydown(R), keypress(R), keyup(R)
  // keyup(shift)
  ASSERT_EQ(events_.size(), 5);
  EXPECT_THAT(GetTypes(),
              ElementsAre("keydown", "keydown", "keypress", "keyup", "keyup"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre(dom::keycode::kShift, 'R', 0, 'R',
                                         dom::keycode::kShift));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 0, 'R', 0, 0));
  EXPECT_THAT(GetModifiers(),
              ElementsAre(kShift, kShift, kShift, kShift, kNoModifier));
  EXPECT_THAT(GetLocations(),
              ElementsAre(kLocationLeft, kLocationStandard, kLocationStandard,
                          kLocationStandard, kLocationLeft));
}

TEST_F(KeyboardTest, ShiftedCharacter) {
  std::string keys = "&";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  // keydown(shift)
  //   keydown(7), keypress(&), keyup(7)
  // keyup(shift)
  ASSERT_EQ(events_.size(), 5);
  EXPECT_THAT(GetTypes(),
              ElementsAre("keydown", "keydown", "keypress", "keyup", "keyup"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre(dom::keycode::kShift, '7', 0, '7',
                                         dom::keycode::kShift));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 0, '&', 0, 0));
  EXPECT_THAT(GetModifiers(),
              ElementsAre(kShift, kShift, kShift, kShift, kNoModifier));
  EXPECT_THAT(GetLocations(),
              ElementsAre(kLocationLeft, kLocationStandard, kLocationStandard,
                          kLocationStandard, kLocationLeft));
}

TEST_F(KeyboardTest, Modifier) {
  // \uE00A is the Alt-key modifier.
  std::string keys = "\uE00A";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  ASSERT_EQ(events_.size(), 1);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre(dom::keycode::kMenu));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0));
  EXPECT_THAT(GetModifiers(), ElementsAre(kAlt));
  EXPECT_THAT(GetLocations(), ElementsAre(kLocationLeft));
}

TEST_F(KeyboardTest, ModifiersAreKept) {
  // \uE00A is the Alt-key modifier.
  // \uE008 is the Shift-key modifier.
  std::string keys = "\uE00A\uE008";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  ASSERT_EQ(events_.size(), 2);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keydown"));
  EXPECT_THAT(GetKeyCodes(),
              ElementsAre(dom::keycode::kMenu, dom::keycode::kShift));
  EXPECT_THAT(GetCharCodes(), Each(Eq(0)));
  EXPECT_THAT(GetModifiers(), ElementsAre(kAlt, kAltAndShift));
  EXPECT_THAT(GetLocations(), Each(Eq(kLocationLeft)));
}

TEST_F(KeyboardTest, ModifiersAreReleased) {
  // \uE00A is the Alt-key modifier.
  // \uE008 is the Shift-key modifier.
  std::string keys = "\uE00A\uE008";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kReleaseModifiers, &events_);

  ASSERT_EQ(events_.size(), 4);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keydown", "keyup", "keyup"));
  EXPECT_THAT(GetKeyCodes(),
              ElementsAre(dom::keycode::kMenu, dom::keycode::kShift,
                          dom::keycode::kMenu, dom::keycode::kShift));
  EXPECT_THAT(GetCharCodes(), Each(Eq(0)));
  EXPECT_THAT(GetModifiers(),
              ElementsAre(kAlt, kAltAndShift, kShift, kNoModifier));
  EXPECT_THAT(GetLocations(), Each(Eq(kLocationLeft)));
}

TEST_F(KeyboardTest, SpecialCharacter) {
  // \uE012 is the Left-arrow key.
  std::string keys = "\uE012";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  ASSERT_EQ(events_.size(), 2);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keyup"));
  EXPECT_THAT(GetKeyCodes(), Each(Eq(dom::keycode::kLeft)));
  EXPECT_THAT(GetCharCodes(), Each(Eq(0)));
  EXPECT_THAT(GetModifiers(), Each(Eq(kNoModifier)));
  EXPECT_THAT(GetLocations(), Each(Eq(kLocationStandard)));
}

TEST_F(KeyboardTest, ModifierIsSticky) {
  // \uE00A is the Alt-key modifier. Corresponds to the kMenu key code.
  std::string keys = "\uE00AaB";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  // keydown(alt)
  //   keydown(A), keypress(a), keyup(A),
  //   keydown(shift)
  //     keydown(B), keypress(B), keyup(B)
  //   keyup(shift)
  ASSERT_EQ(events_.size(), 9);
  EXPECT_THAT(GetTypes(),
              ElementsAre("keydown", "keydown", "keypress", "keyup", "keydown",
                          "keydown", "keypress", "keyup", "keyup"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre(dom::keycode::kMenu, 'A', 0, 'A',
                                         dom::keycode::kShift, 'B', 0, 'B',
                                         dom::keycode::kShift));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 0, 'a', 0, 0, 0, 'B', 0, 0));
  EXPECT_THAT(GetModifiers(),
              ElementsAre(kAlt, kAlt, kAlt, kAlt, kAltAndShift, kAltAndShift,
                          kAltAndShift, kAltAndShift, kAlt));
  EXPECT_THAT(GetLocations(),
              ElementsAre(kLocationLeft, kLocationStandard, kLocationStandard,
                          kLocationStandard, kLocationLeft, kLocationStandard,
                          kLocationStandard, kLocationStandard, kLocationLeft));
}

TEST_F(KeyboardTest, ToggleModifier) {
  // \uE008 is the Shift-key modifier.
  std::string keys = "\uE008a\uE008a";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  // keydown(shift)
  //   keydown(A), keypress(A), keyup(A)
  // keyup(shift)
  // keydown(A), keypress(a), keyup(A)
  ASSERT_EQ(events_.size(), 8);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keydown", "keypress", "keyup",
                                      "keyup", "keydown", "keypress", "keyup"));
  EXPECT_THAT(GetKeyCodes(), ElementsAre(dom::keycode::kShift, 'A', 0, 'A',
                                         dom::keycode::kShift, 'A', 0, 'A'));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 0, 'A', 0, 0, 0, 'a', 0));

  EXPECT_THAT(GetModifiers(),
              ElementsAre(kShift, kShift, kShift, kShift, kNoModifier,
                          kNoModifier, kNoModifier, kNoModifier));
  EXPECT_THAT(GetLocations(),
              ElementsAre(kLocationLeft, kLocationStandard, kLocationStandard,
                          kLocationStandard, kLocationLeft, kLocationStandard,
                          kLocationStandard, kLocationStandard));
}

TEST_F(KeyboardTest, NullClearsModifiers) {
  std::string keys = "\uE008\uE00A\uE000a";
  Keyboard::TranslateToKeyEvents(keys, Keyboard::kKeepModifiers, &events_);

  // keydown(shift)
  //   keydown(alt)
  //   keyup(alt)
  // keyup(shift)
  // keydown(A), keypress(a), keyup(A)
  ASSERT_EQ(events_.size(), 7);
  EXPECT_THAT(GetTypes(), ElementsAre("keydown", "keydown", "keyup", "keyup",
                                      "keydown", "keypress", "keyup"));
  EXPECT_THAT(
      GetKeyCodes(),
      ElementsAre(dom::keycode::kShift, dom::keycode::kMenu,
                  dom::keycode::kMenu, dom::keycode::kShift, 'A', 0, 'A'));
  EXPECT_THAT(GetCharCodes(), ElementsAre(0, 0, 0, 0, 0, 'a', 0));

  EXPECT_THAT(GetModifiers(),
              ElementsAre(kShift, kAltAndShift, kShift, kNoModifier,
                          kNoModifier, kNoModifier, kNoModifier));
  EXPECT_THAT(
      GetLocations(),
      ElementsAre(kLocationLeft, kLocationLeft, kLocationLeft, kLocationLeft,
                  kLocationStandard, kLocationStandard, kLocationStandard));
}

}  // namespace webdriver
}  // namespace cobalt
