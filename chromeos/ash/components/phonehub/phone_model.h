// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/browser_tabs_model.h"
#include "chromeos/ash/components/phonehub/phone_status_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace phonehub {

// Model representing the phone used for Phone Hub. Provides getters which
// return the state of the phone when connected, or null if disconnected. Also
// exposes an observer interface so that clients can be notified of changes to
// the model.
class PhoneModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when some part of the model has changed.
    virtual void OnModelChanged() = 0;
  };

  PhoneModel(const PhoneModel&) = delete;
  PhoneModel& operator=(const PhoneModel&) = delete;
  virtual ~PhoneModel();

  const absl::optional<std::u16string>& phone_name() const {
    return phone_name_;
  }

  const absl::optional<PhoneStatusModel>& phone_status_model() const {
    return phone_status_model_;
  }

  const absl::optional<BrowserTabsModel>& browser_tabs_model() const {
    return browser_tabs_model_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  PhoneModel();

  void NotifyModelChanged();

  absl::optional<std::u16string> phone_name_;
  absl::optional<PhoneStatusModel> phone_status_model_;
  absl::optional<BrowserTabsModel> browser_tabs_model_;

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_PHONE_MODEL_H_
