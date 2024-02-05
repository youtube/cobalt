// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_STORE_H_
#define COMPONENTS_PREFS_PREF_STORE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/prefs/prefs_export.h"


// This is an abstract interface for reading and writing from/to a persistent
// preference store, used by PrefService. An implementation using a JSON file
// can be found in JsonPrefStore, while an implementation without any backing
// store for testing can be found in TestingPrefStore. Furthermore, there is
// CommandLinePrefStore, which bridges command line options to preferences and
// ConfigurationPolicyPrefStore, which is used for hooking up configuration
// policy with the preference subsystem.
class COMPONENTS_PREFS_EXPORT PrefStore : public base::RefCounted<PrefStore> {
 public:
  // Observer interface for monitoring PrefStore.
  class COMPONENTS_PREFS_EXPORT Observer {
   public:
    // Called when the value for the given |key| in the store changes.
    virtual void OnPrefValueChanged(const std::string& key) = 0;
    // Notification about the PrefStore being fully initialized.
    virtual void OnInitializationCompleted(bool succeeded) = 0;

   protected:
    virtual ~Observer() {}
  };

  PrefStore() {}

  // Add and remove observers.
  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual bool HasObservers() const;

  // Whether the store has completed all asynchronous initialization.
  virtual bool IsInitializationComplete() const;

  // Get the value for a given preference |key| and stores it in |*result|.
  // |*result| is only modified if the return value is true and if |result|
  // is not NULL. Ownership of the |*result| value remains with the PrefStore.
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const = 0;

  // Get all the values. Never returns a null pointer.
  virtual base::Value::Dict GetValues() const = 0;

 protected:
  friend class base::RefCounted<PrefStore>;
  virtual ~PrefStore() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefStore);
};

#endif  // COMPONENTS_PREFS_PREF_STORE_H_
