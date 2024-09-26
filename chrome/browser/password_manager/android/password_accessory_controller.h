// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/accessory_controller.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-forward.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/credential_cache.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

// Interface for password-specific keyboard accessory controller between the
// ManualFillingController and PasswordManagerClient.
//
// There is a single instance per WebContents that can be accessed by calling:
//     PasswordAccessoryController::GetOrCreate(web_contents);
// On the first call, an instance is attached to |web_contents|, so it can be
// returned by subsequent calls.
class PasswordAccessoryController
    : public base::SupportsWeakPtr<PasswordAccessoryController>,
      public AccessoryController {
 public:
  PasswordAccessoryController() = default;

  PasswordAccessoryController(const PasswordAccessoryController&) = delete;
  PasswordAccessoryController& operator=(const PasswordAccessoryController&) =
      delete;

  ~PasswordAccessoryController() override = default;

  // Returns a reference to the unique PasswordAccessoryController associated
  // with |web_contents|. A new instance is created if the first time this
  // function is called.
  static PasswordAccessoryController* GetOrCreate(
      content::WebContents* web_contents,
      password_manager::CredentialCache* credential_cache);

  // Returns a reference to the unique PasswordAccessoryController associated
  // with |web_contents|. Returns null if no such instance exists.
  static PasswordAccessoryController* GetIfExisting(
      content::WebContents* web_contents);

  // -----------------------------
  // Methods called by the client:
  // -----------------------------

  // Makes sure, that all shown suggestions are appropriate for the currently
  // focused field and for fields that lost the focus.
  virtual void RefreshSuggestionsForField(
      autofill::mojom::FocusedFieldType focused_field_type,
      bool is_manual_generation_available) = 0;

  // Signals that generation was requested from the accessory. |type|
  // indicates whether generation was requested via the manual fallback or from
  // the automatically provided button.
  virtual void OnGenerationRequested(
      autofill::password_generation::PasswordGenerationType type) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_ACCESSORY_CONTROLLER_H_
