// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/testing_pref_service.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "components/prefs/default_pref_store.h"
#include "components/prefs/pref_notifier_impl.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_value_store.h"
#include "testing/gtest/include/gtest/gtest.h"

template <>
TestingPrefServiceBase<PrefService, PrefRegistry>::TestingPrefServiceBase(
    TestingPrefStore* managed_prefs,
    TestingPrefStore* supervised_user_prefs,
    TestingPrefStore* extension_prefs,
    TestingPrefStore* standalone_browser_prefs,
    TestingPrefStore* user_prefs,
    TestingPrefStore* recommended_prefs,
    PrefRegistry* pref_registry,
    PrefNotifierImpl* pref_notifier)
    : PrefService(
          std::unique_ptr<PrefNotifierImpl>(pref_notifier),
          std::make_unique<PrefValueStore>(managed_prefs,
                                           supervised_user_prefs,
                                           extension_prefs,
                                           standalone_browser_prefs,
                                           /*command_line_prefs=*/nullptr,
                                           user_prefs,
                                           recommended_prefs,
                                           pref_registry->defaults().get(),
                                           pref_notifier),
          user_prefs,
          standalone_browser_prefs,
          pref_registry,
          base::BindRepeating(
              &TestingPrefServiceBase<PrefService,
                                      PrefRegistry>::HandleReadError),
          false),
      managed_prefs_(managed_prefs),
      supervised_user_prefs_(supervised_user_prefs),
      extension_prefs_(extension_prefs),
      standalone_browser_prefs_(standalone_browser_prefs),
      user_prefs_(user_prefs),
      recommended_prefs_(recommended_prefs) {}

TestingPrefServiceSimple::TestingPrefServiceSimple()
    : TestingPrefServiceBase<PrefService, PrefRegistry>(
          /*managed_prefs=*/new TestingPrefStore(),
          /*supervised_user_prefs=*/new TestingPrefStore(),
          /*extension_prefs=*/new TestingPrefStore(),
          /*standalone_browser_prefs=*/new TestingPrefStore(),
          /*user_prefs=*/new TestingPrefStore(),
          /*recommended_prefs=*/new TestingPrefStore(),
          new PrefRegistrySimple(),
          new PrefNotifierImpl()) {}

TestingPrefServiceSimple::~TestingPrefServiceSimple() {
}

PrefRegistrySimple* TestingPrefServiceSimple::registry() {
  return static_cast<PrefRegistrySimple*>(DeprecatedGetPrefRegistry());
}
