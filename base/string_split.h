// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRING_SPLIT_H_
#define BASE_STRING_SPLIT_H_
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "base/string16.h"

namespace base {

bool SplitStringIntoKeyValues(
    const std::string& line,
    char key_value_delimiter,
    std::string* key, std::vector<std::string>* values);

bool SplitStringIntoKeyValuePairs(
    const std::string& line,
    char key_value_delimiter,
    char key_value_pair_delimiter,
    std::vector<std::pair<std::string, std::string> >* kv_pairs);

// The same as SplitString, but use a substring delimiter instead of a char.
void SplitStringUsingSubstr(const string16& str,
                            const string16& s,
                            std::vector<string16>* r);
void SplitStringUsingSubstr(const std::string& str,
                            const std::string& s,
                            std::vector<std::string>* r);

}  // namespace base

#endif  // BASE_STRING_SPLIT_H
