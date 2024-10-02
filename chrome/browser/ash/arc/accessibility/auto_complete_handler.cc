// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/accessibility/auto_complete_handler.h"

#include "ash/components/arc/mojom/accessibility_helper.mojom-forward.h"
#include "chrome/browser/ash/arc/accessibility/accessibility_info_data_wrapper.h"
#include "chrome/browser/ash/arc/accessibility/arc_accessibility_util.h"
#include "chrome/browser/ash/arc/accessibility/ax_tree_source_arc.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_android_constants.h"

namespace {

// The list value aria-autocomplete property.
constexpr char kAutoCompleteListAttribute[] = "list";

bool IsAutoComplete(arc::mojom::AccessibilityNodeInfoData* node) {
  if (!node || !node->string_properties)
    return false;

  auto it = node->string_properties->find(
      arc::mojom::AccessibilityStringProperty::CLASS_NAME);
  if (it == node->string_properties->end())
    return false;

  return it->second == ui::kAXAutoCompleteTextViewClassname ||
         it->second == ui::kAXMultiAutoCompleteTextViewClassname;
}

int32_t GetAnchorId(arc::mojom::AccessibilityWindowInfoData* window) {
  if (!window)
    return ui::kInvalidAXNodeID;

  int32_t id;
  if (arc::GetProperty(
          window->int_properties,
          arc::mojom::AccessibilityWindowIntProperty::ANCHOR_NODE_ID, &id)) {
    return id;
  }
  return ui::kInvalidAXNodeID;
}

}  // namespace

namespace arc {

AutoCompleteHandler::AutoCompleteHandler(const int32_t editable_node_id)
    : anchored_node_id_(editable_node_id) {}

AutoCompleteHandler::~AutoCompleteHandler() = default;

// static
std::vector<AutoCompleteHandler::IdAndHandler>
AutoCompleteHandler::CreateIfNecessary(
    AXTreeSourceArc* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  if (event_data.event_type !=
      mojom::AccessibilityEventType::WINDOW_CONTENT_CHANGED) {
    return {};
  }

  std::vector<IdAndHandler> results;

  // Check all updated nodes under the event source.
  std::vector<AccessibilityInfoDataWrapper*> to_visit;
  to_visit.push_back(tree_source->GetFromId(event_data.source_id));
  while (!to_visit.empty()) {
    AccessibilityInfoDataWrapper* node_ptr = to_visit.back();
    to_visit.pop_back();
    if (!node_ptr)
      continue;

    const int32_t node_id = node_ptr->GetId();
    if (IsAutoComplete(node_ptr->GetNode())) {
      results.emplace_back(node_id,
                           std::make_unique<AutoCompleteHandler>(node_id));
    }

    node_ptr->GetChildren(&to_visit);
  }

  return results;
}

bool AutoCompleteHandler::PreDispatchEvent(
    AXTreeSourceArc* tree_source,
    const mojom::AccessibilityEventData& event_data) {
  if (event_data.event_type == mojom::AccessibilityEventType::WINDOWS_CHANGED) {
    // Check if a popup window anchoring the node exists.
    // The first window is the main window. Look for other windows.
    for (size_t i = 1; i < event_data.window_data->size(); ++i) {
      if (GetAnchorId(event_data.window_data->at(i).get()) !=
          anchored_node_id_) {
        continue;
      }

      int32_t window_id = event_data.window_data->at(i)->window_id;
      if (suggestion_window_id_ != window_id) {
        // Anchoring window has changed.
        suggestion_window_id_ = window_id;
        return true;
      } else {
        // Nothing related changed. No need to update.
        return false;
      }
    }

    if (suggestion_window_id_.has_value()) {
      // No popup window found. The window has disappeared.
      suggestion_window_id_.reset();
      selected_node_id_.reset();
      return true;
    }
    return false;
  } else if (event_data.event_type ==
             mojom::AccessibilityEventType::VIEW_SELECTED) {
    // VIEW_SELECTED event from the suggestion list is a user's selecting action
    // in a candidate.
    if (!suggestion_window_id_.has_value())
      return false;

    AccessibilityInfoDataWrapper* source_node =
        tree_source->GetFromId(event_data.source_id);
    if (!source_node || source_node->GetWindowId() != suggestion_window_id_)
      return false;

    AccessibilityInfoDataWrapper* selected_node =
        GetSelectedNodeInfoFromAdapterViewEvent(event_data, source_node);
    if (!selected_node || selected_node->GetId() == selected_node_id_)
      return false;

    selected_node_id_ = selected_node->GetId();
    return true;
  }
  return false;
}

void AutoCompleteHandler::PostSerializeNode(ui::AXNodeData* out_data) const {
  DCHECK_EQ(out_data->role, ax::mojom::Role::kTextField);

  out_data->AddStringAttribute(ax::mojom::StringAttribute::kAutoComplete,
                               kAutoCompleteListAttribute);

  if (suggestion_window_id_.has_value()) {
    out_data->AddState(ax::mojom::State::kExpanded);
    out_data->AddIntListAttribute(ax::mojom::IntListAttribute::kControlsIds,
                                  {suggestion_window_id_.value()});
    if (selected_node_id_.has_value()) {
      out_data->AddIntAttribute(ax::mojom::IntAttribute::kActivedescendantId,
                                selected_node_id_.value());
    }
  } else {
    out_data->AddState(ax::mojom::State::kCollapsed);
  }
}

}  // namespace arc
