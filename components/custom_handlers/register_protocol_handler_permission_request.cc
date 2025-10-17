// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/custom_handlers/register_protocol_handler_permission_request.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/permissions/permission_request_data.h"
#include "components/permissions/request_type.h"
#include "components/permissions/resolvers/content_setting_permission_resolver.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace custom_handlers {

RegisterProtocolHandlerPermissionRequest::
    RegisterProtocolHandlerPermissionRequest(
        custom_handlers::ProtocolHandlerRegistry* registry,
        const ProtocolHandler& handler,
        GURL url,
        base::ScopedClosureRunner fullscreen_block)
    : PermissionRequest(
          std::make_unique<permissions::PermissionRequestData>(
              std::make_unique<permissions::ContentSettingPermissionResolver>(
                  permissions::RequestType::kRegisterProtocolHandler),
              /*user_gesture=*/false,
              url.DeprecatedGetOriginAsURL()),
          base::BindRepeating(
              &RegisterProtocolHandlerPermissionRequest::PermissionDecided,
              base::Unretained(this))),
      registry_(registry),
      handler_(handler),
      fullscreen_block_(std::move(fullscreen_block)) {}

RegisterProtocolHandlerPermissionRequest::
    ~RegisterProtocolHandlerPermissionRequest() = default;

bool RegisterProtocolHandlerPermissionRequest::IsDuplicateOf(
    permissions::PermissionRequest* other_request) const {
  // The downcast here is safe because PermissionRequest::IsDuplicateOf ensures
  // that both requests are of type kRegisterProtocolHandler.
  return permissions::PermissionRequest::IsDuplicateOf(other_request) &&
         handler_.protocol() ==
             static_cast<RegisterProtocolHandlerPermissionRequest*>(
                 other_request)
                 ->handler_.protocol();
}

std::u16string
RegisterProtocolHandlerPermissionRequest::GetMessageTextFragment() const {
  ProtocolHandler old_handler = registry_->GetHandlerFor(handler_.protocol());
  return old_handler.IsEmpty()
             ? l10n_util::GetStringFUTF16(
                   IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_FRAGMENT,
                   handler_.GetProtocolDisplayName())
             : l10n_util::GetStringFUTF16(
                   IDS_REGISTER_PROTOCOL_HANDLER_CONFIRM_REPLACE_FRAGMENT,
                   handler_.GetProtocolDisplayName(),
                   base::UTF8ToUTF16(old_handler.url().host_piece()));
}

void RegisterProtocolHandlerPermissionRequest::PermissionDecided(
    ContentSetting result,
    bool is_one_time,
    bool is_final_decision,
    const permissions::PermissionRequestData& request_data) {
  DCHECK(!is_one_time);
  DCHECK(is_final_decision);
  if (result == ContentSetting::CONTENT_SETTING_ALLOW) {
    base::RecordAction(
        base::UserMetricsAction("RegisterProtocolHandler.Infobar_Accept"));
    registry_->OnAcceptRegisterProtocolHandler(handler_);
  } else {
    DCHECK(result == CONTENT_SETTING_BLOCK ||
           result == CONTENT_SETTING_DEFAULT);
    base::RecordAction(
        base::UserMetricsAction("RegisterProtocolHandler.InfoBar_Deny"));
    registry_->OnIgnoreRegisterProtocolHandler(handler_);
  }
}

}  // namespace custom_handlers
