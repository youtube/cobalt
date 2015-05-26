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

#include "cobalt/render_tree/resource_provider.h"

namespace cobalt {
namespace render_tree {

// The ResourceProvider defined in this file provides a bare minimum of
// implementation necessary.  It is useful for tests that do not care about
// actually rasterizing render trees.  For certain resources like Images,
// it provides introspection of internal pixel data so that tests can check
// that images do indeed contain the data they are expected to contain.

// Simple in-memory pixel data.
class ImageDataStub : public render_tree::ImageData {
 public:
  ImageDataStub(const math::Size& size, render_tree::PixelFormat pixel_format,
                 render_tree::AlphaFormat alpha_format)
      : descriptor_(size, pixel_format, alpha_format,
                    size.width() * render_tree::BytesPerPixel(pixel_format)),
        memory_(new uint8[static_cast<size_t>(
                              size.height() * descriptor_.pitch_in_bytes)]) {}

  const render_tree::ImageDataDescriptor& GetDescriptor() const OVERRIDE {
    return descriptor_;
  }

  uint8* GetMemory() OVERRIDE { return memory_.get(); }

 private:
  render_tree::ImageDataDescriptor descriptor_;
  scoped_array<uint8> memory_;
};

// Simply wraps the ImageDataStub object and also makes it visible to the
// public so that tests can access the pixel data.
class ImageStub : public render_tree::Image {
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
class FontStub : public render_tree::Font {
 public:
  math::RectF GetBounds(const std::string& text) const OVERRIDE {
    FontMetrics font_metrics = GetFontMetrics();
    return math::RectF(
        0, 0,
        static_cast<float>(text.length() * 10),
        font_metrics.ascent + font_metrics.descent);
  }

  FontMetrics GetFontMetrics() const OVERRIDE {
    return FontMetrics(10, 5, 3);
  }

 private:
  ~FontStub() OVERRIDE {}
};

// Return the stub versions defined above for each resource.
class ResourceProviderStub : public render_tree::ResourceProvider {
 public:
  ~ResourceProviderStub() OVERRIDE {}

  scoped_ptr<render_tree::ImageData> AllocateImageData(
      const math::Size& size, render_tree::PixelFormat pixel_format,
      render_tree::AlphaFormat alpha_format) OVERRIDE {
    return scoped_ptr<render_tree::ImageData>(
        new ImageDataStub(size, pixel_format, alpha_format));
  }

  scoped_refptr<render_tree::Image> CreateImage(
      scoped_ptr<render_tree::ImageData> source_data) OVERRIDE {
    scoped_ptr<ImageDataStub> skia_source_data(
        base::polymorphic_downcast<ImageDataStub*>(source_data.release()));
    return make_scoped_refptr(new ImageStub(skia_source_data.Pass()));
  }

  scoped_ptr<render_tree::RawImageMemory> AllocateRawImageMemory(
      size_t size_in_bytes, size_t alignment) OVERRIDE {
    return scoped_ptr<render_tree::RawImageMemory>();
  }

  scoped_refptr<render_tree::Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<render_tree::RawImageMemory> raw_image_memory,
      const render_tree::MultiPlaneImageDataDescriptor& descriptor) OVERRIDE {
    return scoped_refptr<render_tree::Image>();
  }

  scoped_refptr<render_tree::Font> GetPreInstalledFont(
      const char* font_family_name, render_tree::FontStyle font_style,
      float font_size) OVERRIDE {
    return scoped_refptr<render_tree::Font>();
  }
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
