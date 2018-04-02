// Copyright 2014 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_RENDER_TREE_IMAGE_H_
#define COBALT_RENDER_TREE_IMAGE_H_

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace render_tree {

// Formats of pixel data that we support creating images from.
enum PixelFormat {
  kPixelFormatRGBA8,
  kPixelFormatBGRA8,
  kPixelFormatUYVY,
  kPixelFormatY8,
  kPixelFormatU8,
  kPixelFormatV8,
  kPixelFormatUV8,
  kPixelFormatInvalid,
};

inline int BytesPerPixel(PixelFormat pixel_format) {
  switch (pixel_format) {
    case kPixelFormatRGBA8:
      return 4;
    case kPixelFormatBGRA8:
      return 4;
    case kPixelFormatY8:
      return 1;
    case kPixelFormatU8:
      return 1;
    case kPixelFormatV8:
      return 1;
    case kPixelFormatUV8:
      return 2;
    case kPixelFormatUYVY:
    case kPixelFormatInvalid:
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

  // Indicates that all alpha values in the image data are opaque.  If
  // non-opaque alpha data is used with this format, visual output will be
  // undefined. This information may be used to enable optimizations, and can
  // result in Image::IsOpaque() returning true.
  kAlphaFormatOpaque,
};

// Describes the format of a contiguous block of memory that represents an
// image.  This descriptor can only describe image formats where all information
// for a given pixel is stored contiguously in memory, and all pixels are
// sequentially ordered.  For image data that is stored within multiple planes,
// see MultiPlaneImageDataDescriptor below.
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

class RawImageMemory {
 public:
  virtual ~RawImageMemory() {}

  virtual size_t GetSizeInBytes() const = 0;
  virtual uint8_t* GetMemory() = 0;
};

// Specifies formats for multi-plane images, typically produced by video
// decoders.
enum MultiPlaneImageFormat {
  // A YUV image where each channel, Y, U and V, is stored as a separate
  // single-channel image plane.
  kMultiPlaneImageFormatYUV3PlaneBT709,
  // A YUV image where the Y channel is stored as a single-channel image plane
  // and the U and V channels are interleaved in a second image plane.
  kMultiPlaneImageFormatYUV2PlaneBT709,
  // A YUV image where each channel, Y, U and V, is stored as a separate
  // single-channel 10bit unnormalized image plane.
  kMultiPlaneImageFormatYUV3Plane10BitBT2020,
};

// Like the ImageDataDescriptor object, a MultiPlaneImageDataDescriptor
// describes the format of a multiplane image pixel data.  A multi plane
// image is an image who's pixel data is split among multiple contiguous
// regions of memory (planes).  For example, many video decoders output
// YUV image data where each of the three channels, Y, U and V, are stored
// separately as their own single channel images.
// The MultiPlaneImageDataDescriptor describes each channel in terms of a
// standard single plane ImageDataDescriptor object.  In addition, a
// MultiPlaneImageFormat label is also assigned to the the image to describe
// how separate channels should be combined to form the final image.
class MultiPlaneImageDataDescriptor {
 public:
  // For efficiency reasons (to avoid dynamic memory allocations) we define a
  // maximum number of supported planes.
  static const int kMaxPlanes = 3;

  explicit MultiPlaneImageDataDescriptor(MultiPlaneImageFormat image_format)
      : image_format_(image_format), num_planes_(0) {}

  // Pushes a new plane descriptor onto the list of planes.  Multi-plane images
  // are defined over one large contiguous region of memory, and each plane
  // occupies a subset of that memory.  The offset parameter specifies where
  // to find the newly added plane relative to a contiguous region of memory.
  // For example, in 3 plane YUV, the V plane would have an offset at least
  // larger than the size of the data for the Y plane plus the U plane.
  void AddPlane(intptr_t offset, const ImageDataDescriptor& descriptor) {
    DCHECK_GT(kMaxPlanes, num_planes_);
    plane_descriptors_[num_planes_].emplace(offset, descriptor);
    ++num_planes_;
  }

  // Returns the multi-plane image format of this image as a whole.
  MultiPlaneImageFormat image_format() const { return image_format_; }

  // Returns the number of planes described by this descriptor (i.e. the number
  // of times AddPlane() has been called).
  int num_planes() const { return num_planes_; }

  // Returns the offset specified by AddPlane() for the given plane_index.
  intptr_t GetPlaneOffset(int plane_index) const {
    DCHECK_LE(0, plane_index);
    DCHECK_GT(num_planes_, plane_index);
    return plane_descriptors_[plane_index]->offset;
  }

  // Returns the single-plane image descriptor specified by AddPlane() for the
  // given plane_index.
  const ImageDataDescriptor& GetPlaneDescriptor(int plane_index) const {
    DCHECK_LE(0, plane_index);
    DCHECK_GT(num_planes_, plane_index);
    return plane_descriptors_[plane_index]->descriptor;
  }

 private:
  struct PlaneInformation {
    PlaneInformation(intptr_t offset, const ImageDataDescriptor& descriptor)
        : offset(offset), descriptor(descriptor) {}

    intptr_t offset;
    ImageDataDescriptor descriptor;
  };

  MultiPlaneImageFormat image_format_;
  int num_planes_;

  // We keep an array of base::optionals so that we don't have to specify a
  // default constructor for PlaneInformation.
  base::optional<PlaneInformation> plane_descriptors_[kMaxPlanes];
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

  // The default implementation is to estimate the size based on the width and
  // height. Derived classes may override this calculation with a more accurate
  // one.
  virtual uint32 GetEstimatedSizeInBytes() const {
    return static_cast<uint32>(GetSize().width() * GetSize().height() *
                               BytesPerPixel(kPixelFormatRGBA8));
  }

  // If an Image is able to know that it contains no alpha data (e.g. if it
  // was constructed from ImageData whose alpha format is set to
  // kAlphaFormatOpaque), then this method can return true, and code can
  // be written to take advantage of this and perform optimizations.
  virtual bool IsOpaque() const { return false; }

 protected:
  virtual ~Image() {}

  // Allow the reference counting system access to our destructor.
  friend class base::RefCountedThreadSafe<Image>;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_IMAGE_H_
