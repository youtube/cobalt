// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_ID_H_
#define COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_ID_H_

#include "base/types/id_type.h"

namespace tabs_api {

using DropTargetId = base::IdType64<class DropTargetTag>;

}  // namespace tabs_api

#endif  // COMPONENTS_BROWSER_APIS_TAB_DRAG_DESTINATIONS_DROP_TARGET_ID_H_
