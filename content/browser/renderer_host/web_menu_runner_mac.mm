// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/web_menu_runner_mac.h"

#include <stddef.h>

#include "base/base64.h"
#include "base/strings/sys_string_conversions.h"

@interface WebMenuRunner (PrivateAPI)

// Worker function used during initialization.
- (void)addItem:(const blink::mojom::MenuItemPtr&)item;

// A callback for the menu controller object to call when an item is selected
// from the menu. This is not called if the menu is dismissed without a
// selection.
- (void)menuItemSelected:(id)sender;

@end  // WebMenuRunner (PrivateAPI)

@implementation WebMenuRunner {
  // The native menu control.
  NSMenu* __strong _menu;

  // A flag set to YES if a menu item was chosen, or NO if the menu was
  // dismissed without selecting an item.
  BOOL _menuItemWasChosen;

  // The index of the selected menu item.
  int _index;

  // The font size being used for the menu.
  CGFloat _fontSize;

  // Whether the menu should be displayed right-aligned.
  BOOL _rightAligned;
}

- (id)initWithItems:(const std::vector<blink::mojom::MenuItemPtr>&)items
           fontSize:(CGFloat)fontSize
       rightAligned:(BOOL)rightAligned {
  if ((self = [super init])) {
    _menu = [[NSMenu alloc] initWithTitle:@""];
    _menu.autoenablesItems = NO;
    _index = -1;
    _fontSize = fontSize;
    _rightAligned = rightAligned;
    for (const auto& item : items) {
      [self addItem:item];
    }
  }
  return self;
}

- (void)addItem:(const blink::mojom::MenuItemPtr&)item {
  if (item->type == blink::mojom::MenuItem::Type::kSeparator) {
    [_menu addItem:[NSMenuItem separatorItem]];
    return;
  }

  NSString* title = base::SysUTF8ToNSString(item->label.value_or(""));
  // https://crbug.com/1140620: SysUTF8ToNSString will return nil if the bits
  // that it is passed cannot be turned into a CFString. If this nil value is
  // passed to -[NSMenuItem addItemWithTitle:action:keyEquivalent], Chromium
  // will crash. Therefore, for debugging, if the result is nil, substitute in
  // the raw bytes, encoded for safety in base64, to allow for investigation.
  if (!title) {
    std::string base64;
    base::Base64Encode(*item->label, &base64);
    title = base::SysUTF8ToNSString(base64);
  }
  NSMenuItem* menuItem = [_menu addItemWithTitle:title
                                          action:@selector(menuItemSelected:)
                                   keyEquivalent:@""];
  if (item->tool_tip.has_value()) {
    NSString* toolTip = base::SysUTF8ToNSString(item->tool_tip.value());
    [menuItem setToolTip:toolTip];
  }
  [menuItem setEnabled:(item->enabled &&
                        item->type != blink::mojom::MenuItem::Type::kGroup)];
  [menuItem setTarget:self];

  // Set various alignment/language attributes.
  NSMutableDictionary* attrs = [[NSMutableDictionary alloc] initWithCapacity:3];
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment =
      _rightAligned ? NSTextAlignmentRight : NSTextAlignmentLeft;
  NSWritingDirection writingDirection =
      item->text_direction == base::i18n::RIGHT_TO_LEFT
          ? NSWritingDirectionRightToLeft
          : NSWritingDirectionLeftToRight;
  paragraphStyle.baseWritingDirection = writingDirection;
  attrs[NSParagraphStyleAttributeName] = paragraphStyle;

  if (item->has_text_direction_override) {
    attrs[NSWritingDirectionAttributeName] =
        @[ @(long{writingDirection} | NSWritingDirectionOverride) ];
  }

  attrs[NSFontAttributeName] = [NSFont menuFontOfSize:_fontSize];

  NSAttributedString* attrTitle =
      [[NSAttributedString alloc] initWithString:title attributes:attrs];
  menuItem.attributedTitle = attrTitle;

  // We set the title as well as the attributed title here. The attributed title
  // will be displayed in the menu, but typeahead will use the non-attributed
  // string that doesn't contain any leading or trailing whitespace. This is
  // what Apple uses in WebKit as well:
  // http://trac.webkit.org/browser/trunk/Source/WebKit2/UIProcess/mac/WebPopupMenuProxyMac.mm#L90
  NSCharacterSet* whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
  [menuItem setTitle:[title stringByTrimmingCharactersInSet:whitespaceSet]];

  [menuItem setTag:[_menu numberOfItems] - 1];
}

// Reflects the result of the user's interaction with the popup menu. If NO, the
// menu was dismissed without the user choosing an item, which can happen if the
// user clicked outside the menu region or hit the escape key. If YES, the user
// selected an item from the menu.
- (BOOL)menuItemWasChosen {
  return _menuItemWasChosen;
}

- (void)menuItemSelected:(id)sender {
  _menuItemWasChosen = YES;
}

- (void)runMenuInView:(NSView*)view
           withBounds:(NSRect)bounds
         initialIndex:(int)index {
  // Set up the button cell, converting to NSView coordinates. The menu is
  // positioned such that the currently selected menu item appears over the
  // popup button, which is the expected Mac popup menu behavior.
  NSPopUpButtonCell* cell = [[NSPopUpButtonCell alloc] initTextCell:@""
                                                          pullsDown:NO];
  cell.menu = _menu;
  // We use selectItemWithTag below so if the index is out-of-bounds nothing
  // bad happens.
  [cell selectItemWithTag:index];

  if (_rightAligned) {
    cell.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
    _menu.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionRightToLeft;
  }

  // When popping up a menu near the Dock, Cocoa restricts the menu
  // size to not overlap the Dock, with a scroll arrow.  Below a
  // certain point this doesn't work.  At that point the menu is
  // popped up above the element, so that the current item can be
  // selected without mouse-tracking selecting a different item
  // immediately.
  //
  // Unfortunately, instead of popping up above the passed |bounds|,
  // it pops up above the bounds of the view passed to inView:.  Use a
  // dummy view to fake this out.
  NSView* dummyView = [[NSView alloc] initWithFrame:bounds];
  [view addSubview:dummyView];

  // Display the menu, and set a flag if a menu item was chosen.
  [cell attachPopUpWithFrame:dummyView.bounds inView:dummyView];
  [cell performClickWithFrame:dummyView.bounds inView:dummyView];

  [dummyView removeFromSuperview];

  if ([self menuItemWasChosen])
    _index = [cell indexOfSelectedItem];
}

- (void)hide {
  [_menu cancelTracking];
}

- (int)indexOfSelectedItem {
  return _index;
}

@end  // WebMenuRunner
