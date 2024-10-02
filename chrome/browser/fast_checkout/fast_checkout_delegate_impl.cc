// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_delegate_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

FastCheckoutDelegateImpl::FastCheckoutDelegateImpl(
    content::WebContents* web_contents,
    FastCheckoutClient* client,
    autofill::BrowserAutofillManager* manager)
    : web_contents_(web_contents), client_(client), manager_(manager) {
  DCHECK(client_);
  DCHECK(manager_);
}

FastCheckoutDelegateImpl::~FastCheckoutDelegateImpl() = default;

bool FastCheckoutDelegateImpl::TryToShowFastCheckout(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    base::WeakPtr<autofill::AutofillManager> autofill_manager) {
  const GURL& url = web_contents_->GetLastCommittedURL();
  return client_->TryToStart(url, form, field, autofill_manager);
}

bool FastCheckoutDelegateImpl::IntendsToShowFastCheckout(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::FieldGlobalId field_id) const {
  if (const autofill::FormStructure* form =
          manager_->FindCachedFormById(form_id)) {
    if (const autofill::AutofillField* field = form->GetFieldById(field_id)) {
      return client_->IsSupported(form->ToFormData(), *field, manager);
    }
  }
  return false;
}

bool FastCheckoutDelegateImpl::IsShowingFastCheckoutUI() const {
  return client_->IsShowing();
}

void FastCheckoutDelegateImpl::HideFastCheckout(bool allow_further_runs) {
  if (IsShowingFastCheckoutUI()) {
    client_->Stop(/*allow_further_runs=*/allow_further_runs);
  }
}
