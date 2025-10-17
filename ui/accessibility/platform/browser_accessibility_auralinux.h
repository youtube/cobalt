// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_AURALINUX_H_

#include "ui/accessibility/platform/browser_accessibility.h"
#include "base/component_export.h"
#include "ui/accessibility/ax_node.h"

namespace ui {
class AXPlatformNodeAuraLinux;

class BrowserAccessibilityAuraLinux : public BrowserAccessibility {
 public:
  BrowserAccessibilityAuraLinux(BrowserAccessibilityManager* manager,
                                AXNode* node);
  ~BrowserAccessibilityAuraLinux() override;
  BrowserAccessibilityAuraLinux(const BrowserAccessibilityAuraLinux&) = delete;
  BrowserAccessibilityAuraLinux& operator=(
      const BrowserAccessibilityAuraLinux&) = delete;

  COMPONENT_EXPORT(AX_PLATFORM) AXPlatformNodeAuraLinux* GetNode() const;

  COMPONENT_EXPORT(AX_PLATFORM)
  BrowserAccessibilityAuraLinux* GetExtraAnnouncementNode(
      ax::mojom::AriaNotificationPriority priority_property) const;

  // This is used to call UpdateHypertext, when a node needs to be
  // updated for some other reason other than via OnAtomicUpdateFinished.
  void UpdatePlatformAttributes() override;

  // BrowserAccessibility methods.
  void OnDataChanged() override;
  size_t PlatformChildCount() const override;
  BrowserAccessibility* PlatformGetChild(size_t child_index) const override;
  BrowserAccessibility* PlatformGetLastChild() const override;
  BrowserAccessibility* PlatformGetNextSibling() const override;
  BrowserAccessibility* PlatformGetPreviousSibling() const override;

  AXPlatformNode* GetAXPlatformNode() const override;
  std::u16string GetHypertext() const override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;

  TextAttributeList ComputeTextAttributes() const override;

  void SetIsPrimaryWebContentsForWindow() override;
  bool IsPrimaryWebContentsForWindow() const override;

 private:
  bool HasExtraAnnouncementNodes() const;

  AXPlatformNode::Pointer platform_node_;
};

COMPONENT_EXPORT(AX_PLATFORM) BrowserAccessibilityAuraLinux* ToBrowserAccessibilityAuraLinux(
    BrowserAccessibility* obj);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_BROWSER_ACCESSIBILITY_AURALINUX_H_
