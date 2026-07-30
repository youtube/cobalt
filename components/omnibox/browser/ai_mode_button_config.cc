// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/ai_mode_button_config.h"

#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "url/gurl.h"

namespace ai_mode_button_config {

bool AiModeButtonConfig::IsValid() const {
  if (id == SearchEngineType::SEARCH_ENGINE_GOOGLE) {
    return true;
  }

  if (text.empty()) {
    return false;
  }

  if (!GURL(navigation_url).is_valid() ||
      !GURL(navigation_url_empty).is_valid() || !GURL(favicon_url).is_valid()) {
    return false;
  }

  TemplateURLData turl_data;
  turl_data.SetURL(navigation_url);
  TemplateURL turl(turl_data);
  SearchTermsData search_terms_data;
  if (!turl.url_ref().IsValid(search_terms_data) ||
      !turl.url_ref().SupportsReplacement(search_terms_data)) {
    return false;
  }

  return true;
}

}  // namespace ai_mode_button_config
