// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/public/skills_service.h"

namespace skills {

bool SkillsService::Observer::Require1PSkillRefresh() {
  return false;
}

SkillsService::SkillsService() = default;

SkillsService::~SkillsService() = default;

// static
bool SkillsService::IsValidSkillImageUrl(const GURL& gurl) {
  return gurl.is_valid() && gurl.DomainIs("gstatic.com") &&
         (gurl.SchemeIs(url::kHttpsScheme) || gurl.SchemeIs(url::kHttpScheme));
}

}  // namespace skills
