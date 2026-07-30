// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/google_lens_handler.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "components/lens/lens_overlay_metrics.h"

// TODO(crbug.com/506864805): Add recording "No thanks" button click.
GoogleLensHandler::GoogleLensHandler(
    mojo::PendingReceiver<feature_showcase::mojom::GoogleLensPageHandler>
        receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {}

GoogleLensHandler::~GoogleLensHandler() = default;

void GoogleLensHandler::EnableGoogleLens() {
  lens::GrantLensOverlayNeededPermissions(profile_);
}
