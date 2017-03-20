// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/media/blink/active_loader.h"

#include <utility>

#include "third_party/WebKit/public/platform/WebURLLoader.h"

namespace media {

ActiveLoader::ActiveLoader(std::unique_ptr<blink::WebURLLoader> loader)
    : loader_(std::move(loader)), deferred_(false) {}

ActiveLoader::~ActiveLoader() { loader_->cancel(); }

void ActiveLoader::SetDeferred(bool deferred) {
  deferred_ = deferred;
  loader_->setDefersLoading(deferred);
}

}  // namespace media
