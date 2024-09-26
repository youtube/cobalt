// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_action_handler_registry.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/strings/string_number_conversions.h"
#include "ui/accessibility/ax_action_handler_base.h"

namespace ui {

// static
AXActionHandlerRegistry* AXActionHandlerRegistry::GetInstance() {
  static base::NoDestructor<AXActionHandlerRegistry> registry;
  return registry.get();
}

void AXActionHandlerRegistry::SetFrameIDForAXTreeID(
    const FrameID& frame_id,
    const AXTreeID& ax_tree_id) {
  auto it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end()) {
    NOTREACHED();
    return;
  }

  frame_to_ax_tree_id_map_[frame_id] = ax_tree_id;
  ax_tree_to_frame_id_map_[ax_tree_id] = frame_id;
}

void AXActionHandlerRegistry::AddObserver(AXActionHandlerObserver* observer) {
  observers_.AddObserver(observer);
}

void AXActionHandlerRegistry::RemoveObserver(
    AXActionHandlerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AXActionHandlerRegistry::PerformAction(
    const ui::AXActionData& action_data) {
  for (AXActionHandlerObserver& observer : observers_) {
    observer.PerformAction(action_data);
  }
}

AXActionHandlerRegistry::FrameID AXActionHandlerRegistry::GetFrameID(
    const AXTreeID& ax_tree_id) {
  auto it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (it != ax_tree_to_frame_id_map_.end())
    return it->second;

  return FrameID(-1, -1);
}

AXTreeID AXActionHandlerRegistry::GetAXTreeID(
    AXActionHandlerRegistry::FrameID frame_id) {
  auto it = frame_to_ax_tree_id_map_.find(frame_id);
  if (it != frame_to_ax_tree_id_map_.end())
    return it->second;

  return ui::AXTreeIDUnknown();
}

AXTreeID AXActionHandlerRegistry::GetOrCreateAXTreeID(
    AXActionHandlerBase* handler) {
  for (auto it : id_to_action_handler_) {
    if (it.second == handler)
      return it.first;
  }
  AXTreeID new_id = AXTreeID::CreateNewAXTreeID();
  SetAXTreeID(new_id, handler);
  return new_id;
}

AXActionHandlerBase* AXActionHandlerRegistry::GetActionHandler(
    AXTreeID ax_tree_id) {
  auto it = id_to_action_handler_.find(ax_tree_id);
  if (it == id_to_action_handler_.end())
    return nullptr;
  return it->second;
}

void AXActionHandlerRegistry::SetAXTreeID(const ui::AXTreeID& id,
                                          AXActionHandlerBase* action_handler) {
  DCHECK(id_to_action_handler_.find(id) == id_to_action_handler_.end());
  id_to_action_handler_[id] = action_handler;
}

void AXActionHandlerRegistry::RemoveAXTreeID(AXTreeID ax_tree_id) {
  auto frame_it = ax_tree_to_frame_id_map_.find(ax_tree_id);
  if (frame_it != ax_tree_to_frame_id_map_.end()) {
    frame_to_ax_tree_id_map_.erase(frame_it->second);
    ax_tree_to_frame_id_map_.erase(frame_it);
  }

  auto action_it = id_to_action_handler_.find(ax_tree_id);
  if (action_it != id_to_action_handler_.end())
    id_to_action_handler_.erase(action_it);

  for (AXActionHandlerObserver& observer : observers_)
    observer.TreeRemoved(ax_tree_id);
}

AXActionHandlerRegistry::AXActionHandlerRegistry() {}

AXActionHandlerRegistry::~AXActionHandlerRegistry() {}

}  // namespace ui
