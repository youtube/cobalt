// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/starboard/client_native_pixmap_factory_starboard.h"

#include "base/logging.h"
#include "ui/ozone/common/stub_client_native_pixmap_factory.h"

namespace ui {

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryStarboard() {
  LOG(INFO) << "CreateClientNativePixmapFactoryStarboard";
  return CreateStubClientNativePixmapFactory();
}

}  // namespace ui
