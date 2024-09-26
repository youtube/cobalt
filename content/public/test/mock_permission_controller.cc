// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/mock_permission_controller.h"

namespace content {

MockPermissionController::MockPermissionController() = default;

MockPermissionController::~MockPermissionController() = default;

void MockPermissionController::RequestPermissionFromCurrentDocument(
    blink::PermissionType permission,
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(blink::mojom::PermissionStatus)> callback) {}

void MockPermissionController::RequestPermissionsFromCurrentDocument(
    const std::vector<blink::PermissionType>& permission,
    RenderFrameHost* render_frame_host,
    bool user_gesture,
    base::OnceCallback<void(const std::vector<blink::mojom::PermissionStatus>&)>
        callback) {}

void MockPermissionController::ResetPermission(blink::PermissionType permission,
                                               const url::Origin& origin) {}

void MockPermissionController::UnsubscribePermissionStatusChange(
    SubscriptionId subscription_id) {}
}  // namespace content
