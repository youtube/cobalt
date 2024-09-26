// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/add_to_homescreen_params.h"

#include "components/webapps/browser/android/shortcut_info.h"

namespace webapps {

AddToHomescreenParams::AddToHomescreenParams() = default;
AddToHomescreenParams::~AddToHomescreenParams() = default;

std::string AddToHomescreenParams::GetAppIdentifier() {
  return native_app_data.is_null() ? shortcut_info->url.spec()
                                   : native_app_package_name;
}

}  // namespace webapps
