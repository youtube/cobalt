// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/updater/app/server/posix/app_server_posix.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/updater/mac/setup/keystone.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/util/mac_util.h"

namespace updater {

bool AppServerPosix::MigrateLegacyUpdaters(
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback) {
  // TODO(crbug.com/1250524): This must not run concurrently with Keystone.
  return MigrateKeystoneApps(GetKeystoneFolderPath(updater_scope()).value(),
                             register_callback);
}

scoped_refptr<App> MakeAppServer() {
  return base::MakeRefCounted<AppServerPosix>();
}

}  // namespace updater
