// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_split.h"

#include "base/logging.h"
#include "base/string_util.h"

namespace base {

bool SplitStringIntoKeyValues(
    const std::string& line,
    char key_value_delimiter,
    std::string* key, std::vector<std::string>* values) {
  key->clear();
  values->clear();

  // Find the key string.
  size_t end_key_pos = line.find_first_of(key_value_delimiter);
  if (end_key_pos == std::string::npos) {
    DLOG(INFO) << "cannot parse key from line: " << line;
    return false;    // no key
  }
  key->assign(line, 0, end_key_pos);

  // Find the values string.
  std::string remains(line, end_key_pos, line.size() - end_key_pos);
  size_t begin_values_pos = remains.find_first_not_of(key_value_delimiter);
  if (begin_values_pos == std::string::npos) {
    DLOG(INFO) << "cannot parse value from line: " << line;
    return false;   // no value
  }
  std::string values_string(remains, begin_values_pos,
                            remains.size() - begin_values_pos);

  // Construct the values vector.
  values->push_back(values_string);
  return true;
}

bool SplitStringIntoKeyValuePairs(
    const std::string& line,
    char key_value_delimiter,
    char key_value_pair_delimiter,
    std::vector<std::pair<std::string, std::string> >* kv_pairs) {
  kv_pairs->clear();

  std::vector<std::string> pairs;
  SplitString(line, key_value_pair_delimiter, &pairs);

  bool success = true;
  for (size_t i = 0; i < pairs.size(); ++i) {
    // Empty pair. SplitStringIntoKeyValues is more strict about an empty pair
    // line, so continue with the next pair.
    if (pairs[i].empty())
      continue;

    std::string key;
    std::vector<std::string> value;
    if (!SplitStringIntoKeyValues(pairs[i],
                                  key_value_delimiter,
                                  &key, &value)) {
      // Don't return here, to allow for keys without associated
      // values; just record that our split failed.
      success = false;
    }
    DCHECK_LE(value.size(), 1U);
    kv_pairs->push_back(make_pair(key, value.empty()? "" : value[0]));
  }
  return success;
}

template <typename STR>
static void SplitStringUsingSubstrT(const STR& str,
                                    const STR& s,
                                    std::vector<STR>* r) {
  typename STR::size_type begin_index = 0;
  while (true) {
    const typename STR::size_type end_index = str.find(s, begin_index);
    if (end_index == STR::npos) {
      const STR term = str.substr(begin_index);
      STR tmp;
      TrimWhitespace(term, TRIM_ALL, &tmp);
      r->push_back(tmp);
      return;
    }
    const STR term = str.substr(begin_index, end_index - begin_index);
    STR tmp;
    TrimWhitespace(term, TRIM_ALL, &tmp);
    r->push_back(tmp);
    begin_index = end_index + s.size();
  }
}

void SplitStringUsingSubstr(const string16& str,
                            const string16& s,
                            std::vector<string16>* r) {
  SplitStringUsingSubstrT(str, s, r);
}

void SplitStringUsingSubstr(const std::string& str,
                            const std::string& s,
                            std::vector<std::string>* r) {
  SplitStringUsingSubstrT(str, s, r);
}

}  // namespace base
