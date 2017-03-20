// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/cdm_initialized_promise.h"

namespace media {

CdmInitializedPromise::CdmInitializedPromise(
    const CdmCreatedCB& cdm_created_cb, const scoped_refptr<MediaKeys>& cdm)
    : cdm_created_cb_(cdm_created_cb), cdm_(cdm) {}

CdmInitializedPromise::~CdmInitializedPromise() {}

void CdmInitializedPromise::resolve() {
  MarkPromiseSettled();
  cdm_created_cb_.Run(cdm_, "");
}

void CdmInitializedPromise::reject(MediaKeys::Exception exception_code,
                                   uint32_t system_code,
                                   const std::string& error_message) {
  MarkPromiseSettled();
  cdm_created_cb_.Run(NULL, error_message);
  // Usually after this |this| (and the |cdm_| within it) will be destroyed.
}

}  // namespace media
