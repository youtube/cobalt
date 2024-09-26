// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/accessibility_event_rewriter_delegate_impl.h"

#include "ash/constants/app_types.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/event_handler_common.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/common/extensions/api/accessibility_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"

namespace ash {
namespace {

std::string ToString(SwitchAccessCommand command) {
  switch (command) {
    case SwitchAccessCommand::kSelect:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_SELECT);
    case SwitchAccessCommand::kNext:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::SWITCH_ACCESS_COMMAND_NEXT);
    case SwitchAccessCommand::kPrevious:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::
              SWITCH_ACCESS_COMMAND_PREVIOUS);
    case SwitchAccessCommand::kNone:
      NOTREACHED();
      return "";
  }
}

std::string ToString(MagnifierCommand command) {
  switch (command) {
    case MagnifierCommand::kMoveStop:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVESTOP);
    case MagnifierCommand::kMoveUp:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVEUP);
    case MagnifierCommand::kMoveDown:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVEDOWN);
    case MagnifierCommand::kMoveLeft:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVELEFT);
    case MagnifierCommand::kMoveRight:
      return extensions::api::accessibility_private::ToString(
          extensions::api::accessibility_private::MAGNIFIER_COMMAND_MOVERIGHT);
  }

  return "";
}

}  // namespace

AccessibilityEventRewriterDelegateImpl::
    AccessibilityEventRewriterDelegateImpl() = default;

AccessibilityEventRewriterDelegateImpl::
    ~AccessibilityEventRewriterDelegateImpl() = default;

void AccessibilityEventRewriterDelegateImpl::DispatchKeyEventToChromeVox(
    std::unique_ptr<ui::Event> event,
    bool capture) {
  extensions::ExtensionHost* host =
      GetAccessibilityExtensionHost(extension_misc::kChromeVoxExtensionId);
  if (!host)
    return;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  host->host_contents()->SetDelegate(capture ? this : nullptr);

  // Forward the event to ChromeVox's background page.
  ForwardKeyToExtension(*(event->AsKeyEvent()), host);
}

void AccessibilityEventRewriterDelegateImpl::DispatchMouseEvent(
    std::unique_ptr<ui::Event> event) {
  ax::mojom::Event event_type;

  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
      event_type = ax::mojom::Event::kMouseMoved;
      break;
    case ui::ET_MOUSE_DRAGGED:
      event_type = ax::mojom::Event::kMouseDragged;
      break;
    default:
      NOTREACHED();
      return;
  }

  AutomationManagerAura::GetInstance()->HandleEvent(event_type);
}

void AccessibilityEventRewriterDelegateImpl::SendSwitchAccessCommand(
    SwitchAccessCommand command) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::List event_args;
  event_args.Append(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnSwitchAccessCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::SendPointScanPoint(
    const gfx::PointF& point) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::Dict point_dict;
  point_dict.Set("x", point.x());
  point_dict.Set("y", point.y());

  base::Value::List event_args;
  event_args.Append(std::move(point_dict));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_POINT_SCAN_SET,
      extensions::api::accessibility_private::OnPointScanSet::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kSwitchAccessExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::SendMagnifierCommand(
    MagnifierCommand command) {
  extensions::EventRouter* event_router =
      extensions::EventRouter::Get(AccessibilityManager::Get()->profile());

  base::Value::List event_args;
  event_args.Append(ToString(command));

  auto event = std::make_unique<extensions::Event>(
      extensions::events::ACCESSIBILITY_PRIVATE_ON_SWITCH_ACCESS_COMMAND,
      extensions::api::accessibility_private::OnMagnifierCommand::kEventName,
      std::move(event_args));

  event_router->DispatchEventWithLazyListener(
      extension_misc::kAccessibilityCommonExtensionId, std::move(event));
}

void AccessibilityEventRewriterDelegateImpl::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  EventRewriterController::Get()->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

bool AccessibilityEventRewriterDelegateImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  OnUnhandledSpokenFeedbackEvent(event.os_event->Clone());
  return true;
}

}  // namespace ash
