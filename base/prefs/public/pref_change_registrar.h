// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_PUBLIC_PREF_CHANGE_REGISTRAR_H_
#define BASE_PREFS_PUBLIC_PREF_CHANGE_REGISTRAR_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/prefs/base_prefs_export.h"

class PrefServiceBase;

namespace content {
class NotificationObserver;
}

// Automatically manages the registration of one or more pref change observers
// with a PrefStore. Functions much like NotificationRegistrar, but specifically
// manages observers of preference changes. When the Registrar is destroyed,
// all registered observers are automatically unregistered with the PrefStore.
class BASE_PREFS_EXPORT PrefChangeRegistrar {
 public:
  PrefChangeRegistrar();
  virtual ~PrefChangeRegistrar();

  // Must be called before adding or removing observers. Can be called more
  // than once as long as the value of |service| doesn't change.
  void Init(PrefServiceBase* service);

  // Adds an pref observer for the specified pref |path| and |obs| observer
  // object. All registered observers will be automatically unregistered
  // when the registrar's destructor is called unless the observer has been
  // explicitly removed by a call to Remove beforehand.
  void Add(const char* path,
           content::NotificationObserver* obs);

  // Removes a preference observer that has previously been added with a call to
  // Add.
  void Remove(const char* path,
              content::NotificationObserver* obs);

  // Removes all observers that have been previously added with a call to Add.
  void RemoveAll();

  // Returns true if no pref observers are registered.
  bool IsEmpty() const;

  // Check whether |pref| is in the set of preferences being observed.
  bool IsObserved(const std::string& pref);

  // Check whether any of the observed preferences has the managed bit set.
  bool IsManaged();

 private:
  typedef std::pair<std::string, content::NotificationObserver*>
      ObserverRegistration;

  std::set<ObserverRegistration> observers_;
  PrefServiceBase* service_;

  DISALLOW_COPY_AND_ASSIGN(PrefChangeRegistrar);
};

#endif  // BASE_PREFS_PUBLIC_PREF_CHANGE_REGISTRAR_H_
