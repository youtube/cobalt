// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/skills/skills_page_handler_v2.h"

#include "base/check_deref.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/storage_partition_config.h"
#include "content/public/browser/web_contents.h"

namespace skills {

SkillsPageHandlerV2::SkillsPageHandlerV2(
    mojo::PendingReceiver<::skills::mojom::SkillsPageHandler> receiver,
    Profile* profile,
    signin::IdentityManager* identity_manager,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(CHECK_DEREF(profile)),
      web_contents_(CHECK_DEREF(web_contents)),
      cookie_synchronizer_(std::make_unique<glic::GlicCookieSynchronizer>(
          &profile_.get(),
          identity_manager,
          content::StoragePartitionConfig::Create(
              &profile_.get(),
              chrome::kChromeUISkillsHost,
              /*partition_name=*/"glicskillspart",
              /*in_memory=*/true))) {}

SkillsPageHandlerV2::~SkillsPageHandlerV2() = default;

void SkillsPageHandlerV2::SyncCookies(SyncCookiesCallback callback) {
  cookie_synchronizer_->CopyCookiesToWebviewStoragePartition(
      std::move(callback));
}

}  // namespace skills
