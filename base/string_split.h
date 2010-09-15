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

// The same as SplitString, but don't trim white space.
// Where wchar_t is char16 (i.e. Windows), |c| must be in BMP
// (Basic Multilingual Plane). Elsewhere (Linux/Mac), wchar_t
// should be a valid Unicode code point (32-bit).
void SplitStringDontTrim(const std::wstring& str,
                         wchar_t c,
                         std::vector<std::wstring>* r);
// NOTE: |c| must be in BMP (Basic Multilingual Plane)
void SplitStringDontTrim(const string16& str,
                         char16 c,
                         std::vector<string16>* r);
// |str| should not be in a multi-byte encoding like Shift-JIS or GBK in which
// the trailing byte of a multi-byte character can be in the ASCII range.
// UTF-8, and other single/multi-byte ASCII-compatible encodings are OK.
// Note: |c| must be in the ASCII range.
void SplitStringDontTrim(const std::string& str,
                         char c,
                         std::vector<std::string>* r);

}  // namespace base

#endif  // BASE_STRING_SPLIT_H
