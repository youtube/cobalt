// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contact_info_helper {

autofill::AutofillProfile BuildTestAccountProfile();

autofill::PersonalDataManager* GetPersonalDataManager(Profile* profile);

// Helper class to wait until the PersonalDataManager's profiles match a given
// predicate.
class PersonalDataManagerProfileChecker
    : public StatusChangeChecker,
      public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataManagerProfileChecker(
      autofill::PersonalDataManager* pdm,
      const testing::Matcher<std::vector<autofill::AutofillProfile>>& matcher);
  ~PersonalDataManagerProfileChecker() override;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

  // PersonalDataManagerObserver overrides.
  void OnPersonalDataChanged() override;

 private:
  const base::raw_ptr<autofill::PersonalDataManager> pdm_;
  const testing::Matcher<std::vector<autofill::AutofillProfile>> matcher_;
};

}  // namespace contact_info_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_CONTACT_INFO_HELPER_H_
