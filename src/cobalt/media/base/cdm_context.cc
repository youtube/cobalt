// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/base/cdm_context.h"

namespace media {

const int CdmContext::kInvalidCdmId = 0;

CdmContext::CdmContext() {}

CdmContext::~CdmContext() {}

void IgnoreCdmAttached(bool /* success */) {}

}  // namespace media
