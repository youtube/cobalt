// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_CHANNEL_TO_ENUM_H_
#define CHROMEOS_CROSAPI_CPP_CHANNEL_TO_ENUM_H_

#include "base/component_export.h"
#include "base/strings/string_piece.h"
#include "chromeos/crosapi/cpp/crosapi_constants.h"
#include "components/version_info/version_info.h"

namespace crosapi {

COMPONENT_EXPORT(CROSAPI)
version_info::Channel ChannelToEnum(base::StringPiece channel);

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_CHANNEL_TO_ENUM_H_
