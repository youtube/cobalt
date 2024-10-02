// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill/address_accessory_controller.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class ManualFillingController;

namespace autofill {
class PersonalDataManager;

// Use either AddressAccessoryController::GetOrCreate or
// AddressAccessoryController::GetIfExisting to obtain instances of this class.
// This class exists for every tab and should never store state based on the
// contents of one of its frames.
class AddressAccessoryControllerImpl
    : public AddressAccessoryController,
      public PersonalDataManagerObserver,
      public content::WebContentsUserData<AddressAccessoryControllerImpl> {
 public:
  AddressAccessoryControllerImpl(const AddressAccessoryControllerImpl&) =
      delete;
  AddressAccessoryControllerImpl& operator=(
      const AddressAccessoryControllerImpl&) = delete;

  ~AddressAccessoryControllerImpl() override;

  // AccessoryController:
  void RegisterFillingSourceObserver(FillingSourceObserver observer) override;
  absl::optional<AccessorySheetData> GetSheetData() const override;
  void OnFillingTriggered(FieldGlobalId focused_field_id,
                          const AccessorySheetField& selection) override;
  void OnOptionSelected(AccessoryAction selected_action) override;
  void OnToggleChanged(AccessoryAction toggled_action, bool enabled) override;

  // AddressAccessoryController:
  void RefreshSuggestions() override;

  // PersonalDataManagerObserver:
  void OnPersonalDataChanged() override;

  // Like |CreateForWebContents|, it creates the controller and attaches it to
  // the given |web_contents|. Additionally, it allows inject a manual filling
  // controller.
  static void CreateForWebContentsForTesting(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

 private:
  friend class content::WebContentsUserData<AddressAccessoryControllerImpl>;

  // Required for construction via |CreateForWebContents|:
  explicit AddressAccessoryControllerImpl(content::WebContents* contents);

  // Constructor that allows to inject a mock filling controller.
  AddressAccessoryControllerImpl(
      content::WebContents* web_contents,
      base::WeakPtr<ManualFillingController> mf_controller);

  // Lazy-initializes and returns the ManualFillingController for the current
  // |web_contents_|. The lazy initialization allows injecting mocks for tests.
  base::WeakPtr<ManualFillingController> GetManualFillingController();

  // The observer to notify if available suggestions change.
  FillingSourceObserver source_observer_;

  // The password accessory controller object to forward client requests to.
  base::WeakPtr<ManualFillingController> mf_controller_;

  // The data manager used to retrieve the profiles.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ADDRESS_ACCESSORY_CONTROLLER_IMPL_H_
