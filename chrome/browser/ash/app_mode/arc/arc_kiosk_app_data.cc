// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_data.h"

#include <utility>

#include "base/logging.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_manager.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

ArcKioskAppData::ArcKioskAppData(const std::string& app_id,
                                 const std::string& package_name,
                                 const std::string& activity,
                                 const std::string& intent,
                                 const AccountId& account_id,
                                 const std::string& name)
    : KioskAppDataBase(ArcKioskAppManager::kArcKioskDictionaryName,
                       app_id,
                       account_id),
      package_name_(package_name),
      activity_(activity),
      intent_(intent) {
  DCHECK(!package_name_.empty());
  DCHECK(activity.empty() || intent.empty());
  name_ = name;
}

ArcKioskAppData::~ArcKioskAppData() = default;

bool ArcKioskAppData::operator==(const std::string& other_app_id) const {
  return app_id() == other_app_id;
}

bool ArcKioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dict = local_state->GetDict(dictionary_name());

  if (!LoadFromDictionary(dict)) {
    return false;
  }

  DecodeIcon(base::BindOnce(&ArcKioskAppData::OnIconLoadDone,
                            weak_ptr_factory_.GetWeakPtr()));
  return true;
}

void ArcKioskAppData::SetCache(const std::string& name,
                               const gfx::ImageSkia& icon) {
  DCHECK(!name.empty());
  DCHECK(!icon.isNull());
  name_ = name;
  icon_ = icon;

  base::FilePath cache_dir;
  ArcKioskAppManager::Get()->GetKioskAppIconCacheDir(&cache_dir);

  SaveIcon(*icon_.bitmap(), cache_dir);

  PrefService* local_state = g_browser_process->local_state();
  ScopedDictPrefUpdate dict_update(local_state, dictionary_name());

  SaveToDictionary(dict_update);
}

void ArcKioskAppData::OnIconLoadDone(absl::optional<gfx::ImageSkia> icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();

  if (!icon.has_value()) {
    LOG(ERROR) << "Icon Load Failure";
    return;
  }

  icon_ = icon.value();
}

}  // namespace ash
