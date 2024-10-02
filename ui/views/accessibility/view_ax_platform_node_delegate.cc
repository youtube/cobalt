// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate.h"

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_data.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"
#include "ui/accessibility/platform/ax_unique_id.h"
#include "ui/accessibility/single_ax_tree_manager.h"
#include "ui/base/layout.h"
#include "ui/events/event_utils.h"
#include "ui/views/accessibility/view_accessibility_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

namespace {

// Information required to fire a delayed accessibility event.
struct QueuedEvent {
  QueuedEvent(ax::mojom::Event type, int32_t node_id)
      : type(type), node_id(node_id) {}

  ax::mojom::Event type;
  int32_t node_id;
};

base::LazyInstance<std::vector<QueuedEvent>>::Leaky g_event_queue =
    LAZY_INSTANCE_INITIALIZER;

bool g_is_queueing_events = false;

bool IsAccessibilityFocusableWhenEnabled(View* view) {
  return view->GetFocusBehavior() != View::FocusBehavior::NEVER &&
         view->IsDrawn();
}

// Used to determine if a View should be ignored by accessibility clients by
// being a non-keyboard-focusable child of a keyboard-focusable ancestor. E.g.,
// LabelButtons contain Labels, but a11y should just show that there's a button.
bool IsViewUnfocusableDescendantOfFocusableAncestor(View* view) {
  if (IsAccessibilityFocusableWhenEnabled(view))
    return false;

  while (view->parent()) {
    view = view->parent();
    if (IsAccessibilityFocusableWhenEnabled(view))
      return true;
  }
  return false;
}

ui::AXPlatformNode* FromNativeWindow(gfx::NativeWindow native_window) {
  Widget* widget = Widget::GetWidgetForNativeWindow(native_window);
  if (!widget)
    return nullptr;

  View* view = widget->GetRootView();
  if (!view)
    return nullptr;

  gfx::NativeViewAccessible native_view_accessible =
      view->GetNativeViewAccessible();
  if (!native_view_accessible)
    return nullptr;

  return ui::AXPlatformNode::FromNativeViewAccessible(native_view_accessible);
}

ui::AXPlatformNode* PlatformNodeFromNodeID(int32_t id) {
  // Note: For Views, node IDs and unique IDs are the same - but that isn't
  // necessarily true for all AXPlatformNodes.
  return ui::AXPlatformNodeBase::GetFromUniqueId(id);
}

void FireEvent(QueuedEvent event) {
  ui::AXPlatformNode* node = PlatformNodeFromNodeID(event.node_id);
  if (node)
    node->NotifyAccessibilityEvent(event.type);
}

void FlushQueue() {
  DCHECK(g_is_queueing_events);
  for (QueuedEvent event : g_event_queue.Get())
    FireEvent(event);
  g_is_queueing_events = false;
  g_event_queue.Get().clear();
}

void PostFlushEventQueueTaskIfNecessary() {
  if (!g_is_queueing_events) {
    g_is_queueing_events = true;
    base::OnceCallback<void()> cb = base::BindOnce(&FlushQueue);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                                std::move(cb));
  }
}

}  // namespace

ViewAXPlatformNodeDelegate::ChildWidgetsResult::ChildWidgetsResult() = default;

ViewAXPlatformNodeDelegate::ChildWidgetsResult::ChildWidgetsResult(
    std::vector<Widget*> child_widgets,
    bool is_tab_modal_showing)
    : child_widgets(child_widgets),
      is_tab_modal_showing(is_tab_modal_showing) {}
ViewAXPlatformNodeDelegate::ChildWidgetsResult::ChildWidgetsResult(
    const ViewAXPlatformNodeDelegate::ChildWidgetsResult& other) = default;

ViewAXPlatformNodeDelegate::ChildWidgetsResult::~ChildWidgetsResult() = default;

ViewAXPlatformNodeDelegate::ChildWidgetsResult&
ViewAXPlatformNodeDelegate::ChildWidgetsResult::operator=(
    const ViewAXPlatformNodeDelegate::ChildWidgetsResult& other) = default;

ViewAXPlatformNodeDelegate::ViewAXPlatformNodeDelegate(View* view)
    : ViewAccessibility(view) {}

void ViewAXPlatformNodeDelegate::Init() {
  ax_platform_node_ = ui::AXPlatformNode::Create(this);
  DCHECK(ax_platform_node_);

  static bool first_time = true;
  if (first_time) {
    ui::AXPlatformNode::RegisterNativeWindowHandler(
        base::BindRepeating(&FromNativeWindow));
    first_time = false;
  }
}

ViewAXPlatformNodeDelegate::~ViewAXPlatformNodeDelegate() {
  if (ui::AXPlatformNode::GetPopupFocusOverride() ==
      ax_platform_node_->GetNativeViewAccessible()) {
    ui::AXPlatformNode::SetPopupFocusOverride(nullptr);
  }
  // Call ExtractAsDangling() first to clear the underlying pointer and return
  // another raw_ptr instance that is allowed to dangle.
  ax_platform_node_.ExtractAsDangling()->Destroy();
}

bool ViewAXPlatformNodeDelegate::IsAccessibilityFocusable() const {
  return GetData().HasState(ax::mojom::State::kFocusable);
}

bool ViewAXPlatformNodeDelegate::IsFocusedForTesting() const {
  if (ui::AXPlatformNode::GetPopupFocusOverride()) {
    return ui::AXPlatformNode::GetPopupFocusOverride() ==
           GetNativeViewAccessible();
  }

  return ViewAccessibility::IsFocusedForTesting();
}

void ViewAXPlatformNodeDelegate::SetPopupFocusOverride() {
  ui::AXPlatformNode::SetPopupFocusOverride(GetNativeViewAccessible());
}

void ViewAXPlatformNodeDelegate::EndPopupFocusOverride() {
  ui::AXPlatformNode::SetPopupFocusOverride(nullptr);
}

void ViewAXPlatformNodeDelegate::FireFocusAfterMenuClose() {
  ui::AXPlatformNodeBase* focused_node =
      static_cast<ui::AXPlatformNodeBase*>(ax_platform_node_);
  // Continue to drill down focused nodes to get to the "deepest" node that is
  // focused. This is not necessarily a view. It could be web content.
  while (focused_node) {
    ui::AXPlatformNodeBase* deeper_focus = static_cast<ui::AXPlatformNodeBase*>(
        ui::AXPlatformNode::FromNativeViewAccessible(focused_node->GetFocus()));
    if (!deeper_focus || deeper_focus == focused_node)
      break;
    focused_node = deeper_focus;
  }
  if (focused_node) {
    // Callback used for testing.
    if (accessibility_events_callback_) {
      accessibility_events_callback_.Run(
          this, ax::mojom::Event::kFocusAfterMenuClose);
    }

    focused_node->NotifyAccessibilityEvent(
        ax::mojom::Event::kFocusAfterMenuClose);
  }
}

bool ViewAXPlatformNodeDelegate::IsIgnored() const {
  // TODO(nektar): Make `ViewAccessibility::IsIgnored()` non-virtual and delete
  // this method. For this to happen
  // `IsViewUnfocusableDescendantOfFocusableAncestor()` needs to be moved to
  // `ViewAccessibility`.
  return GetData().IsIgnored();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetNativeObject() const {
  DCHECK(ax_platform_node_);
  return ax_platform_node_->GetNativeViewAccessible();
}

void ViewAXPlatformNodeDelegate::NotifyAccessibilityEvent(
    ax::mojom::Event event_type) {
  DCHECK(ax_platform_node_);
  if (accessibility_events_callback_)
    accessibility_events_callback_.Run(this, event_type);
  if (g_is_queueing_events) {
    g_event_queue.Get().emplace_back(event_type, GetUniqueId());
    return;
  }

  // Some events have special handling.
  switch (event_type) {
    case ax::mojom::Event::kFocusAfterMenuClose: {
      DCHECK(!ui::AXPlatformNode::GetPopupFocusOverride())
          << "Must call ViewAccessibility::EndPopupFocusOverride() as menu "
             "closes.";
      break;
    }
    case ax::mojom::Event::kFocus: {
      if (ui::AXPlatformNode::GetPopupFocusOverride()) {
        DCHECK_EQ(ui::AXPlatformNode::GetPopupFocusOverride(),
                  GetNativeViewAccessible())
            << "If the popup focus override is on, then the kFocus event must "
               "match it. Most likely the popup has closed, but did not call "
               "ViewAccessibility::EndPopupFocusOverride(), and focus has "
               "now moved on.";
      }
      break;
    }
    case ax::mojom::Event::kFocusContext: {
      // A focus context event is intended to send a focus event and a delay
      // before the next focus event. It makes sense to delay the entire next
      // synchronous batch of next events so that ordering remains the same.
      // Begin queueing subsequent events and flush queue asynchronously.
      PostFlushEventQueueTaskIfNecessary();
      break;
    }
    case ax::mojom::Event::kLiveRegionChanged: {
      // Fire after a delay so that screen readers don't wipe it out when
      // another user-generated event fires simultaneously.
      PostFlushEventQueueTaskIfNecessary();
      g_event_queue.Get().emplace_back(event_type, GetUniqueId());
      return;
    }
    default:
      break;
  }

  // Fire events here now that our internal state is up-to-date.
  ax_platform_node_->NotifyAccessibilityEvent(event_type);
}

#if BUILDFLAG(IS_MAC)
void ViewAXPlatformNodeDelegate::AnnounceText(const std::u16string& text) {
  ax_platform_node_->AnnounceText(text);
}
#endif  // BUILDFLAG(IS_MAC)

const ui::AXNodeData& ViewAXPlatformNodeDelegate::GetData() const {
  // Clear the data, then populate it.
  data_ = ui::AXNodeData();
  GetAccessibleNodeData(&data_);

  // View::IsDrawn is true if a View is visible and all of its ancestors are
  // visible too, since invisibility inherits.
  //
  // (We could try to move this logic to ViewAccessibility, but
  // that would require ensuring that Chrome OS invalidates the whole
  // subtree when a View changes its visibility state.)
  if (!view()->IsDrawn())
    data_.AddState(ax::mojom::State::kInvisible);

  // Make sure this element is excluded from the a11y tree if there's a
  // focusable parent. All keyboard focusable elements should be leaf nodes.
  // Exceptions to this rule will themselves be accessibility focusable.
  //
  // Note: this code was added to support MacViews accessibility,
  // because we needed a way to mark a View as a leaf node in the
  // accessibility tree. We need to replace this with a cross-platform
  // solution that works for ChromeVox, too, and move it to ViewAccessibility.
  if (IsViewUnfocusableDescendantOfFocusableAncestor(view()))
    data_.AddState(ax::mojom::State::kIgnored);

  return data_;
}

size_t ViewAXPlatformNodeDelegate::GetChildCount() const {
  // We call `ViewAccessibility::IsLeaf` here instead of our own override
  // because our class has an expanded definition of what a leaf node is, which
  // includes all nodes with zero unignored children. Calling our own override
  // would create a circular definition of what a "leaf node" is.
  if (ViewAccessibility::IsLeaf())
    return 0;

  // If present, virtual view children override any real children.
  if (!virtual_children().empty()) {
    // Ignored virtual views are not exposed in any accessibility platform APIs.
    // Remove all ignored virtual view children and recursively replace them
    // with their unignored children count.
    size_t virtual_child_count = 0;
    for (const std::unique_ptr<AXVirtualView>& virtual_child :
         virtual_children()) {
      if (virtual_child->IsIgnored()) {
        virtual_child_count += virtual_child->GetChildCount();
      } else {
        ++virtual_child_count;
      }
    }

    // A virtual views subtree hides any real view children.
    return virtual_child_count;
  }

  const ChildWidgetsResult child_widgets_result = GetChildWidgets();
  if (child_widgets_result.is_tab_modal_showing) {
    // In order to support the "read title (NVDAKey+T)" and "read window
    // (NVDAKey+B)" commands in the NVDA screen reader, hide the rest of the UI
    // from the accessibility tree when a modal dialog is showing.
    DCHECK_EQ(child_widgets_result.child_widgets.size(), 1U);
    return 1;
  }

  // Ignored views are not exposed in any accessibility platform APIs. Remove
  // all ignored view children and recursively replace them with their unignored
  // children count. This matches how AXPlatformNodeDelegate::GetChildCount()
  // behaves for Web content.
  size_t view_child_count = 0;
  for (View* child : view()->children()) {
    const ViewAccessibility& view_accessibility = child->GetViewAccessibility();
    if (view_accessibility.IsIgnored()) {
      const auto* child_view_delegate =
          static_cast<const ViewAXPlatformNodeDelegate*>(&view_accessibility);
      DCHECK(child_view_delegate);
      view_child_count += child_view_delegate->GetChildCount();
    } else {
      ++view_child_count;
    }
  }

  return view_child_count + child_widgets_result.child_widgets.size();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::ChildAtIndex(
    size_t index) const {
  DCHECK_LT(index, GetChildCount())
      << "|index| should be less than the unignored child count.";
  if (IsLeaf())
    return nullptr;

  if (!virtual_children().empty()) {
    // A virtual views subtree hides all the real view children.
    for (const std::unique_ptr<AXVirtualView>& virtual_child :
         virtual_children()) {
      if (virtual_child->IsIgnored()) {
        size_t virtual_child_count = virtual_child->GetChildCount();
        if (index < virtual_child_count)
          return virtual_child->ChildAtIndex(index);
        index -= virtual_child_count;
      } else {
        if (index == 0)
          return virtual_child->GetNativeObject();
        --index;
      }
    }

    NOTREACHED_NORETURN()
        << "|index| should be less than the unignored child count.";
  }

  // Our widget might have child widgets. If this is a root view, include those
  // widgets in the list of the root view's children because this is the most
  // opportune location in the accessibility tree to expose them.
  const ChildWidgetsResult child_widgets_result = GetChildWidgets();
  const std::vector<Widget*>& child_widgets =
      child_widgets_result.child_widgets;

  // If a visible tab modal dialog is present, return the dialog's root view.
  //
  // This is in order to support the "read title (NVDAKey+T)" and "read window
  // (NVDAKey+B)" commands in the NVDA screen reader. We need to hide the rest
  // of the UI, other than the dialog, from the screen reader.
  if (child_widgets_result.is_tab_modal_showing) {
    DCHECK_EQ(index, 0u);
    DCHECK_EQ(child_widgets.size(), 1U);
    return child_widgets[0]->GetRootView()->GetNativeViewAccessible();
  }

  for (View* child : view()->children()) {
    ViewAccessibility& view_accessibility = child->GetViewAccessibility();
    if (view_accessibility.IsIgnored()) {
      auto* child_view_delegate =
          static_cast<ViewAXPlatformNodeDelegate*>(&view_accessibility);
      DCHECK(child_view_delegate);
      size_t child_count = child_view_delegate->GetChildCount();
      if (index < child_count)
        return child_view_delegate->ChildAtIndex(index);
      index -= child_count;
    } else {
      if (index == 0)
        return view_accessibility.view()->GetNativeViewAccessible();
      --index;
    }
  }

  CHECK_LT(index, child_widgets_result.child_widgets.size())
      << "|index| should be less than the unignored child count.";
  return child_widgets[index]->GetRootView()->GetNativeViewAccessible();
}

bool ViewAXPlatformNodeDelegate::HasModalDialog() const {
  return GetChildWidgets().is_tab_modal_showing;
}

bool ViewAXPlatformNodeDelegate::IsChildOfLeaf() const {
  return AXPlatformNodeDelegate::IsChildOfLeaf();
}

ui::AXNodePosition::AXPositionInstance
ViewAXPlatformNodeDelegate::CreateTextPositionAt(
    int offset,
    ax::mojom::TextAffinity affinity) const {
  // Support text navigation only on text fields for now. Primarily this is to
  // support navigating the address bar.
  if (!IsDescendantOfAtomicTextField())
    return ui::AXNodePosition::CreateNullPosition();

  if (!single_tree_manager_) {
    ui::AXTreeUpdate initial_state;
    initial_state.root_id = GetData().id;
    initial_state.nodes = {GetData()};
    initial_state.has_tree_data = true;
    initial_state.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    auto tree = std::make_unique<ui::AXTree>(initial_state);
    single_tree_manager_ =
        std::make_unique<ui::SingleAXTreeManager>(std::move(tree));
  } else {
    DCHECK(single_tree_manager_->ax_tree());
    ui::AXTreeUpdate update;
    update.nodes = {GetData()};
    const_cast<ui::AXTree*>(single_tree_manager_->ax_tree())
        ->Unserialize(update);
  }

  return ui::AXNodePosition::CreatePosition(*single_tree_manager_->GetRoot(),
                                            offset, affinity);
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetNSWindow() {
  NOTIMPLEMENTED() << "Should only be called on Mac.";
  return nullptr;
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetNativeViewAccessible()
    const {
  // TODO(nektar): Make "GetNativeViewAccessible" const throughout the codebase.
  return const_cast<ViewAXPlatformNodeDelegate*>(this)
      ->GetNativeViewAccessible();
}

gfx::NativeViewAccessible
ViewAXPlatformNodeDelegate::GetNativeViewAccessible() {
  // The WebView class returns the BrowserAccessibility instance exposed by its
  // WebContents child, not its own AXPlatformNode. This is done by overriding
  // "GetNativeViewAccessible", so we can't simply call "GetNativeObject" here.
  return view()->GetNativeViewAccessible();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetParent() const {
  if (View* parent_view = view()->parent()) {
    ViewAccessibility& view_accessibility = parent_view->GetViewAccessibility();
    if (!view_accessibility.IsIgnored())
      return parent_view->GetNativeViewAccessible();

    auto* parent_view_delegate =
        static_cast<ViewAXPlatformNodeDelegate*>(&view_accessibility);
    DCHECK(parent_view_delegate);
    return parent_view_delegate->GetParent();
  }

  if (Widget* widget = view()->GetWidget()) {
    Widget* top_widget = widget->GetTopLevelWidget();
    if (top_widget && widget != top_widget && top_widget->GetRootView())
      return top_widget->GetRootView()->GetNativeViewAccessible();
  }

  return nullptr;
}

bool ViewAXPlatformNodeDelegate::IsLeaf() const {
  return ViewAccessibility::IsLeaf() || AXPlatformNodeDelegate::IsLeaf();
}

bool ViewAXPlatformNodeDelegate::IsInvisibleOrIgnored() const {
  return IsIgnored() || !view()->GetVisible();
}

bool ViewAXPlatformNodeDelegate::IsAccessibilityEnabled() const {
  return GetData().GetRestriction() != ax::mojom::Restriction::kDisabled;
}

bool ViewAXPlatformNodeDelegate::IsFocused() const {
  return GetFocus() == GetNativeObject();
}

bool ViewAXPlatformNodeDelegate::IsToplevelBrowserWindow() {
  // Note: only used on Desktop Linux. Other platforms don't have an application
  // node so this would never return true.
  ui::AXNodeData data = GetData();
  if (data.role != ax::mojom::Role::kWindow)
    return false;

  AXPlatformNodeDelegate* parent = GetParentDelegate();
  return parent && parent->GetData().role == ax::mojom::Role::kApplication;
}

gfx::Rect ViewAXPlatformNodeDelegate::GetBoundsRect(
    const ui::AXCoordinateSystem coordinate_system,
    const ui::AXClippingBehavior clipping_behavior,
    ui::AXOffscreenResult* offscreen_result) const {
  switch (coordinate_system) {
    case ui::AXCoordinateSystem::kScreenDIPs:
      // We could optionally add clipping here if ever needed.
      return view()->GetBoundsInScreen();
    case ui::AXCoordinateSystem::kScreenPhysicalPixels:
    case ui::AXCoordinateSystem::kRootFrame:
    case ui::AXCoordinateSystem::kFrame:
      NOTIMPLEMENTED();
      return gfx::Rect();
  }
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::HitTestSync(
    int screen_physical_pixel_x,
    int screen_physical_pixel_y) const {
  if (!view() || !view()->GetWidget())
    return nullptr;

  if (IsLeaf())
    return GetNativeViewAccessible();

  gfx::NativeView native_view = view()->GetWidget()->GetNativeView();
  float scale_factor = 1.0;
  if (native_view) {
    scale_factor = ui::GetScaleFactorForNativeView(native_view);
    scale_factor = scale_factor <= 0 ? 1.0 : scale_factor;
  }
  int screen_dips_x = screen_physical_pixel_x / scale_factor;
  int screen_dips_y = screen_physical_pixel_y / scale_factor;

  // Search child widgets first, since they're on top in the z-order.
  for (Widget* child_widget : GetChildWidgets().child_widgets) {
    View* child_root_view = child_widget->GetRootView();
    gfx::Point point(screen_dips_x, screen_dips_y);
    View::ConvertPointFromScreen(child_root_view, &point);
    if (child_root_view->HitTestPoint(point))
      return child_root_view->GetNativeViewAccessible();
  }

  gfx::Point point(screen_dips_x, screen_dips_y);
  View::ConvertPointFromScreen(view(), &point);
  if (!view()->HitTestPoint(point))
    return nullptr;

  // Check if the point is within any of the virtual children of this view.
  // AXVirtualView's HitTestSync is a recursive function that will return the
  // deepest child, since it does not support relative bounds.
  if (!virtual_children().empty()) {
    // Search the greater indices first, since they're on top in the z-order.
    for (const std::unique_ptr<AXVirtualView>& child :
         base::Reversed(virtual_children())) {
      gfx::NativeViewAccessible result =
          child->HitTestSync(screen_physical_pixel_x, screen_physical_pixel_y);
      if (result)
        return result;
    }
    // If it's not inside any of our virtual children, it's inside this view.
    return GetNativeViewAccessible();
  }

  // Check if the point is within any of the immediate children of this
  // view. We don't have to search further because AXPlatformNode will
  // do a recursive hit test if we return anything other than |this| or NULL.
  View* v = view();
  const auto is_point_in_child = [point, v](View* child) {
    if (!child->GetVisible())
      return false;
    ui::AXNodeData child_data;
    child->GetViewAccessibility().GetAccessibleNodeData(&child_data);
    if (child_data.IsInvisible())
      return false;
    gfx::Point point_in_child_coords = point;
    v->ConvertPointToTarget(v, child, &point_in_child_coords);
    return child->HitTestPoint(point_in_child_coords);
  };
  const auto i =
      base::ranges::find_if(base::Reversed(v->children()), is_point_in_child);
  // If it's not inside any of our children, it's inside this view.
  return (i == v->children().rend()) ? GetNativeViewAccessible()
                                     : (*i)->GetNativeViewAccessible();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegate::GetFocus() const {
  gfx::NativeViewAccessible focus_override =
      ui::AXPlatformNode::GetPopupFocusOverride();
  if (focus_override)
    return focus_override;

  FocusManager* focus_manager = view()->GetFocusManager();
  View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;

  if (!focused_view)
    return nullptr;

  // The accessibility focus will be either on the |focused_view| or on one of
  // its virtual children.
  return focused_view->GetViewAccessibility().GetFocusedDescendant();
}

ui::AXPlatformNode* ViewAXPlatformNodeDelegate::GetFromNodeID(int32_t id) {
  return PlatformNodeFromNodeID(id);
}

ui::AXPlatformNode* ViewAXPlatformNodeDelegate::GetFromTreeIDAndNodeID(
    const ui::AXTreeID& ax_tree_id,
    int32_t id) {
  return nullptr;
}

bool ViewAXPlatformNodeDelegate::AccessibilityPerformAction(
    const ui::AXActionData& data) {
  return view()->HandleAccessibleAction(data);
}

bool ViewAXPlatformNodeDelegate::ShouldIgnoreHoveredStateForTesting() {
  return false;
}

bool ViewAXPlatformNodeDelegate::IsOffscreen() const {
  // TODO(katydek): need to implement.
  return false;
}

std::u16string ViewAXPlatformNodeDelegate::GetAuthorUniqueId() const {
  const View* v = view();
  if (v) {
    const int view_id = v->GetID();
    if (view_id)
      return u"view_" + base::NumberToString16(view_id);
  }

  return std::u16string();
}

bool ViewAXPlatformNodeDelegate::IsMinimized() const {
  Widget* widget = view()->GetWidget();
  return widget && widget->IsMinimized();
}

// TODO(accessibility): This function should call AXNode::IsReadOnlySupported
// instead, just like in BrowserAccessibility, but ViewAccessibility objects
// don't have a corresponding AXNode yet.
bool ViewAXPlatformNodeDelegate::IsReadOnlySupported() const {
  return ui::IsReadOnlySupported(GetData().role);
}

// TODO(accessibility): This function should call AXNode::IsReadOnlyOrDisabled
// instead, just like in BrowserAccessibility, but ViewAccessibility objects
// don't have a corresponding AXNode yet.
bool ViewAXPlatformNodeDelegate::IsReadOnlyOrDisabled() const {
  switch (GetData().GetRestriction()) {
    case ax::mojom::Restriction::kReadOnly:
    case ax::mojom::Restriction::kDisabled:
      return true;
    case ax::mojom::Restriction::kNone: {
      if (HasState(ax::mojom::State::kEditable) ||
          HasState(ax::mojom::State::kRichlyEditable)) {
        return false;
      }

      if (ui::ShouldHaveReadonlyStateByDefault(GetData().role))
        return true;

      // When readonly is not supported, we assume that the node is always
      // read-only and mark it as such since this is the default behavior.
      return !IsReadOnlySupported();
    }
  }
}

const ui::AXUniqueId& ViewAXPlatformNodeDelegate::GetUniqueId() const {
  return ViewAccessibility::GetUniqueId();
}

std::vector<int32_t> ViewAXPlatformNodeDelegate::GetColHeaderNodeIds() const {
  std::vector<int32_t> col_header_ids;
  if (!virtual_children().empty()) {
    for (const std::unique_ptr<AXVirtualView>& header_cell :
         virtual_children().front()->children()) {
      const ui::AXNodeData& header_data = header_cell->GetData();
      if (header_data.role == ax::mojom::Role::kColumnHeader) {
        col_header_ids.push_back(header_data.id);
      }
    }
  }
  return col_header_ids;
}

std::vector<int32_t> ViewAXPlatformNodeDelegate::GetColHeaderNodeIds(
    int col_index) const {
  std::vector<int32_t> columns = GetColHeaderNodeIds();
  if (static_cast<size_t>(col_index) >= columns.size()) {
    return {};
  }
  return {columns[static_cast<size_t>(col_index)]};
}

absl::optional<int32_t> ViewAXPlatformNodeDelegate::GetCellId(
    int row_index,
    int col_index) const {
  if (virtual_children().empty() || !GetAncestorTableView())
    return absl::nullopt;

  AXVirtualView* ax_cell = GetAncestorTableView()->GetVirtualAccessibilityCell(
      static_cast<size_t>(row_index), static_cast<size_t>(col_index));
  if (!ax_cell)
    return absl::nullopt;

  const ui::AXNodeData& cell_data = ax_cell->GetData();
  if (cell_data.role == ax::mojom::Role::kCell)
    return cell_data.id;

  return absl::nullopt;
}

TableView* ViewAXPlatformNodeDelegate::GetAncestorTableView() const {
  ui::AXNodeData data;
  view()->GetViewAccessibility().GetAccessibleNodeData(&data);

  if (!ui::IsTableLike(data.role))
    return nullptr;

  return static_cast<TableView*>(view());
}

bool ViewAXPlatformNodeDelegate::IsOrderedSetItem() const {
  const ui::AXNodeData& data = GetData();
  return (view()->GetGroup() >= 0) ||
         (data.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet) &&
          data.HasIntAttribute(ax::mojom::IntAttribute::kSetSize));
}

bool ViewAXPlatformNodeDelegate::IsOrderedSet() const {
  return (view()->GetGroup() >= 0) ||
         GetData().HasIntAttribute(ax::mojom::IntAttribute::kSetSize);
}

absl::optional<int> ViewAXPlatformNodeDelegate::GetPosInSet() const {
  // Consider overridable attributes first.
  const ui::AXNodeData& data = GetData();
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kPosInSet))
    return data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet);

  std::vector<View*> views_in_group;
  GetViewsInGroupForSet(&views_in_group);
  if (views_in_group.empty())
    return absl::nullopt;
  // Check this is in views_in_group; it may be removed if it is ignored.
  auto found_view = base::ranges::find(views_in_group, view());
  if (found_view == views_in_group.end())
    return absl::nullopt;

  int pos_in_set = base::checked_cast<int>(
      std::distance(views_in_group.begin(), found_view));
  // pos_in_set is zero-based; users expect one-based, so increment.
  return ++pos_in_set;
}

absl::optional<int> ViewAXPlatformNodeDelegate::GetSetSize() const {
  // Consider overridable attributes first.
  const ui::AXNodeData& data = GetData();
  if (data.HasIntAttribute(ax::mojom::IntAttribute::kSetSize))
    return data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize);

  std::vector<View*> views_in_group;
  GetViewsInGroupForSet(&views_in_group);
  if (views_in_group.empty())
    return absl::nullopt;
  // Check this is in views_in_group; it may be removed if it is ignored.
  auto found_view = base::ranges::find(views_in_group, view());
  if (found_view == views_in_group.end())
    return absl::nullopt;

  return base::checked_cast<int>(views_in_group.size());
}

void ViewAXPlatformNodeDelegate::GetViewsInGroupForSet(
    std::vector<View*>* views_in_group) const {
  const int group_id = view()->GetGroup();
  if (group_id < 0)
    return;

  View* view_to_check = view();
  // If this view has a parent, check from the parent, to make sure we catch any
  // siblings.
  if (view()->parent())
    view_to_check = view()->parent();
  view_to_check->GetViewsInGroup(group_id, views_in_group);

  // Remove any views that are ignored in the accessibility tree.
  views_in_group->erase(
      std::remove_if(
          views_in_group->begin(), views_in_group->end(),
          [](View* view) {
            ViewAccessibility& view_accessibility =
                view->GetViewAccessibility();
            return view_accessibility.IsIgnored();
          }),
      views_in_group->end());
}

bool ViewAXPlatformNodeDelegate::TableHasColumnOrRowHeaderNodeForTesting()
    const {
  if (!GetAncestorTableView()) {
    return false;
  }
  return !GetAncestorTableView()->visible_columns().empty();
}

ViewAXPlatformNodeDelegate::ChildWidgetsResult
ViewAXPlatformNodeDelegate::GetChildWidgets() const {
  // This method is used to create a parent / child relationship between the
  // root view and any child widgets. Child widgets should only be exposed as
  // the direct children of the root view. A root view should appear as the only
  // child of a widget.
  Widget* widget = view()->GetWidget();
  // Note that during window close, a Widget may exist in a state where it has
  // no NativeView, but hasn't yet torn down its view hierarchy.
  if (!widget || !widget->GetNativeView() || widget->GetRootView() != view())
    return ChildWidgetsResult();

  std::set<Widget*> owned_widgets;
  Widget::GetAllOwnedWidgets(widget->GetNativeView(), &owned_widgets);

  std::vector<Widget*> visible_widgets;
  base::ranges::copy_if(owned_widgets, std::back_inserter(visible_widgets),
                        &Widget::IsVisible);

  // Focused child widgets should take the place of the web page they cover in
  // the accessibility tree.
  const FocusManager* focus_manager = view()->GetFocusManager();
  const View* focused_view =
      focus_manager ? focus_manager->GetFocusedView() : nullptr;
  const auto is_focused_child = [focused_view](Widget* child_widget) {
    return ViewAccessibilityUtils::IsFocusedChildWidget(child_widget,
                                                        focused_view);
  };
  const auto i = base::ranges::find_if(visible_widgets, is_focused_child);
  // In order to support the "read title (NVDAKey+T)" and "read window
  // (NVDAKey+B)" commands in the NVDA screen reader, hide the rest of the UI
  // from the accessibility tree when a modal dialog is showing.
  if (i != visible_widgets.cend())
    return ChildWidgetsResult({*i}, true /* is_tab_modal_showing */);

  return ChildWidgetsResult(visible_widgets, false /* is_tab_modal_showing */);
}

}  // namespace views
