// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/nsmenuitem_additions.h"

#include <Carbon/Carbon.h>

#include <ostream>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

@interface NSEventForTesting : NSEvent
@property(copy, nonatomic) NSString* characters;
@end

@implementation NSEventForTesting

@synthesize characters = _characters;

- (void)setCharacters:(NSString*)aString {
  _characters = [aString copy];
  if (aString.length)
    ASSERT_TRUE([[self characters] isEqualToString:aString]);
}

- (NSString*)characters {
  return _characters != nil ? _characters : [super characters];
}

@end

std::ostream& operator<<(std::ostream& out, NSObject* obj) {
  return out << base::SysNSStringToUTF8([obj description]);
}

std::ostream& operator<<(std::ostream& out, NSMenuItem* item) {
  return out << "NSMenuItem " << base::SysNSStringToUTF8([item keyEquivalent]);
}

namespace ui {
namespace cocoa {
namespace {

NSEvent* KeyEvent(const NSUInteger modifierFlags,
                  NSString* chars,
                  NSString* charsNoMods,
                  const NSUInteger keyCode = 0) {
  return [NSEventForTesting keyEventWithType:NSEventTypeKeyDown
                                    location:NSZeroPoint
                               modifierFlags:modifierFlags
                                   timestamp:0.0
                                windowNumber:0
                                     context:nil
                                  characters:chars
                 charactersIgnoringModifiers:charsNoMods
                                   isARepeat:NO
                                     keyCode:keyCode];
}

NSMenuItem* MenuItem(NSString* equiv, NSUInteger mask = 0) {
  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:@""
                                                 action:NULL
                                          keyEquivalent:@""] autorelease];
  [item setKeyEquivalent:equiv];
  [item setKeyEquivalentModifierMask:mask];
  return item;
}

// Returns whether a keyboard layout is one of the "commandless" cyrillic
// layouts introduced in 10.15: these layouts do not ever fire key equivalents
// (in any app, not just Chrome) and appear not to be intended for full-time
// use.
bool IsCommandlessCyrillicLayout(NSString* layoutId) {
  return [layoutId isEqualToString:@"com.apple.keylayout.Kyrgyz-Cyrillic"] ||
         [layoutId isEqualToString:@"com.apple.keylayout.Mongolian-Cyrillic"];
}

void ExpectKeyFiresItemEq(bool expected_result,
                          NSEvent* key,
                          NSMenuItem* item,
                          NSString* layout_id,
                          bool compare_cocoa) {
  if (layout_id.length)
    layout_id = [NSString stringWithFormat:@"\nLayout: %@", layout_id];

  EXPECT_EQ(expected_result, [item cr_firesForKeyEquivalentEvent:key])
      << key << '\n'
      << item << layout_id;

  // Make sure that Cocoa does in fact agree with our expectations. However,
  // in some cases cocoa behaves weirdly (if you create e.g. a new event that
  // contains all fields of the event that you get when hitting cmd-a with a
  // russion keyboard layout, the copy won't fire a menu item that has cmd-a as
  // key equivalent, even though the original event would) and isn't a good
  // oracle function.
  if (compare_cocoa) {
    base::scoped_nsobject<NSMenu> menu([[NSMenu alloc] initWithTitle:@"Menu!"]);
    [menu setAutoenablesItems:NO];
    EXPECT_FALSE([menu performKeyEquivalent:key]);
    [menu addItem:item];
    EXPECT_EQ(expected_result, [menu performKeyEquivalent:key])
        << key << '\n'
        << item << layout_id;
  }
}

void ExpectKeyFiresItem(NSEvent* key,
                        NSMenuItem* item,
                        bool compare_cocoa = true,
                        NSString* layout_id = @"") {
  ExpectKeyFiresItemEq(true, key, item, layout_id, compare_cocoa);
}

void ExpectKeyDoesntFireItem(NSEvent* key,
                             NSMenuItem* item,
                             bool compare_cocoa = true,
                             NSString* layout_id = @"") {
  ExpectKeyFiresItemEq(false, key, item, layout_id, compare_cocoa);
}

TEST(NSMenuItemAdditionsTest, TestExtractsKeyEventModifierMask) {
  const NSEventModifierFlags mask =
      NSEventModifierFlagCommand | NSEventModifierFlagControl |
      NSEventModifierFlagOption | NSEventModifierFlagShift;

  // The mask returned by ModifierMaskForKeyEvent() should include
  // NSEventModifierFlagFunction if the user holds down the function modifier
  // key with a non-function key.
  NSEvent* event = KeyEvent(mask | NSEventModifierFlagFunction, @"e", @"e");
  EXPECT_EQ(mask | NSEventModifierFlagFunction, ModifierMaskForKeyEvent(event));

  // The mask returned by ModifierMaskForKeyEvent() should not include
  // NSEventModifierFlagFunction when the user presses a function key such as
  // up arrow (the AppKit adds NSEventModifierFlagFunction to the key event's
  // modifiers whenever the user presses a function key regardless of the state
  // of the function modifier key).
  unichar buffer[2];
  buffer[0] = NSUpArrowFunctionKey;
  buffer[1] = 0;
  NSString* characters = [NSString stringWithCharacters:buffer length:1];
  event = KeyEvent(NSEventModifierFlagFunction, characters, characters);
  EXPECT_EQ(mask, ModifierMaskForKeyEvent(event));

  // The mask returned by ModifierMaskForKeyEvent() should not include
  // NSEventModifierFlagFunction if the user does not hold down the function
  // modifier key with a non-function key.
  const NSEventModifierFlags kNoModifiers = 0;
  event = KeyEvent(kNoModifiers, @"a", @"a");
  EXPECT_EQ(mask, ModifierMaskForKeyEvent(event));
}

TEST(NSMenuItemAdditionsTest, TestFiresForKeyEvent) {
  // These test cases were built by writing a small test app that has a
  // MainMenu.xib with a given key equivalent set in Interface Builder and
  // some code that prints both the key equivalent that fires a menu item and
  // the menu item's key equivalent and modifier masks. I then pasted those
  // below. This was done with a US layout, unless otherwise noted. In the
  // comments, "z" always means the physical "z" key on a US layout no matter
  // what character that key produces.
  //
  // The modifier flag passed to KeyEvent below consists of standard key masks
  // like NSEventModifierFlagShift and device-dependent information in the
  // lower 16 bits.

  NSMenuItem* item;
  NSEvent* key;
  unichar ch;
  NSString* s;

  // Sanity
  item = MenuItem(@"");
  EXPECT_TRUE([item isEnabled]);

  // a
  key = KeyEvent(0x100, @"a", @"a", 0);
  item = MenuItem(@"a");
  ExpectKeyFiresItem(key, item);
  ExpectKeyDoesntFireItem(KeyEvent(0x20102, @"A", @"A", 0), item);

  // Disabled menu item
  key = KeyEvent(0x100, @"a", @"a", 0);
  item = MenuItem(@"a");
  [item setEnabled:NO];
  ExpectKeyDoesntFireItem(key, item, false);

  // shift-a
  key = KeyEvent(0x20102, @"A", @"A", 0);
  item = MenuItem(@"A");
  ExpectKeyFiresItem(key, item);
  ExpectKeyDoesntFireItem(KeyEvent(0x100, @"a", @"a", 0), item);

  // cmd-shift-t
  key = KeyEvent(0x12010a, @"t", @"T", 0);
  item = MenuItem(@"T", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);
  item = MenuItem(@"t", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);

  // cmd-opt-shift-a
  key = KeyEvent(0x1a012a, @"\u00c5", @"A", 0);
  item = MenuItem(@"A", NSEventModifierFlagCommand | NSEventModifierFlagOption);
  ExpectKeyFiresItem(key, item);

  // cmd-opt-a
  key = KeyEvent(0x18012a, @"\u00e5", @"a", 0);
  item = MenuItem(@"a", NSEventModifierFlagCommand | NSEventModifierFlagOption);
  ExpectKeyFiresItem(key, item);

  // cmd-=
  key = KeyEvent(0x100110, @"=", @"=", 0x18);
  item = MenuItem(@"=", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // cmd-shift-=
  key = KeyEvent(0x12010a, @"=", @"+", 0x18);
  item = MenuItem(@"+", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // Turns out Cocoa fires "+ 100108 + 18" if you hit cmd-= and the menu only
  // has a cmd-+ shortcut. But that's transparent for
  // |cr_firesForKeyEquivalentEvent:|.

  // ctrl-3
  key = KeyEvent(0x40101, @"3", @"3", 0x14);
  item = MenuItem(@"3", NSEventModifierFlagControl);
  ExpectKeyFiresItem(key, item);

  // return
  key = KeyEvent(0, @"\r", @"\r", 0x24);
  item = MenuItem(@"\r");
  ExpectKeyFiresItem(key, item);

  // shift-return
  key = KeyEvent(0x20102, @"\r", @"\r", 0x24);
  item = MenuItem(@"\r", NSEventModifierFlagShift);
  ExpectKeyFiresItem(key, item);

  // shift-left
  ch = NSLeftArrowFunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0xa20102, s, s, 0x7b);
  item = MenuItem(s, NSEventModifierFlagShift);
  ExpectKeyFiresItem(key, item);

  // shift-f1 (with a layout that needs the fn key down for f1)
  ch = NSF1FunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0x820102, s, s, 0x7a);
  item = MenuItem(s, NSEventModifierFlagShift);
  ExpectKeyFiresItem(key, item);

  // esc
  // Turns out this doesn't fire.
  key = KeyEvent(0x100, @"\e", @"\e", 0x35);
  item = MenuItem(@"\e");
  ExpectKeyDoesntFireItem(key, item, false);

  // shift-esc
  // Turns out this doesn't fire.
  key = KeyEvent(0x20102, @"\e", @"\e", 0x35);
  item = MenuItem(@"\e", NSEventModifierFlagShift);
  ExpectKeyDoesntFireItem(key, item, false);

  // cmd-esc
  key = KeyEvent(0x100108, @"\e", @"\e", 0x35);
  item = MenuItem(@"\e", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // ctrl-esc
  key = KeyEvent(0x40101, @"\e", @"\e", 0x35);
  item = MenuItem(@"\e", NSEventModifierFlagControl);
  ExpectKeyFiresItem(key, item);

  // delete ("backspace")
  key = KeyEvent(0x100, @"\x7f", @"\x7f", 0x33);
  item = MenuItem(@"\x08");
  ExpectKeyFiresItem(key, item, false);

  // shift-delete
  key = KeyEvent(0x20102, @"\x7f", @"\x7f", 0x33);
  item = MenuItem(@"\x08", NSEventModifierFlagShift);
  ExpectKeyFiresItem(key, item, false);

  // forwarddelete (fn-delete / fn-backspace)
  ch = NSDeleteFunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0x800100, s, s, 0x75);
  item = MenuItem(@"\x7f");
  ExpectKeyFiresItem(key, item, false);

  // shift-forwarddelete (shift-fn-delete / shift-fn-backspace)
  ch = NSDeleteFunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0x820102, s, s, 0x75);
  item = MenuItem(@"\x7f", NSEventModifierFlagShift);
  ExpectKeyFiresItem(key, item, false);

  // fn-left
  ch = NSHomeFunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0x800100, s, s, 0x73);
  item = MenuItem(s);
  ExpectKeyFiresItem(key, item);

  // cmd-left
  ch = NSLeftArrowFunctionKey;
  s = [NSString stringWithCharacters:&ch length:1];
  key = KeyEvent(0xb00108, s, s, 0x7b);
  item = MenuItem(s, NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // Hitting the "a" key with a russian keyboard layout -- does not fire
  // a menu item that has "a" as key equiv.
  key = KeyEvent(0x100, @"\u0444", @"\u0444", 0);
  item = MenuItem(@"a");
  ExpectKeyDoesntFireItem(key, item);

  // cmd-a on a russion layout -- fires for a menu item with cmd-a as key equiv.
  key = KeyEvent(0x100108, @"a", @"\u0444", 0);
  item = MenuItem(@"a", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item, false);

  // cmd-z on US layout
  key = KeyEvent(0x100108, @"z", @"z", 6);
  item = MenuItem(@"z", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // cmd-y on german layout (has same keycode as cmd-z on us layout, shouldn't
  // fire).
  key = KeyEvent(0x100108, @"y", @"y", 6);
  item = MenuItem(@"z", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);

  // cmd-z on german layout
  key = KeyEvent(0x100108, @"z", @"z", 0x10);
  item = MenuItem(@"z", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // fn-return (== enter)
  key = KeyEvent(0x800100, @"\x3", @"\x3", 0x4c);
  item = MenuItem(@"\r");
  ExpectKeyDoesntFireItem(key, item);

  // cmd-z on dvorak layout (so that the key produces ';')
  key = KeyEvent(0x100108, @";", @";", 6);
  ExpectKeyDoesntFireItem(key, MenuItem(@"z", NSEventModifierFlagCommand));
  ExpectKeyFiresItem(key, MenuItem(@";", NSEventModifierFlagCommand));

  // Change to Command-QWERTY
  SetIsInputSourceCommandQwertyForTesting(true);

  // cmd-z on dvorak qwerty layout (so that the key produces ';', but 'z' if
  // cmd is down)
  key = KeyEvent(0x100108, @"z", @";", 6);
  ExpectKeyFiresItem(key, MenuItem(@"z", NSEventModifierFlagCommand), false);
  ExpectKeyDoesntFireItem(key, MenuItem(@";", NSEventModifierFlagCommand),
                          false);

  // On dvorak-qwerty, pressing the keys for 'cmd' and '=' triggers an event
  // whose characters are cmd-'+'.
  // cmd-'+' on dvorak qwerty should not trigger a menu item for cmd-']', and
  // not a menu item for cmd-'+'.
  key = KeyEvent(0x100108, @"+", @"+", 30);
  ExpectKeyFiresItem(key, MenuItem(@"]", NSEventModifierFlagCommand), false);
  ExpectKeyDoesntFireItem(key, MenuItem(@"+", NSEventModifierFlagCommand),
                          false);

  // cmd-shift-'+' on dvorak qwerty should trigger a menu item for cmd-shift-'}'
  // and not a menu item for cmd-shift-'+'.
  key = KeyEvent(0x12010a, @"}", @"+", 30);
  ExpectKeyFiresItem(key, MenuItem(@"}", NSEventModifierFlagCommand), false);
  ExpectKeyDoesntFireItem(key, MenuItem(@"+", NSEventModifierFlagCommand),
                          false);

  // ctr-shift-tab should trigger correctly.
  key = KeyEvent(0x60103, @"\x19", @"\x19", 48);
  ExpectKeyFiresItem(
      key,
      MenuItem(@"\x9", NSEventModifierFlagShift | NSEventModifierFlagControl),
      false);
  ExpectKeyDoesntFireItem(key, MenuItem(@"\x9", NSEventModifierFlagControl),
                          false);

  // Change away from Command-QWERTY
  SetIsInputSourceCommandQwertyForTesting(false);

  // With Dvorak Right or Left, some of the number row keys produce
  // letters. Ensure that we ignore the key code when taking input
  // from Dvorak RL.

  const NSUInteger keyCodeForEightKey = 28;
  key = KeyEvent(0x100110, @"8", @"8", keyCodeForEightKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"8", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  const NSUInteger keyCodeForNumericKeypadEightKey = 91;
  key = KeyEvent(0x100110, @"8", @"8", keyCodeForNumericKeypadEightKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"8", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  const NSUInteger keyCodeForFiveKey = 23;
  key = KeyEvent(0x100110, @"5", @"5", keyCodeForFiveKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"5", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  const NSUInteger keyCodeForNumericKeypadFiveKey = 87;
  key = KeyEvent(0x100110, @"5", @"5", keyCodeForNumericKeypadFiveKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"5", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  SetIsInputSourceDvorakRightOrLeftForTesting(true);

  // Under Dvorak Right, the eight key is the letter "f".
  key = KeyEvent(0x100110, @"f", @"f", keyCodeForEightKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);
  item = MenuItem(@"8", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);

  // Pressing the eight key on the numeric keypad should switch tabs.
  key = KeyEvent(0x100110, @"8", @"8", keyCodeForNumericKeypadEightKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"8", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  // Under Dvorak Left, the five key is the letter "f".
  key = KeyEvent(0x100110, @"f", @"f", keyCodeForFiveKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);
  item = MenuItem(@"5", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);

  // Pressing the five key on the numeric keypad should switch tabs.
  key = KeyEvent(0x100110, @"5", @"5", keyCodeForNumericKeypadFiveKey);
  item = MenuItem(@"f", NSEventModifierFlagCommand);
  ExpectKeyDoesntFireItem(key, item);
  item = MenuItem(@"5", NSEventModifierFlagCommand);
  ExpectKeyFiresItem(key, item);

  SetIsInputSourceDvorakRightOrLeftForTesting(false);

  // cmd-shift-z on dvorak layout (so that we get a ':')
  key = KeyEvent(0x12010a, @";", @":", 6);
  ExpectKeyFiresItem(key, MenuItem(@":", NSEventModifierFlagCommand));
  ExpectKeyDoesntFireItem(key, MenuItem(@";", NSEventModifierFlagCommand));

  // On PT layout, caps lock should not affect the keyEquivalent.
  key = KeyEvent(0x110108, @"T", @"t", 17);
  ExpectKeyFiresItem(key, MenuItem(@"t", NSEventModifierFlagCommand));
  ExpectKeyDoesntFireItem(key, MenuItem(@"T", NSEventModifierFlagCommand));

  // cmd-s with a serbian layout (just "s" produces something that looks a lot
  // like "c" in some fonts, but is actually \u0441. cmd-s activates a menu item
  // with key equivalent "s", not "c")
  key = KeyEvent(0x100108, @"s", @"\u0441", 1);
  ExpectKeyFiresItem(key, MenuItem(@"s", NSEventModifierFlagCommand), false);
  ExpectKeyDoesntFireItem(key, MenuItem(@"c", NSEventModifierFlagCommand));

  // ctr + shift + tab produces the "End of Medium" keyEquivalent, even though
  // it should produce the "Horizontal Tab" keyEquivalent. Check to make sure
  // it matches anyways.
  key = KeyEvent(0x60103, @"\x19", @"\x19", 1);
  ExpectKeyFiresItem(
      key,
      MenuItem(@"\x9", NSEventModifierFlagShift | NSEventModifierFlagControl),
      false);

  // In 2-set Korean layout, (cmd + shift + t) and (cmd + t) both produce
  // multi-byte unmodified chars. For keyEquivalent purposes, we use their
  // raw characters, where "shift" should be handled correctly.
  key = KeyEvent(0x100108, @"t", @"\u3145", 17);
  ExpectKeyFiresItem(key, MenuItem(@"t", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);
  ExpectKeyDoesntFireItem(key, MenuItem(@"T", NSEventModifierFlagCommand),
                          /*compareCocoa=*/false);

  key = KeyEvent(0x12010a, @"t", @"\u3146", 17);
  ExpectKeyDoesntFireItem(key, MenuItem(@"t", NSEventModifierFlagCommand),
                          /*compareCocoa=*/false);
  ExpectKeyFiresItem(key, MenuItem(@"T", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);

  // On Czech layout, cmd + '+' should instead trigger cmd + '1'.
  key = KeyEvent(0x100108, @"1", @"+", 18);
  ExpectKeyFiresItem(key, MenuItem(@"1", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);

  // On Vietnamese layout, cmd + '' [vkeycode = 18] should instead trigger cmd +
  // '1'. Ditto for other number keys.
  key = KeyEvent(0x100108, @"1", @"", 18);
  ExpectKeyFiresItem(key, MenuItem(@"1", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);
  key = KeyEvent(0x100108, @"4", @"", 21);
  ExpectKeyFiresItem(key, MenuItem(@"4", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);

  // On French AZERTY layout, cmd + '&' [vkeycode = 18] should instead trigger
  // cmd + '1'. Ditto for other number keys.
  key = KeyEvent(0x100108, @"&", @"&", 18);
  ExpectKeyFiresItem(key, MenuItem(@"1", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);
  key = KeyEvent(0x100108, @"é", @"é", 19);
  ExpectKeyFiresItem(key, MenuItem(@"2", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);

  // In Hebrew layout, make sure Cmd-q works.
  key = KeyEvent(0x100110, @"q", @"/", 12);
  ExpectKeyDoesntFireItem(key, MenuItem(@"q", NSEventModifierFlagCommand),
                          /*compareCocoa=*/false);

  SetIsInputSourceCommandHebrewForTesting(true);

  ExpectKeyFiresItem(key, MenuItem(@"q", NSEventModifierFlagCommand),
                     /*compareCocoa=*/false);

  SetIsInputSourceCommandHebrewForTesting(false);
}

// With the Persian - Standard layout, pressing Cmd W without Shift but with
// Caps Lock on generates a key event with a capital W. Make sure we treat
// the W as lower case so that we don't match Shift Cmd W, unless the user
// is also holding down the Shift key.
TEST(NSMenuItemAdditionsTest, TestCmdCapsLockOnPersianStandardLayout) {
  NSString* capitalW = @"W";
  NSMenuItem* closeTabItem = MenuItem(@"w", NSEventModifierFlagCommand);
  NSMenuItem* closeWindowItem = MenuItem(capitalW, NSEventModifierFlagCommand);

  // Simulate pressing Cmd W with Caps Lock on.
  NSEvent* cmdWWithCapsLock =
      KeyEvent(NSEventModifierFlagCommand | NSEventModifierFlagCapsLock,
               capitalW, @"\u0635", kVK_ANSI_W);
  // The layout generates an event with a capital W. We have to force the
  // characters because the regular NSEvent machinery insists on converting
  // the string to lower case.
  [base::mac::ObjCCastStrict<NSEventForTesting>(cmdWWithCapsLock)
      setCharacters:capitalW];
  ExpectKeyFiresItem(cmdWWithCapsLock, closeTabItem, /*compareCocoa=*/false);

  // Make sure Shift-Cmd W triggers Close Window.
  NSEvent* shiftCmdW =
      KeyEvent(NSEventModifierFlagCommand | NSEventModifierFlagShift, capitalW,
               @"\u1612", kVK_ANSI_W);
  [base::mac::ObjCCastStrict<NSEventForTesting>(shiftCmdW)
      setCharacters:capitalW];
  ExpectKeyFiresItem(shiftCmdW, closeWindowItem, /*compareCocoa=*/false);

  // And also Shift-Cmd W with Caps Lock down.
  NSEvent* shiftCmdWWithCapsLock =
      KeyEvent(NSEventModifierFlagCommand | NSEventModifierFlagShift |
                   NSEventModifierFlagCapsLock,
               capitalW, @"\u1612", kVK_ANSI_W);
  [base::mac::ObjCCastStrict<NSEventForTesting>(shiftCmdWWithCapsLock)
      setCharacters:capitalW];
  ExpectKeyFiresItem(shiftCmdWWithCapsLock, closeWindowItem,
                     /*compareCocoa=*/false);
}

NSString* keyCodeToCharacter(NSUInteger keyCode,
                             EventModifiers modifiers,
                             TISInputSourceRef layout) {
  UInt32 deadKeyStateUnused = 0;
  UniChar unicodeChar = ui::TranslatedUnicodeCharFromKeyCode(
      layout, (UInt16)keyCode, kUCKeyActionDown, modifiers, LMGetKbdType(),
      &deadKeyStateUnused);

  CFStringRef temp =
      CFStringCreateWithCharacters(kCFAllocatorDefault, &unicodeChar, 1);
  return [(NSString*)temp autorelease];
}

TEST(NSMenuItemAdditionsTest, TestMOnDifferentLayouts) {
  // There's one key -- "m" -- that has the same keycode on most keyboard
  // layouts. This function tests a menu item with cmd-m as key equivalent
  // can be fired on all layouts.
  NSMenuItem* item = MenuItem(@"m", NSEventModifierFlagCommand);

  NSDictionary* filter = [NSDictionary
      dictionaryWithObject:(NSString*)kTISTypeKeyboardLayout
                    forKey:(NSString*)kTISPropertyInputSourceType];

  // Docs say that including all layouts instead of just the active ones is
  // slow, but there's no way around that.
  NSArray* list =
      (NSArray*)TISCreateInputSourceList((CFDictionaryRef)filter, true);
  for (id layout in list) {
    TISInputSourceRef ref = (TISInputSourceRef)layout;

    NSUInteger keyCode = 0x2e;  // "m" on a US layout and most other layouts.

    // On a few layouts, "m" has a different key code.
    NSString* layoutId =
        (NSString*)TISGetInputSourceProperty(ref, kTISPropertyInputSourceID);
    if ([layoutId isEqualToString:@"com.apple.keylayout.Belgian"] ||
        [layoutId isEqualToString:@"com.apple.keylayout.Italian"] ||
        [layoutId isEqualToString:@"com.apple.keylayout.ABC-AZERTY"] ||
        [layoutId hasPrefix:@"com.apple.keylayout.French"]) {
      keyCode = 0x29;
    } else if ([layoutId isEqualToString:@"com.apple.keylayout.Turkish"] ||
               [layoutId
                   isEqualToString:@"com.apple.keylayout.Turkish-Standard"]) {
      keyCode = 0x28;
    } else if ([layoutId isEqualToString:@"com.apple.keylayout.Dvorak-Left"]) {
      keyCode = 0x16;
    } else if ([layoutId isEqualToString:@"com.apple.keylayout.Dvorak-Right"]) {
      keyCode = 0x1a;
    } else if ([layoutId
                   isEqualToString:@"com.apple.keylayout.Tibetan-Wylie"]) {
      // In Tibetan-Wylie, the only way to type the "m" character is with cmd +
      // key_code=0x2e. As such, it doesn't make sense for this same combination
      // to trigger a keyEquivalent, since then it won't be possible to type
      // "m".
      continue;
    } else if ([layoutId isEqualToString:@"com.apple.keylayout.Geez-QWERTY"]) {
      // There is no way to type an "m" using the Amharic keyboard. It's
      // designed for the Ge'ez language.
      continue;
    } else if (IsCommandlessCyrillicLayout(layoutId)) {
      // Commandless layouts have no way to trigger a menu key equivalent at
      // all, in any app.
      continue;
    }

    if (IsKeyboardLayoutCommandQwerty(layoutId)) {
      SetIsInputSourceCommandQwertyForTesting(true);
    }

    EventModifiers modifiers = cmdKey >> 8;
    NSString* chars = keyCodeToCharacter(keyCode, modifiers, ref);
    NSString* charsIgnoringMods = keyCodeToCharacter(keyCode, 0, ref);
    NSEvent* key =
        KeyEvent(NSEventModifierFlagCommand, chars, charsIgnoringMods, keyCode);
    if ([layoutId isEqualToString:@"com.apple.keylayout.Dvorak-Left"] ||
        [layoutId isEqualToString:@"com.apple.keylayout.Dvorak-Right"]) {
      // On Dvorak, we expect this comparison to fail because the cmd + <keycode
      // for numerical key> will always trigger tab switching. This causes
      // Chrome to match the behavior of Safari, and has been expected by users
      // of every other keyboard layout.
      ExpectKeyDoesntFireItem(key, item, false, layoutId);
    } else {
      ExpectKeyFiresItem(key, item, false, layoutId);
    }

    if (IsKeyboardLayoutCommandQwerty(layoutId)) {
      SetIsInputSourceCommandQwertyForTesting(false);
    }
  }
  CFRelease(list);
}

// Tests that ModifierMaskForKeyEvent() works correctly for "flags changed"
// events.
TEST(NSMenuItemAdditionsTest, MMFKEHandlesFlagsChangedEvents) {
  NSEventModifierFlags modifiers = NSEventModifierFlagCommand;
  NSEvent* flags_changed_event =
      [NSEvent keyEventWithType:NSEventTypeFlagsChanged
                             location:NSZeroPoint
                        modifierFlags:modifiers
                            timestamp:0.0
                         windowNumber:0
                              context:nil
                           characters:@""
          charactersIgnoringModifiers:@""
                            isARepeat:NO
                              keyCode:0];

  NSEventModifierFlags expected_flags =
      NSEventModifierFlagCommand | NSEventModifierFlagControl |
      NSEventModifierFlagOption | NSEventModifierFlagShift;
  // Flags changed events don't have characters. Make sure we don't make the
  // mistake of of assuming the event contains characters.
  EXPECT_EQ(expected_flags, ModifierMaskForKeyEvent(flags_changed_event));

  modifiers = NSEventModifierFlagFunction;
  flags_changed_event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
                                         location:NSZeroPoint
                                    modifierFlags:modifiers
                                        timestamp:0.0
                                     windowNumber:0
                                          context:nil
                                       characters:@""
                      charactersIgnoringModifiers:@""
                                        isARepeat:NO
                                          keyCode:0];

  // Make sure, in particular, we handle flags changed for the function key.
  // See https://crbug.com/1340934 .
  EXPECT_EQ(expected_flags, ModifierMaskForKeyEvent(flags_changed_event));

  modifiers = NSEventModifierFlagOption;
  NSEvent* key_up_event = [NSEvent keyEventWithType:NSEventTypeKeyUp
                                           location:NSZeroPoint
                                      modifierFlags:modifiers
                                          timestamp:0.0
                                       windowNumber:0
                                            context:nil
                                         characters:@"a"
                        charactersIgnoringModifiers:@"a"
                                          isARepeat:NO
                                            keyCode:0];

  // Check that there are no issues with key up events.
  EXPECT_EQ(expected_flags, ModifierMaskForKeyEvent(key_up_event));

  modifiers = NSEventModifierFlagFunction;
  NSEvent* empty_chars_event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                                location:NSZeroPoint
                                           modifierFlags:modifiers
                                               timestamp:0.0
                                            windowNumber:0
                                                 context:nil
                                              characters:@""
                             charactersIgnoringModifiers:@""
                                               isARepeat:NO
                                                 keyCode:0];

  // Make sure we correctly handle the situation of function key press event
  // with no characters (dead keys).
  EXPECT_EQ(expected_flags, ModifierMaskForKeyEvent(empty_chars_event));
}

}  // namespace
}  // namespace cocoa
}  // namespace ui
