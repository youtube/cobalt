// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/contexts/speaker_selection_permission_context.h"

#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

SpeakerSelectionPermissionContext::SpeakerSelectionPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::SPEAKER_SELECTION,
          blink::mojom::PermissionsPolicyFeature::kSpeakerSelection) {}
