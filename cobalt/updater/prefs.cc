// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/prefs.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "cobalt/extension/installation_manager.h"
#include "cobalt/updater/utils.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/update_client/update_client.h"

namespace cobalt {
namespace updater {

std::unique_ptr<PrefService> CreatePrefService() {
  base::FilePath product_data_dir;
  if (!CreateProductDirectory(&product_data_dir)) return nullptr;

  const CobaltExtensionInstallationManagerApi* installation_api =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_api) {
    SB_LOG(ERROR) << "Failed to get installation manager api.";
    return nullptr;
  }
  char app_key[IM_EXT_MAX_APP_KEY_LENGTH];
  if (installation_api->GetAppKey(app_key, IM_EXT_MAX_APP_KEY_LENGTH) ==
      IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get app key.";
    return nullptr;
  }

  SB_LOG(INFO) << "Updater: prefs app_key=" << app_key;
  PrefServiceFactory pref_service_factory;
  std::string file_name = "prefs_";
  file_name += app_key;
  file_name += ".json";
  pref_service_factory.set_user_prefs(base::MakeRefCounted<JsonPrefStore>(
      product_data_dir.Append(FILE_PATH_LITERAL(file_name))));

  auto pref_registry = base::MakeRefCounted<PrefRegistrySimple>();
  update_client::RegisterPrefs(pref_registry.get());

  return pref_service_factory.Create(pref_registry);
}

}  // namespace updater
}  // namespace cobalt
