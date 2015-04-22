/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef RENDER_TREE_IMAGE_H_
#define RENDER_TREE_IMAGE_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace render_tree {

// Formats of pixel data that we support creating images from.
enum PixelFormat {
  kPixelFormatRGBA8,
  kPixelFormatInvalid,
};

inline int BytesPerPixel(PixelFormat pixel_format) {
  switch (pixel_format) {
    case kPixelFormatRGBA8:
      return 4;
    case kPixelFormatInvalid:
    default:
      DLOG(FATAL) << "Unexpected pixel format.";
  }
  return -1;
}

enum AlphaFormat {
  // Premultiplied alpha means that the RGB components (in terms of
  // the range [0.0, 1.0]) have already been multiplied by the A component
  // (also in the range [0.0, 1.0]).  Thus, it is expected that for all
  // pixels, each component is less than or equal to the alpha component.
  kAlphaFormatPremultiplied,

  // This alpha format implies standard alpha, where each component is
  // independent of the alpha.
  kAlphaFormatUnpremultiplied,
};

struct ImageDataDescriptor {
  ImageDataDescriptor(const math::Size& size, PixelFormat pixel_format,
                      AlphaFormat alpha_format, int pitch_in_bytes)
      : size(size),
        pixel_format(pixel_format),
        alpha_format(alpha_format),
        pitch_in_bytes(pitch_in_bytes) {}

  math::Size size;
  PixelFormat pixel_format;
  AlphaFormat alpha_format;
  int pitch_in_bytes;
};

// ImageData is an interface for an object that contains an allocation
// of CPU-accessible memory that is intended to be passed in to CreateImage()
// so that it may be used by the GPU.
class ImageData {
 public:
  virtual ~ImageData() {}

  // Returns information about the kind of data this ImageData is
  // intended to store.
  virtual const ImageDataDescriptor& GetDescriptor() const = 0;

  // Returns a pointer to the image data so that one can set pixel data as
  // necessary.
  virtual uint8_t* GetMemory() = 0;
};

// The Image type is an abstract base class that represents a stored image
// and all of its pixel information.  When constructing a render tree,
// external images can be introduced by adding an ImageNode and associating it
// with a specific Image object.  Examples of concrete Image objects include
// an Image that stores its pixel data in a CPU memory buffer, or one that
// stores its image data as a GPU texture.  Regardless, the concrete type of
// an Image objects is not relevant unless the Image is being constructed or
// it is being read by a rasterizer reading a submitted render tree.  Since
// the rasterizer may only be compatible with specific concrete Image types,
// it is expected that the object will be safely downcast by the rasterizer
// to a rasterizer-specific Image type using base::polymorphic_downcast().
class Image : public base::RefCountedThreadSafe<Image> {
 public:
  virtual const math::Size& GetSize() const = 0;

 protected:
  virtual ~Image() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Image>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_IMAGE_H_
