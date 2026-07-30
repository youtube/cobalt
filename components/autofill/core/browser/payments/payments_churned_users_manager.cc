// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_manager.h"

#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

namespace autofill::payments {

PaymentsChurnedUsersManager::PaymentsChurnedUsersManager(
    AutofillClient* autofill_client) {
  autofill_managers_observation_.Observe(
      autofill_client, ScopedAutofillManagersObservation::InitializationPolicy::
                           kObservePreexistingManagers);
}

PaymentsChurnedUsersManager::~PaymentsChurnedUsersManager() = default;

}  // namespace autofill::payments
