// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/scoped_ole_initializer.h"

#include <ole2.h>

#include <ostream>

#include "base/check_op.h"

namespace ui {

ScopedOleInitializer::ScopedOleInitializer() : hr_(OleInitialize(NULL)) {
  DCHECK_NE(OLE_E_WRONGCOMPOBJ, hr_) << "Incompatible DLLs on machine";
  DCHECK_NE(RPC_E_CHANGED_MODE, hr_) << "Invalid COM thread model change";
}

ScopedOleInitializer::~ScopedOleInitializer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (SUCCEEDED(hr_))
    OleUninitialize();
}

}  // namespace ui
