// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_
#define UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/views/views_export.h"

namespace ui {
struct AXActionData;
struct AXNodeData;
struct AXTreeData;
}  // namespace ui

namespace views {

class AXAuraObjCache;
class AXAuraObjWrapper;

// This class exposes the views hierarchy as an accessibility tree permitting
// use with other accessibility classes. Subclasses must implement GetRoot().
// The root can be an existing object in the Widget/View hierarchy or a new node
// (for example to create the "desktop" node for the extension API call
// chrome.automation.getDesktop()).
class VIEWS_EXPORT AXTreeSourceViews
    : public ui::AXTreeSource<AXAuraObjWrapper*> {
 public:
  AXTreeSourceViews(AXAuraObjWrapper* root,
                    const ui::AXTreeID& tree_id,
                    AXAuraObjCache* cache);
  AXTreeSourceViews(const AXTreeSourceViews&) = delete;
  AXTreeSourceViews& operator=(const AXTreeSourceViews&) = delete;
  ~AXTreeSourceViews() override;

  // Invokes an action on an Aura object.
  void HandleAccessibleAction(const ui::AXActionData& action);

  // AXTreeSource:
  bool GetTreeData(ui::AXTreeData* data) const override;
  AXAuraObjWrapper* GetRoot() const override;
  AXAuraObjWrapper* GetFromId(int32_t id) const override;
  int32_t GetId(AXAuraObjWrapper* node) const override;
  void CacheChildrenIfNeeded(AXAuraObjWrapper*) override;
  size_t GetChildCount(AXAuraObjWrapper* node) const override;
  void ClearChildCache(AXAuraObjWrapper*) override;
  AXAuraObjWrapper* ChildAt(AXAuraObjWrapper* node, size_t) const override;
  AXAuraObjWrapper* GetParent(AXAuraObjWrapper* node) const override;
  bool IsIgnored(AXAuraObjWrapper* node) const override;
  bool IsEqual(AXAuraObjWrapper* node1, AXAuraObjWrapper* node2) const override;
  AXAuraObjWrapper* GetNull() const override;
  std::string GetDebugString(AXAuraObjWrapper* node) const override;
  void SerializeNode(AXAuraObjWrapper* node,
                     ui::AXNodeData* out_data) const override;

  // Useful for debugging.
  std::string ToString(views::AXAuraObjWrapper* root, std::string prefix);

  const ui::AXTreeID tree_id() const { return tree_id_; }

 private:
  // The top-level object to use for the AX tree. See class comment.
  const raw_ptr<AXAuraObjWrapper, DanglingUntriaged> root_ = nullptr;

  // ID to use for the AX tree.
  const ui::AXTreeID tree_id_;

  raw_ptr<views::AXAuraObjCache, DanglingUntriaged> cache_;
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_AX_TREE_SOURCE_VIEWS_H_
