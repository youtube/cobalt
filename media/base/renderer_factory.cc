// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory.h"

namespace media {

RendererFactory::RendererFactory() = default;

RendererFactory::~RendererFactory() = default;

MediaResource::Type RendererFactory::GetRequiredMediaResourceType() {
  return MediaResource::Type::STREAM;
}

}  // namespace media
