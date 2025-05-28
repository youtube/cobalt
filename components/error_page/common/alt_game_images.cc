// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/error_page/common/alt_game_images.h"

#include <memory>
#include <string>

#include "base/base64url.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "components/error_page/common/alt_game_image_data.h"
#include "components/error_page/common/error_page_switches.h"

namespace error_page {

bool EnableAltGameMode() {
  return false;
}

std::string GetAltGameImage(int image_id, int scale) {
  NOTIMPLEMENTED();
  return std::string();
}

int ChooseAltGame() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace error_page
