// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_ENTRY_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_ENTRY_H_

#include <string>

#include "base/time/time.h"

namespace autofill {

class AutocompleteKey {
 public:
  AutocompleteKey();
  AutocompleteKey(const std::u16string& name, const std::u16string& value);
  AutocompleteKey(const std::string& name, const std::string& value);
  AutocompleteKey(const AutocompleteKey& key);
  virtual ~AutocompleteKey();

  const std::u16string& name() const { return name_; }
  const std::u16string& value() const { return value_; }

  bool operator==(const AutocompleteKey& key) const;
  bool operator<(const AutocompleteKey& key) const;

 private:
  std::u16string name_;
  std::u16string value_;
};

class AutocompleteEntry {
 public:
  AutocompleteEntry();
  AutocompleteEntry(const AutocompleteKey& key,
                    const base::Time& date_created,
                    const base::Time& date_last_used);
  ~AutocompleteEntry();

  const AutocompleteKey& key() const { return key_; }
  const base::Time& date_created() const { return date_created_; }
  const base::Time& date_last_used() const { return date_last_used_; }

  bool operator==(const AutocompleteEntry& entry) const;
  bool operator!=(const AutocompleteEntry& entry) const;
  bool operator<(const AutocompleteEntry& entry) const;

 private:
  AutocompleteKey key_;
  base::Time date_created_;
  base::Time date_last_used_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_ENTRY_H_
