// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_STRING_SEARCH_H_
#define BASE_I18N_STRING_SEARCH_H_
#pragma once

#include "base/i18n/base_i18n_export.h"
#include "base/string16.h"

namespace base {
namespace i18n {

// Returns true if |in_this| contains |find_this|. Only differences between base
// letters are taken into consideration. Case and accent differences are
// ignored. Please refer to 'primary level' in
// http://userguide.icu-project.org/collation/concepts for additional details.
BASE_I18N_EXPORT
    bool StringSearchIgnoringCaseAndAccents(const string16& find_this,
                                            const string16& in_this);

}  // namespace i18n
}  // namespace base

#endif  // BASE_I18N_STRING_SEARCH_H_

