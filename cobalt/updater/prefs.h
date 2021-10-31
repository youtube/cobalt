// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_PREFS_H_
#define COBALT_UPDATER_PREFS_H_

#include <memory>

class PrefService;

namespace cobalt {
namespace updater {

std::unique_ptr<PrefService> CreatePrefService();

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_PREFS_H_
