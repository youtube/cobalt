// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/app_restore_utils.h"

#include "ash/constants/app_types.h"
#include "base/functional/bind.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/desk_template_read_handler.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget_delegate.h"

namespace app_restore {
namespace {

const char kCrxAppPrefix[] = "_crx_";

static int32_t session_id_counter = kArcSessionIdOffsetForRestoredLaunching;

}  // namespace

bool IsArcWindow(aura::Window* window) {
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::ARC_APP);
}

bool IsLacrosWindow(aura::Window* window) {
  return window->GetProperty(aura::client::kAppType) ==
         static_cast<int>(ash::AppType::LACROS);
}

bool HasWindowInfo(int32_t restore_window_id) {
  // DeskTemplateReadHandler::GetWindowInfo returns nullptr if
  // `restore_window_id` is unknown.
  if (DeskTemplateReadHandler::Get()->GetWindowInfo(restore_window_id))
    return true;
  return full_restore::FullRestoreReadHandler::GetInstance()->HasWindowInfo(
      restore_window_id);
}

void ApplyProperties(app_restore::WindowInfo* window_info,
                     ui::PropertyHandler* property_handler) {
  DCHECK(window_info);
  DCHECK(property_handler);

  // Create a clone so `property_handler` can have complete ownership of a copy
  // of WindowInfo.
  app_restore::WindowInfo* window_info_clone = window_info->Clone();
  property_handler->SetProperty(app_restore::kWindowInfoKey, window_info_clone);

  if (window_info->activation_index) {
    const int32_t index = *window_info->activation_index;
    // kActivationIndexKey is owned, which allows for passing in this raw
    // pointer.
    property_handler->SetProperty(app_restore::kActivationIndexKey,
                                  std::make_unique<int32_t>(index));
    // Windows opened from full restore should not be activated. Widgets that
    // are shown are activated by default. Force the widget to not be
    // activatable; the activation will be restored in ash once the window is
    // launched.
    property_handler->SetProperty(app_restore::kLaunchedFromAppRestoreKey,
                                  true);
  }
  if (window_info->pre_minimized_show_state_type) {
    property_handler->SetProperty(aura::client::kRestoreShowStateKey,
                                  *window_info->pre_minimized_show_state_type);
  }
}

void ModifyWidgetParams(int32_t restore_window_id,
                        views::Widget::InitParams* out_params) {
  DCHECK(out_params);

  const bool is_arc_app =
      out_params->init_properties_container.GetProperty(
          aura::client::kAppType) == static_cast<int>(ash::AppType::ARC_APP);
  std::unique_ptr<app_restore::WindowInfo> window_info;
  auto* full_restore_read_handler =
      full_restore::FullRestoreReadHandler::GetInstance();
  auto* desk_template_read_handler = DeskTemplateReadHandler::Get();
  if (is_arc_app) {
    // This will return nullptr if `restore_window_id` doesn't belong to a desk
    // template launch. In that case, we fall back on full restore.
    ArcReadHandler* arc_read_handler =
        desk_template_read_handler->GetArcReadHandlerForWindow(
            restore_window_id);
    if (!arc_read_handler)
      arc_read_handler = full_restore_read_handler->arc_read_handler();

    window_info = arc_read_handler
                      ? arc_read_handler->GetWindowInfo(restore_window_id)
                      : nullptr;
  } else {
    window_info = desk_template_read_handler->GetWindowInfo(restore_window_id);
    if (!window_info) {
      window_info = full_restore_read_handler->GetWindowInfoForActiveProfile(
          restore_window_id);
    }
  }
  if (!window_info)
    return;

  ApplyProperties(window_info.get(), &out_params->init_properties_container);

  if (window_info->desk_id)
    out_params->workspace = base::NumberToString(*window_info->desk_id);
  if (window_info->current_bounds)
    out_params->bounds = *window_info->current_bounds;
  if (window_info->window_state_type) {
    // ToWindowShowState will make us lose some ash-specific types (left/right
    // snap). Ash is responsible for restoring these states by checking
    // GetWindowInfo.
    out_params->show_state =
        chromeos::ToWindowShowState(*window_info->window_state_type);
  }

  // Register to track when the widget has initialized. If a delegate is not
  // set, then the widget creator is responsible for calling
  // OnWidgetInitialized.
  views::WidgetDelegate* delegate = out_params->delegate;
  if (delegate) {
    delegate->RegisterWidgetInitializedCallback(base::BindOnce(
        [](views::WidgetDelegate* delegate) {
          app_restore::AppRestoreInfo::GetInstance()->OnWidgetInitialized(
              delegate->GetWidget());
        },
        delegate));
  }
}

int32_t FetchRestoreWindowId(const std::string& app_id) {
  // If full restore is running and full restore knows the app_id, then we use
  // it. Otherwise fall back on desk templates.
  auto* full_restore_read_handler =
      full_restore::FullRestoreReadHandler::GetInstance();

  int32_t restore_window_id = 0;
  if (full_restore_read_handler->IsFullRestoreRunning())
    restore_window_id = full_restore_read_handler->FetchRestoreWindowId(app_id);

  if (!restore_window_id) {
    restore_window_id =
        DeskTemplateReadHandler::Get()->FetchRestoreWindowId(app_id);
  }

  return restore_window_id;
}

int32_t CreateArcSessionId() {
  // ARC session id offset start counting from a large number. When the counter
  // overflow, it will less the start number.
  if (session_id_counter < kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is overflow: " << session_id_counter;
    session_id_counter = kArcSessionIdOffsetForRestoredLaunching;
  }
  return ++session_id_counter;
}

void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id) {
  auto* desk_template_read_handler = DeskTemplateReadHandler::Get();
  if (desk_template_read_handler->IsKnownArcSessionId(arc_session_id)) {
    desk_template_read_handler->SetArcSessionIdForWindowId(arc_session_id,
                                                           window_id);
  } else {
    full_restore::FullRestoreReadHandler::GetInstance()
        ->SetArcSessionIdForWindowId(arc_session_id, window_id);
  }
}

void SetDeskTemplateLaunchIdForArcSessionId(int32_t arc_session_id,
                                            int32_t desk_template_launch_id) {
  DeskTemplateReadHandler::Get()->SetLaunchIdForArcSessionId(
      arc_session_id, desk_template_launch_id);
}

int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id) {
  if (int32_t restore_window_id =
          DeskTemplateReadHandler::Get()->GetArcRestoreWindowIdForTaskId(
              task_id)) {
    return restore_window_id;
  }
  return full_restore::FullRestoreReadHandler::GetInstance()
      ->GetArcRestoreWindowIdForTaskId(task_id);
}

int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id) {
  if (int32_t restore_window_id =
          DeskTemplateReadHandler::Get()->GetArcRestoreWindowIdForSessionId(
              session_id)) {
    return restore_window_id;
  }
  return full_restore::FullRestoreReadHandler::GetInstance()
      ->GetArcRestoreWindowIdForSessionId(session_id);
}

std::string GetAppIdFromAppName(const std::string& app_name) {
  std::string prefix(kCrxAppPrefix);
  if (app_name.substr(0, prefix.length()) != prefix)
    return std::string();
  return app_name.substr(prefix.length());
}

const std::string GetLacrosWindowId(aura::Window* window) {
  const std::string* lacros_window_id =
      window->GetProperty(app_restore::kLacrosWindowId);
  DCHECK(lacros_window_id);
  return *lacros_window_id;
}

int32_t GetLacrosRestoreWindowId(const std::string& lacros_window_id) {
  return full_restore::FullRestoreReadHandler::GetInstance()
      ->GetLacrosRestoreWindowId(lacros_window_id);
}

}  // namespace app_restore
