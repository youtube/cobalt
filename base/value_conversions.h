// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_VALUE_CONVERSIONS_H_
#define BASE_VALUE_CONVERSIONS_H_
#pragma once

// This file contains methods to convert things to a |Value| and back.

#include "base/base_export.h"

class FilePath;

namespace base {

class Time;
class StringValue;
class Value;

// The caller takes ownership of the returned value.
BASE_EXPORT StringValue* CreateFilePathValue(const FilePath& in_value);
BASE_EXPORT bool GetValueAsFilePath(const Value& value, FilePath* file_path);

BASE_EXPORT StringValue* CreateTimeValue(const Time& time);
BASE_EXPORT bool GetValueAsTime(const Value& value, Time* time);

}  // namespace

#endif  // BASE_VALUE_CONVERSIONS_H_
