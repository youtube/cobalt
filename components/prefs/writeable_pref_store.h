// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_WRITEABLE_PREF_STORE_H_
#define COMPONENTS_PREFS_WRITEABLE_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "components/prefs/pref_store.h"

namespace base {
class Value;
}

// A pref store that can be written to as well as read from.
class COMPONENTS_PREFS_EXPORT WriteablePrefStore : public PrefStore {
 public:
  // PrefWriteFlags can be used to change the way a pref will be written to
  // storage.
  enum PrefWriteFlags : uint32_t {
    // No flags are specified.
    DEFAULT_PREF_WRITE_FLAGS = 0,

    // This marks the pref as "lossy". There is no strict time guarantee on when
    // a lossy pref will be persisted to permanent storage when it is modified.
    LOSSY_PREF_WRITE_FLAG = 1 << 1
  };

  WriteablePrefStore() {}

  WriteablePrefStore(const WriteablePrefStore&) = delete;
  WriteablePrefStore& operator=(const WriteablePrefStore&) = delete;

  // Sets a |value| for |key| in the store. |flags| is a bitmask of
  // PrefWriteFlags.
  virtual void SetValue(const std::string& key,
                        base::Value value,
                        uint32_t flags) = 0;

  // Removes the value for |key|.
  virtual void RemoveValue(const std::string& key, uint32_t flags) = 0;

  // Equivalent to PrefStore::GetValue but returns a mutable value.
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) = 0;

  // Triggers a value changed notification. This function or
  // ReportSubValuesChanged needs to be called if one retrieves a list or
  // dictionary with GetMutableValue and change its value. SetValue takes care
  // of notifications itself. Note that ReportValueChanged will trigger
  // notifications even if nothing has changed.  |flags| is a bitmask of
  // PrefWriteFlags.
  virtual void ReportValueChanged(const std::string& key, uint32_t flags) = 0;

  // Triggers a value changed notification for |path_components| in the |key|
  // pref. This function or ReportValueChanged needs to be called if one
  // retrieves a list or dictionary with GetMutableValue and change its value.
  // SetValue takes care of notifications itself. Note that
  // ReportSubValuesChanged will trigger notifications even if nothing has
  // changed. |flags| is a bitmask of PrefWriteFlags.
  virtual void ReportSubValuesChanged(
      const std::string& key,
      std::set<std::vector<std::string>> path_components,
      uint32_t flags);

  // Same as SetValue, but doesn't generate notifications. This is used by
  // PrefService::GetMutableUserPref() in order to put empty entries
  // into the user pref store. Using SetValue is not an option since existing
  // tests rely on the number of notifications generated. |flags| is a bitmask
  // of PrefWriteFlags.
  virtual void SetValueSilently(const std::string& key,
                                base::Value value,
                                uint32_t flags) = 0;

  // Clears all the preferences which names start with |prefix| and doesn't
  // generate update notifications.
  virtual void RemoveValuesByPrefixSilently(const std::string& prefix) = 0;

 protected:
  ~WriteablePrefStore() override {}
};

#endif  // COMPONENTS_PREFS_WRITEABLE_PREF_STORE_H_
