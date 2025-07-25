// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"

namespace ash {

// Handles setting correct value for shill::kTetheringAllowedProperty property
// depending on ash::features::kHotspot flag. This Shill property value is
// updated when the handler initializes or Shill signals a
// shill::kTetheringAllowedProperty changed. Note, setting this value to true
// is a pre-requisite of successfully enable/disable hotspot and check
// tethering readiness in Shill.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) HotspotAllowedFlagHandler
    : public ShillPropertyChangedObserver {
 public:
  HotspotAllowedFlagHandler();
  HotspotAllowedFlagHandler(const HotspotAllowedFlagHandler&) = delete;
  HotspotAllowedFlagHandler& operator=(const HotspotAllowedFlagHandler&) =
      delete;
  ~HotspotAllowedFlagHandler() override;

  void Init();

 private:
  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

  // Callback when the SetHotspotAllowed flag operation failed.
  void OnSetHotspotAllowedFlagFailure(const std::string& error_name,
                                      const std::string& error_message);

  base::WeakPtrFactory<HotspotAllowedFlagHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_HOTSPOT_ALLOWED_FLAG_HANDLER_H_
