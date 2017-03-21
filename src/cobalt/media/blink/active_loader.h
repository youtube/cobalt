// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_MEDIA_BLINK_ACTIVE_LOADER_H_
#define COBALT_MEDIA_BLINK_ACTIVE_LOADER_H_

#include <memory>

#include "base/basictypes.h"
#include "cobalt/media/blink/media_blink_export.h"

namespace blink {
class WebURLLoader;
}

namespace media {

// Wraps an active WebURLLoader with some additional state.
//
// Handles deferring and deletion of loaders.
class MEDIA_BLINK_EXPORT ActiveLoader {
 public:
  // Creates an ActiveLoader with the given loader. It is assumed that the
  // initial state of |loader| is loading and not deferred.
  explicit ActiveLoader(std::unique_ptr<blink::WebURLLoader> loader);
  ~ActiveLoader();

  // Starts or stops deferring the resource load.
  void SetDeferred(bool deferred);
  bool deferred() { return deferred_; }

 private:
  friend class MultibufferDataSourceTest;

  std::unique_ptr<blink::WebURLLoader> loader_;
  bool deferred_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ActiveLoader);
};

}  // namespace media

#endif  // COBALT_MEDIA_BLINK_ACTIVE_LOADER_H_
