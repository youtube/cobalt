/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
#define RENDER_TREE_RESOURCE_PROVIDER_STUB_H_

#include <string>

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace render_tree {

// The ResourceProvider defined in this file provides a bare minimum of
// implementation necessary.  It is useful for tests that do not care about
// actually rasterizing render trees.  For certain resources like Images,
// it provides introspection of internal pixel data so that tests can check
// that images do indeed contain the data they are expected to contain.

// Simple in-memory pixel data.
class ImageDataStub : public ImageData {
 public:
  ImageDataStub(const math::Size& size, PixelFormat pixel_format,
                AlphaFormat alpha_format)
      : descriptor_(size, pixel_format, alpha_format,
                    size.width() * BytesPerPixel(pixel_format)),
        memory_(new uint8[static_cast<size_t>(size.height() *
                                              descriptor_.pitch_in_bytes)]) {}

  const ImageDataDescriptor& GetDescriptor() const OVERRIDE {
    return descriptor_;
  }

  uint8* GetMemory() OVERRIDE { return memory_.get(); }

 private:
  ImageDataDescriptor descriptor_;
  scoped_array<uint8> memory_;
};

// Simply wraps the ImageDataStub object and also makes it visible to the
// public so that tests can access the pixel data.
class ImageStub : public Image {
 public:
  explicit ImageStub(scoped_ptr<ImageDataStub> image_data)
      : image_data_(image_data.Pass()) {}

  const math::Size& GetSize() const OVERRIDE {
    return image_data_->GetDescriptor().size;
  }

  ImageDataStub* GetImageData() { return image_data_.get(); }

 private:
  ~ImageStub() OVERRIDE {}

  scoped_ptr<ImageDataStub> image_data_;
};

// Simple class that returns dummy data for metric information.
class FontStub : public Font {
 public:
  FontStub(const void* data, size_t size)
      : font_data_size_(static_cast<uint32>(size)) {
    UNREFERENCED_PARAMETER(data);
  }

  math::RectF GetBounds(const std::string& text) const OVERRIDE {
    FontMetrics font_metrics = GetFontMetrics();
    return math::RectF(
        0, 0,
        static_cast<float>(text.length() * 10),
        font_metrics.ascent + font_metrics.descent);
  }

  FontMetrics GetFontMetrics() const OVERRIDE {
    return FontMetrics(10, 5, 3, 6);
  }

  uint32 GetEstimatedSizeInBytes() const OVERRIDE { return font_data_size_; }

  scoped_refptr<render_tree::Font> CloneWithSize(
      float font_size) const OVERRIDE {
    UNREFERENCED_PARAMETER(font_size);
    return NULL;
  }

 private:
  ~FontStub() OVERRIDE {}

  uint32 font_data_size_;
};

// Return the stub versions defined above for each resource.
class ResourceProviderStub : public ResourceProvider {
 public:
  ~ResourceProviderStub() OVERRIDE {}

  scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                          PixelFormat pixel_format,
                                          AlphaFormat alpha_format) OVERRIDE {
    return scoped_ptr<ImageData>(
        new ImageDataStub(size, pixel_format, alpha_format));
  }

  scoped_refptr<Image> CreateImage(scoped_ptr<ImageData> source_data) OVERRIDE {
    scoped_ptr<ImageDataStub> skia_source_data(
        base::polymorphic_downcast<ImageDataStub*>(source_data.release()));
    return make_scoped_refptr(new ImageStub(skia_source_data.Pass()));
  }

  scoped_ptr<RawImageMemory> AllocateRawImageMemory(size_t size_in_bytes,
                                                    size_t alignment) OVERRIDE {
    UNREFERENCED_PARAMETER(size_in_bytes);
    UNREFERENCED_PARAMETER(alignment);
    return scoped_ptr<RawImageMemory>();
  }

  scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) OVERRIDE {
    UNREFERENCED_PARAMETER(raw_image_memory);
    UNREFERENCED_PARAMETER(descriptor);
    return scoped_refptr<Image>();
  }

  scoped_refptr<Font> GetPreInstalledFont(const char* font_family_name,
                                          FontStyle font_style,
                                          float font_size) OVERRIDE {
    UNREFERENCED_PARAMETER(font_family_name);
    UNREFERENCED_PARAMETER(font_style);
    UNREFERENCED_PARAMETER(font_size);
    return make_scoped_refptr(new FontStub(NULL, 0));
  }

  scoped_refptr<render_tree::Font> CreateFontFromRawData(
      scoped_ptr<RawFontDataVector> raw_data,
      std::string* error_string) OVERRIDE {
    UNREFERENCED_PARAMETER(raw_data);
    UNREFERENCED_PARAMETER(error_string);
    return make_scoped_refptr(new FontStub(NULL, 0));
  }
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
