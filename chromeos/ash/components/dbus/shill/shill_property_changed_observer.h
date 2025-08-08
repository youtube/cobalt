// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROPERTY_CHANGED_OBSERVER_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROPERTY_CHANGED_OBSERVER_H_

#include <string>

namespace base {
class Value;
}

namespace ash {

// This is a base class for observers which handle the PropertyChanged signal
// sent from Shill.
class ShillPropertyChangedObserver {
 public:
  virtual void OnPropertyChanged(const std::string& name,
                                 const base::Value& value) = 0;

 protected:
  virtual ~ShillPropertyChangedObserver() {}
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SHILL_SHILL_PROPERTY_CHANGED_OBSERVER_H_
