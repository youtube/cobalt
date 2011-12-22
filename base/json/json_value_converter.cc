// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_value_converter.h"

#include <string>

namespace base {
namespace internal {

FieldConverterBase::FieldConverterBase(const std::string& path)
    : field_path_(path) {
}

FieldConverterBase::~FieldConverterBase() {
}

}  // namespace internal
}  // namespace base
