// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

class ImageBitmap;
class ImageBitmapOptions;
class ScriptState;

class CORE_EXPORT ImageBitmapSource {
  DISALLOW_NEW();

 public:
  virtual gfx::Size BitmapSourceSize() const { return gfx::Size(); }
  virtual ScriptPromise CreateImageBitmap(ScriptState*,
                                          absl::optional<gfx::Rect>,
                                          const ImageBitmapOptions*,
                                          ExceptionState&);

  virtual bool IsBlob() const { return false; }

  // TODO(crbug.com/1342260): Option imageOrientation: 'none' will be
  // deprecated. A deprecation warning will be shown to developers when it is
  // used. Adding |options| temporarily here to verify if 'none' is used, which
  // will be removed in the next milestone.
  static ScriptPromise FulfillImageBitmap(ScriptState*,
                                          ImageBitmap*,
                                          const ImageBitmapOptions* options,
                                          ExceptionState&);

 protected:
  virtual ~ImageBitmapSource() = default;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_IMAGEBITMAP_IMAGE_BITMAP_SOURCE_H_
