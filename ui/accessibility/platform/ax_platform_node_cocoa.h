// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_

#include <memory>

#import <Accessibility/Accessibility.h>
#import <Cocoa/Cocoa.h>

#include "base/component_export.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

class AXPlatformNodeBase;
class AXPlatformNodeDelegate;

struct AXAnnouncementSpec {
  AXAnnouncementSpec();
  ~AXAnnouncementSpec();

  base::scoped_nsobject<NSString> announcement;
  base::scoped_nsobject<NSWindow> window;
  bool is_polite;
};

}  // namespace ui

COMPONENT_EXPORT(AX_PLATFORM)
@interface AXPlatformNodeCocoa
    : NSAccessibilityElement <NSAccessibility, AXCustomContentProvider>

- (NSArray*)accessibilityCustomContent;

// Determines if this object is alive, i.e. it hasn't been detached.
- (BOOL)instanceActive;

// Returns true if this accessible element should be included into the ax tree.
- (BOOL)isIncludedInPlatformTree;

// Returns true if this object should expose its accessible name using
// accessibilityLabel (legacy AXDescription attribute).
- (BOOL)isNameFromLabel;

// Returns an accessible element serving as a title UI element, an element
// representing the accessible name of the object and which is exposed via
// accessibilityTitleUIElement (or AXTitleUIElement legacy attribute) not via
// accessibilityTitle (or legacy AXTitle attribute) or accessibilityLabel
// (legacy AXDescription attribute).
- (id)titleUIElement;

// Maps AX roles to native roles. Returns NSAccessibilityUnknownRole if not
// found.
+ (NSString*)nativeRoleFromAXRole:(ax::mojom::Role)role;

// Maps AX roles to native subroles. Returns nil if not found.
+ (NSString*)nativeSubroleFromAXRole:(ax::mojom::Role)role;

// Maps AX events to native notifications. Returns nil if not found.
+ (NSString*)nativeNotificationFromAXEvent:(ax::mojom::Event)event;

- (instancetype)initWithNode:(ui::AXPlatformNodeBase*)node;
- (void)detach;

// Returns this node's internal role, i.e. the one that is stored in
// the internal accessibility tree as opposed to the platform tree.
- (ax::mojom::Role)internalRole;

@property(nonatomic, readonly) NSRect boundsInScreen;
@property(nonatomic, readonly) ui::AXPlatformNodeBase* node;
@property(nonatomic, readonly) ui::AXPlatformNodeDelegate* nodeDelegate;

// Returns the data necessary to queue an NSAccessibility announcement if
// |eventType| should be announced, or nullptr otherwise.
- (std::unique_ptr<ui::AXAnnouncementSpec>)announcementForEvent:
    (ax::mojom::Event)eventType;

// Ask the system to announce |announcementText|. This is debounced to happen
// at most every |kLiveRegionDebounceMillis| per node, with only the most
// recent announcement text read, to account for situations with multiple
// notifications happening one after another (for example, results for
// find-in-page updating rapidly as they come in from subframes).
- (void)scheduleLiveRegionAnnouncement:
    (std::unique_ptr<ui::AXAnnouncementSpec>)announcement;

- (id)AXWindow;

@end

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_COCOA_H_
