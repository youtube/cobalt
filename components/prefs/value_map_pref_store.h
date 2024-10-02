// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_VALUE_MAP_PREF_STORE_H_
#define COMPONENTS_PREFS_VALUE_MAP_PREF_STORE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/prefs/pref_value_map.h"
#include "components/prefs/prefs_export.h"
#include "components/prefs/writeable_pref_store.h"

// A basic PrefStore implementation that uses a simple name-value map for
// storing the preference values.
class COMPONENTS_PREFS_EXPORT ValueMapPrefStore : public WriteablePrefStore {
 public:
  ValueMapPrefStore();

  ValueMapPrefStore(const ValueMapPrefStore&) = delete;
  ValueMapPrefStore& operator=(const ValueMapPrefStore&) = delete;

  // PrefStore overrides:
  bool GetValue(base::StringPiece key,
                const base::Value** value) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;

  // WriteablePrefStore overrides:
  void SetValue(const std::string& key,
                base::Value value,
                uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool GetMutableValue(const std::string& key, base::Value** value) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValuesByPrefixSilently(const std::string& prefix) override;

 protected:
  ~ValueMapPrefStore() override;

  // Notify observers about the initialization completed event.
  void NotifyInitializationCompleted();

 private:
  PrefValueMap prefs_;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
};

#endif  // COMPONENTS_PREFS_VALUE_MAP_PREF_STORE_H_
