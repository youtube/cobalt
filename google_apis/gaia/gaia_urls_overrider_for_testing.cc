// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_urls_overrider_for_testing.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "google_apis/gaia/gaia_config.h"
#include "google_apis/gaia/gaia_urls.h"

GaiaUrlsOverriderForTesting::GaiaUrlsOverriderForTesting(
    const std::string& url_key,
    const std::string& url_value)
    : GaiaUrlsOverriderForTesting(  // IN-TEST
          base::flat_map<std::string, std::string>{{url_key, url_value}}) {}

GaiaUrlsOverriderForTesting::GaiaUrlsOverriderForTesting(  // IN-TEST
    const base::flat_map<std::string, std::string>& overridden_urls) {
  auto urls_dict = base::DictValue();
  for (const auto& [url_key, url_value] : overridden_urls) {
    urls_dict.Set(url_key, base::DictValue().Set("url", url_value));
  }
  auto config_dict = base::DictValue().Set("urls", std::move(urls_dict));
  scoped_config_override_ = GaiaConfig::SetScopedConfigForTesting(
      std::make_unique<GaiaConfig>(std::move(config_dict)));

  // Replace `GaiaUrls` singleton with the temporary scoped instance.
  test_gaia_urls_ = std::make_unique<GaiaUrls>();
  GaiaUrls::SetInstanceForTesting(test_gaia_urls_.get());
}

GaiaUrlsOverriderForTesting::~GaiaUrlsOverriderForTesting() {
  GaiaUrls::SetInstanceForTesting(nullptr);
}
