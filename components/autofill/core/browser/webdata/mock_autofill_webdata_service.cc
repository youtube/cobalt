// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "base/task/single_thread_task_runner.h"

namespace autofill {

MockAutofillWebDataService::MockAutofillWebDataService()
    : AutofillWebDataService(
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault()) {}

MockAutofillWebDataService::~MockAutofillWebDataService() = default;

}  // namespace autofill
