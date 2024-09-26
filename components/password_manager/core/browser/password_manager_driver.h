// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_

#include <map>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/accessibility/ax_tree_id.h"

class GURL;

namespace autofill {
struct FormData;
struct ParsingResult;
struct PasswordFormGenerationData;
struct PasswordFormFillData;
}  // namespace autofill

namespace password_manager {

class PasswordAutofillManager;
class PasswordGenerationFrameHelper;
class PasswordManagerInterface;

// Interface that allows PasswordManager core code to interact with its driver
// (i.e., obtain information from it and give information to it).
class PasswordManagerDriver
    : public base::SupportsWeakPtr<PasswordManagerDriver> {
 public:
#if BUILDFLAG(IS_ANDROID)
  using ShowVirtualKeyboard =
      base::StrongAlias<class ShowVirtualKeyboardTag, bool>;
#endif

  PasswordManagerDriver() = default;

  PasswordManagerDriver(const PasswordManagerDriver&) = delete;
  PasswordManagerDriver& operator=(const PasswordManagerDriver&) = delete;

  virtual ~PasswordManagerDriver() = default;

  // Returns driver id which is unique in the current tab.
  virtual int GetId() const = 0;

  // Fills forms matching |form_data|.
  virtual void SetPasswordFillData(
      const autofill::PasswordFormFillData& form_data) = 0;

  // Informs the driver that there are no saved credentials in the password
  // store for the current page.
  // |should_show_popup_without_passwords| instructs the driver that the popup
  // should be shown even without password suggestions. This is set to true if
  // the popup will include another item that the driver doesn't know about
  // (e.g. a promo to unlock passwords from the user's Google Account).
  // TODO(https://crbug.com/621355): Remove and observe FormFetcher instead.
  virtual void InformNoSavedCredentials(
      bool should_show_popup_without_passwords) {}

  // Notifies the driver that a password can be generated on the fields
  // identified by |form|.
  virtual void FormEligibleForGenerationFound(
      const autofill::PasswordFormGenerationData& form) {}

  // Notifies the driver that the user has accepted a generated password.
  // TODO(crbug/936011): delete this method. The UI should call the one below.
  virtual void GeneratedPasswordAccepted(const std::u16string& password) = 0;

  // Notifies the password manager that the user has accepted a generated
  // password. The password manager can bring up some disambiguation UI in
  // response.
  virtual void GeneratedPasswordAccepted(
      const autofill::FormData& form_data,
      autofill::FieldRendererId generation_element_id,
      const std::u16string& password) {}

  // Tells the driver to fill the form with the |username| and |password|.
  virtual void FillSuggestion(const std::u16string& username,
                              const std::u16string& password) = 0;

  // Tells the renderer to fill the given credential into the focused element.
  // Always calls |completed_callback| with a status indicating success/error.
  virtual void FillIntoFocusedField(
      bool is_password,
      const std::u16string& user_provided_credential) {}

#if BUILDFLAG(IS_ANDROID)
  // Informs the renderer that the Touch To Fill sheet has been closed.
  // Indicates whether the virtual keyboard should be shown instead.
  virtual void TouchToFillClosed(ShowVirtualKeyboard show_virtual_keyboard) {}

  // Triggers form submission on the last interacted web input element.
  virtual void TriggerFormSubmission() {}
#endif

  // Tells the driver to preview filling form with the |username| and
  // |password|.
  virtual void PreviewSuggestion(const std::u16string& username,
                                 const std::u16string& password) = 0;

  // Tells the driver to preview a password generation suggestion.
  virtual void PreviewGenerationSuggestion(const std::u16string& password) = 0;

  // Tells the driver to clear previewed password and username fields.
  virtual void ClearPreviewedForm() = 0;

  // Updates the autofill availability state of the DOM node with
  // |generation_element_id|. It is critical for a11y to keep it updated
  // to make proper announcements.
  virtual void SetSuggestionAvailability(
      autofill::FieldRendererId generation_element_id,
      const autofill::mojom::AutofillState state) = 0;

  // Returns the PasswordGenerationFrameHelper associated with this instance.
  virtual PasswordGenerationFrameHelper* GetPasswordGenerationHelper() = 0;

  // Returns the PasswordManager associated with this instance.
  virtual PasswordManagerInterface* GetPasswordManager() = 0;

  // Returns the PasswordAutofillManager associated with this instance.
  virtual PasswordAutofillManager* GetPasswordAutofillManager() = 0;

  // Sends a message to the renderer whether logging to
  // chrome://password-manager-internals is available.
  virtual void SendLoggingAvailability() {}

  // Return true iff the driver corresponds to the main frame.
  virtual bool IsInPrimaryMainFrame() const = 0;

  // Returns true iff a popup can be shown on the behalf of the associated
  // frame.
  virtual bool CanShowAutofillUi() const = 0;

  // Returns the ax tree id associated with this driver.
  virtual ::ui::AXTreeID GetAxTreeId() const = 0;

  // Returns the frame ID of the frame associated with this driver.
  virtual int GetFrameId() const = 0;

  // Returns the last committed URL of the frame.
  virtual const GURL& GetLastCommittedURL() const = 0;

  // Annotate password related (username, password) DOM input elements with
  // corresponding HTML attributes. It is used only for debugging.
  virtual void AnnotateFieldsWithParsingResult(
      const autofill::ParsingResult& parsing_result) {}
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_DRIVER_H_
