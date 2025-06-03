// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_autofill_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "components/android_autofill/browser/autofill_provider.h"
#include "components/android_autofill/browser/form_event_logger_weblayer_android.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

using base::TimeTicks;

void AndroidDriverInitHook(AutofillClient* client,
                           ContentAutofillDriver* driver) {
  driver->set_autofill_manager(
      base::WrapUnique(new AndroidAutofillManager(driver, client)));
  driver->GetAutofillAgent()->SetUserGestureRequired(false);
  driver->GetAutofillAgent()->SetSecureContextRequired(true);
  driver->GetAutofillAgent()->SetFocusRequiresScroll(false);
  driver->GetAutofillAgent()->SetQueryPasswordSuggestion(true);
}

AndroidAutofillManager::AndroidAutofillManager(AutofillDriver* driver,
                                               AutofillClient* client)
    : AutofillManager(driver, client) {
  StartNewLoggingSession();
  autofill_manager_observation.Observe(this);
}

AndroidAutofillManager::~AndroidAutofillManager() {
  Reset();
}

base::WeakPtr<AutofillManager> AndroidAutofillManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

CreditCardAccessManager* AndroidAutofillManager::GetCreditCardAccessManager() {
  return nullptr;
}

bool AndroidAutofillManager::ShouldClearPreviewedForm() {
  return false;
}

void AndroidAutofillManager::OnFormSubmittedImpl(
    const FormData& form,
    bool known_success,
    mojom::SubmissionSource source) {
  address_logger_->OnWillSubmitForm();
  payments_logger_->OnWillSubmitForm();
  password_logger_->OnWillSubmitForm();
  if (auto* provider = GetAutofillProvider())
    provider->OnFormSubmitted(this, form, known_success, source);
}

void AndroidAutofillManager::OnTextFieldDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    const TimeTicks timestamp) {
  auto* provider = GetAutofillProvider();
  if (!provider) {
    return;
  }

  // We cannot use `field` is_autofilled state because it has already been
  // cleared by blink. Check `provider` cache.
  bool cached_is_autofilled = provider->GetCachedIsAutofilled(field);

  provider->OnTextFieldDidChange(this, form, field, bounding_box, timestamp);

  if (auto* logger = GetEventFormLogger(form, field)) {
    if (cached_is_autofilled) {
      logger->OnEditedAutofilledField();
    } else {
      logger->OnTypedIntoNonFilledField();
    }
  }
}

void AndroidAutofillManager::OnTextFieldDidScrollImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (auto* provider = GetAutofillProvider())
    provider->OnTextFieldDidScroll(this, form, field, bounding_box);
}

void AndroidAutofillManager::OnAskForValuesToFillImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutofillSuggestionTriggerSource trigger_source) {
  auto* provider = GetAutofillProvider();
  if (!provider) {
    return;
  }

  provider->OnAskForValuesToFill(this, form, field, bounding_box,
                                 trigger_source);

  if (auto* logger = GetEventFormLogger(form, field)) {
    logger->OnDidInteractWithAutofillableForm();
  }
}

void AndroidAutofillManager::OnFocusOnFormFieldImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (auto* provider = GetAutofillProvider())
    provider->OnFocusOnFormField(this, form, field, bounding_box);
}

void AndroidAutofillManager::OnSelectControlDidChangeImpl(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box) {
  if (auto* provider = GetAutofillProvider())
    provider->OnSelectControlDidChange(this, form, field, bounding_box);
}

bool AndroidAutofillManager::ShouldParseForms() {
  return true;
}

void AndroidAutofillManager::OnFocusNoLongerOnFormImpl(
    bool had_interacted_form) {
  if (auto* provider = GetAutofillProvider())
    provider->OnFocusNoLongerOnForm(this, had_interacted_form);
}

void AndroidAutofillManager::OnDidFillAutofillFormDataImpl(
    const FormData& form,
    const base::TimeTicks timestamp) {
  if (auto* provider = GetAutofillProvider())
    provider->OnDidFillAutofillFormData(this, form, timestamp);
}

void AndroidAutofillManager::OnHidePopupImpl() {
  if (auto* provider = GetAutofillProvider())
    provider->OnHidePopup(this);
}

void AndroidAutofillManager::OnFormProcessed(
    const FormData& form,
    const FormStructure& form_structure) {
  DenseSet<FormType> form_types = form_structure.GetFormTypes();
  for (FormType form_type : form_types) {
    if (auto* logger = GetEventFormLogger(form_type)) {
      logger->OnDidParseForm();
    }
  }
}

void AndroidAutofillManager::OnServerRequestError(
    FormSignature form_signature,
    AutofillDownloadManager::RequestType request_type,
    int http_error) {
  if (auto* provider = GetAutofillProvider())
    provider->OnServerQueryRequestError(this, form_signature);
}

void AndroidAutofillManager::Reset() {
  // Inform the provider before resetting state in case it needs to access it.
  if (auto* rfh =
          static_cast<ContentAutofillDriver&>(driver()).render_frame_host()) {
    if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
      if (auto* provider = AutofillProvider::FromWebContents(web_contents)) {
        // Note that this doesn't use `GetAutofillProvider()` because we might
        // need to reset even when `rfh` is pending deletion.
        provider->OnManagerResetOrDestroyed(this);
      }
    }
  }
  AutofillManager::Reset();
  forms_with_server_predictions_.clear();
  StartNewLoggingSession();
}

void AndroidAutofillManager::OnContextMenuShownInField(
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id) {
  // Not relevant for Android. Only called via context menu in Desktop.
  NOTREACHED();
}

void AndroidAutofillManager::OnFieldTypesDetermined(AutofillManager& manager,
                                                    FormGlobalId form,
                                                    FieldTypeSource source) {
  CHECK_EQ(&manager, this);
  if (source != FieldTypeSource::kAutofillServer) {
    return;
  }

  forms_with_server_predictions_.insert(form);
  if (auto* provider = GetAutofillProvider()) {
    provider->OnServerPredictionsAvailable(this, form);
  }
}

AutofillProvider* AndroidAutofillManager::GetAutofillProvider() {
  if (auto* rfh =
          static_cast<ContentAutofillDriver&>(driver()).render_frame_host()) {
    if (rfh->IsActive()) {
      if (auto* web_contents = content::WebContents::FromRenderFrameHost(rfh)) {
        return AutofillProvider::FromWebContents(web_contents);
      }
    }
  }
  return nullptr;
}

FieldTypeGroup AndroidAutofillManager::ComputeFieldTypeGroupForField(
    const FormData& form,
    const FormFieldData& field) {
  FormStructure* form_structure = nullptr;
  AutofillField* autofill_field = nullptr;
  return GetCachedFormAndField(form, field, &form_structure, &autofill_field)
             ? autofill_field->Type().group()
             : FieldTypeGroup::kNoGroup;
}

void AndroidAutofillManager::FillOrPreviewForm(
    mojom::ActionPersistence action_persistence,
    const FormData& form,
    FieldTypeGroup field_type_group,
    const url::Origin& triggered_origin) {
  DCHECK_EQ(action_persistence, mojom::ActionPersistence::kFill);
  driver().ApplyFormAction(mojom::ActionType::kFill, action_persistence, form,
                           triggered_origin, {});
  // We do not call OnAutofillProfileOrCreditCardFormFilled() because WebView
  // doesn't have AutofillProfile or CreditCard.
  if (auto* logger = GetEventFormLogger(field_type_group)) {
    logger->OnDidFillSuggestion();
  }
}

void AndroidAutofillManager::StartNewLoggingSession() {
  address_logger_ = std::make_unique<FormEventLoggerWeblayerAndroid>("Address");
  payments_logger_ =
      std::make_unique<FormEventLoggerWeblayerAndroid>("CreditCard");
  password_logger_ =
      std::make_unique<FormEventLoggerWeblayerAndroid>("Password");
}

FormEventLoggerWeblayerAndroid* AndroidAutofillManager::GetEventFormLogger(
    const FormData& form,
    const FormFieldData& field) {
  return GetEventFormLogger(ComputeFieldTypeGroupForField(form, field));
}

FormEventLoggerWeblayerAndroid* AndroidAutofillManager::GetEventFormLogger(
    FieldTypeGroup group) {
  return GetEventFormLogger(FieldTypeGroupToFormType(group));
}

FormEventLoggerWeblayerAndroid* AndroidAutofillManager::GetEventFormLogger(
    FormType form_type) {
  switch (form_type) {
    case FormType::kAddressForm:
      return address_logger_.get();
    case FormType::kCreditCardForm:
      return payments_logger_.get();
    case FormType::kPasswordForm:
      return password_logger_.get();
    case FormType::kUnknownFormType:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace autofill
