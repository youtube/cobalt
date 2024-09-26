// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>
#include <string>
#include <vector>

#import "base/mac/scoped_nsobject.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_position.h"
#include "ui/accessibility/ax_range.h"
#include "ui/accessibility/platform/ax_platform_node_cocoa.h"
#include "ui/accessibility/platform/ax_platform_node_mac.h"

namespace content {

// Used to store changes in edit fields, required by VoiceOver in order to
// support character echo and other announcements during editing.
struct CONTENT_EXPORT AXTextEdit {
  AXTextEdit();
  AXTextEdit(std::u16string inserted_text,
             std::u16string deleted_text,
             id edit_text_marker);
  AXTextEdit(const AXTextEdit& other);
  ~AXTextEdit();

  bool IsEmpty() const { return inserted_text.empty() && deleted_text.empty(); }

  std::u16string inserted_text;
  std::u16string deleted_text;
  base::scoped_nsprotocol<id> edit_text_marker;
};

// Returns true if the given object is an NSRange instance.
bool IsNSRange(id value);

// Returns an AXTextMarker representing the given position in the tree.
id AXTextMarkerFrom(const BrowserAccessibilityCocoa* anchor,
                    int offset,
                    ax::mojom::TextAffinity affinity);

// Returns an AXTextMarkerRange that spans the given AXTextMarkers.
id AXTextMarkerRangeFrom(id anchor_text_marker, id focus_text_marker);

}  // namespace content

// BrowserAccessibilityCocoa is a cocoa wrapper around the BrowserAccessibility
// object. The renderer converts webkit's accessibility tree into a
// WebAccessibility tree and passes it to the browser process over IPC.
// This class converts it into a format Cocoa can query.
@interface BrowserAccessibilityCocoa : AXPlatformNodeCocoa {
 @private
  raw_ptr<content::BrowserAccessibility> _owner;
  // An array of children of this object. Cached to avoid re-computing.
  base::scoped_nsobject<NSMutableArray> _children;
  // Whether the children have changed and need to be updated.
  bool _needsToUpdateChildren;
  // Whether _children is currently being computed.
  bool _gettingChildren;
  // Stores the previous value of an edit field.
  std::u16string _oldValue;
}

// This creates a cocoa browser accessibility object around
// the cross platform BrowserAccessibility object, which can't be nullptr.
- (instancetype)initWithObject:(content::BrowserAccessibility*)accessibility
              withPlatformNode:(ui::AXPlatformNodeMac*)platform_node;

// Clear this object's pointer to the wrapped BrowserAccessibility object
// because the wrapped object has been deleted, but this object may
// persist if the system still has references to it.
- (void)detach;

// Invalidate children for a non-ignored ancestor (including self).
- (void)childrenChanged;

// Get the BrowserAccessibility that this object wraps.
- (content::BrowserAccessibility*)owner;

// Computes the text that was added or deleted in a text field after an edit.
- (content::AXTextEdit)computeTextEdit;

// Convert from the view's local coordinate system (with the origin in the upper
// left) to the primary NSScreen coordinate system (with the origin in the lower
// left).
- (NSRect)rectInScreen:(gfx::Rect)rect;

- (void)getTreeItemDescendantNodeIds:(std::vector<int32_t>*)tree_item_ids;

// Return the method name for the given attribute. For testing only.
- (NSString*)methodNameForAttribute:(NSString*)attribute;

- (NSString*)valueForRange:(NSRange)range;
- (NSRect)frameForRange:(NSRange)range;

// Find the index of the given row among the descendants of this object
// or return nil if this row is not found.
- (bool)findRowIndex:(BrowserAccessibilityCocoa*)toFind
    withCurrentIndex:(int*)currentIndex;

// Choose the appropriate accessibility object to receive an action depending
// on the characteristics of this accessibility node.
- (content::BrowserAccessibility*)actionTarget;

@property(nonatomic, readonly) NSArray* children;
@property(nonatomic, readonly) NSArray* columns;
@property(nonatomic, readonly) NSValue* columnIndexRange;
@property(nonatomic, readonly) NSNumber* disclosing;
@property(nonatomic, readonly) id disclosedByRow;
@property(nonatomic, readonly) NSNumber* disclosureLevel;
@property(nonatomic, readonly) id disclosedRows;
@property(nonatomic, readonly) NSNumber* enabled;
// Returns a text marker that points to the last character in the document that
// can be selected with Voiceover.
@property(nonatomic, readonly) id endTextMarker;
@property(nonatomic, readonly) NSNumber* expanded;
@property(nonatomic, readonly) NSNumber* focused;
@property(nonatomic, readonly) id header;
// Index of a row, column, or tree item.
@property(nonatomic, readonly) NSNumber* index;
@property(nonatomic, readonly) NSNumber* treeItemRowIndex;
@property(nonatomic, readonly) NSNumber* insertionPointLineNumber;
@property(nonatomic, readonly) NSNumber* maxValue;
@property(nonatomic, readonly) NSNumber* minValue;
@property(nonatomic, readonly) NSNumber* numberOfCharacters;
@property(nonatomic, readonly) NSString* orientation;
@property(nonatomic, readonly) id parent;
@property(nonatomic, readonly) NSValue* position;
// A string indicating the role of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* role;
@property(nonatomic, readonly) NSArray* rowHeaders;
@property(nonatomic, readonly) NSValue* rowIndexRange;
@property(nonatomic, readonly) NSArray* selectedChildren;
@property(nonatomic, readonly) NSString* selectedText;
@property(nonatomic, readonly) NSValue* selectedTextRange;
@property(nonatomic, readonly) id selectedTextMarkerRange;
@property(nonatomic, readonly) NSString* sortDirection;
// Returns a text marker that points to the first character in the document that
// can be selected with Voiceover.
@property(nonatomic, readonly) id startTextMarker;
// A string indicating the subrole of this object as far as accessibility
// is concerned.
@property(nonatomic, readonly) NSString* subrole;
// The tabs owned by a tablist.
@property(nonatomic, readonly) NSArray* tabs;
@property(nonatomic, readonly) NSString* value;
@property(nonatomic, readonly) NSString* valueDescription;
@property(nonatomic, readonly) NSValue* visibleCharacterRange;
@property(nonatomic, readonly) NSArray* visibleCells;
@property(nonatomic, readonly) NSArray* visibleChildren;
@property(nonatomic, readonly) NSArray* visibleColumns;
@property(nonatomic, readonly) NSArray* visibleRows;
@property(nonatomic, readonly) id window;
@end

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_COCOA_H_
