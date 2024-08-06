// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/base/renderer_factory.h"

namespace media_m96 {

RendererFactory::RendererFactory() = default;

RendererFactory::~RendererFactory() = default;

MediaResource::Type RendererFactory::GetRequiredMediaResourceType() {
  return MediaResource::Type::STREAM;
}

}  // namespace media_m96
