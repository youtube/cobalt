// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_DEFAULT_PREF_STORE_H_
#define BASE_PREFS_DEFAULT_PREF_STORE_H_

#include <string>

#include "base/prefs/base_prefs_export.h"
#include "base/prefs/value_map_pref_store.h"
#include "base/values.h"

// This PrefStore keeps track of default preference values set when a
// preference is registered with the PrefService.
class BASE_PREFS_EXPORT DefaultPrefStore : public ValueMapPrefStore {
 public:
  DefaultPrefStore();

  // Stores a new |value| for |key|. Assumes ownership of |value|.
  void SetDefaultValue(const std::string& key, Value* value);

  // Removes the value for |key|.
  void RemoveDefaultValue(const std::string& key);

  // Returns the registered type for |key| or Value::TYPE_NULL if the |key|
  // has not been registered.
  base::Value::Type GetType(const std::string& key) const;

 protected:
  virtual ~DefaultPrefStore();

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultPrefStore);
};

#endif  // BASE_PREFS_DEFAULT_PREF_STORE_H_
