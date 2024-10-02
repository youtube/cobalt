// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_test_util.h"

#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/password_manager/core/common/password_manager_features.h"

using password_manager::TestPasswordStore;

scoped_refptr<TestPasswordStore> CreateAndUseTestPasswordStore(
    content::BrowserContext* context) {
  TestPasswordStore* store = static_cast<TestPasswordStore*>(
      PasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              context,
              base::BindRepeating(&password_manager::BuildPasswordStore<
                                  content::BrowserContext, TestPasswordStore>))
          .get());
  return base::WrapRefCounted(store);
}

scoped_refptr<TestPasswordStore> CreateAndUseTestAccountPasswordStore(
    content::BrowserContext* context) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordsAccountStorage)) {
    return nullptr;
  }
  TestPasswordStore* store = static_cast<TestPasswordStore*>(
      AccountPasswordStoreFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              context,
              base::BindRepeating(&password_manager::BuildPasswordStoreWithArgs<
                                      content::BrowserContext,
                                      password_manager::TestPasswordStore,
                                      password_manager::IsAccountStore>,
                                  password_manager::IsAccountStore(true)))
          .get());

  return base::WrapRefCounted(store);
}
