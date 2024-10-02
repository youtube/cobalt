// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/modaldialog/android/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace browser_ui {

BASE_FEATURE(kModalDialogButtonProtection,
             "ModalDialogButtonProtection",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace browser_ui
