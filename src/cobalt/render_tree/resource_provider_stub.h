// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
#define COBALT_RENDER_TREE_RESOURCE_PROVIDER_STUB_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/aligned_memory.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/lottie_animation.h"
#include "cobalt/render_tree/mesh.h"
#include "cobalt/render_tree/resource_provider.h"
#include "third_party/ots/include/opentype-sanitiser.h"
#include "third_party/ots/include/ots-memory-stream.h"

namespace cobalt {
namespace render_tree {

namespace Internal {

const int kDefaultTypefaceSizeInBytes = 256;
const render_tree::TypefaceId kDefaultTypefaceId = 0xffffffff;

const float kRobotoAscentSizeMultiplier = 0.927734f;
const float kRobotoDescentSizeMultiplier = 0.244141f;
const float kRobotoLeadingSizeMultiplier = 0.f;
const float kRobotoXHeightSizeMultiplier = 0.52832f;

const int32 kDefaultCharacter = 48;  // Decimal value for '0'
const render_tree::GlyphIndex kDefaultGlyphIndex = 1;

const float kDefaultCharacterRobotoGlyphWidthSizeMultiplier = 0.562012f;
const float kDefaultCharacterRobotoGlyphHeightSizeMultiplier = 0.7f;

}  // namespace Internal

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

  const ImageDataDescriptor& GetDescriptor() const override {
    return descriptor_;
  }

  void ReleaseMemory() { memory_.reset(); }
  uint8* GetMemory() override { return memory_.get(); }

 private:
  ImageDataDescriptor descriptor_;
  std::unique_ptr<uint8[]> memory_;
};

// Wraps an ImageDataStub object or a RawImageMemory and its associated image
// descriptor.  It also makes the wrapped object visible to the public so that
// tests can access the pixel data.
class ImageStub : public Image {
 public:
  explicit ImageStub(std::unique_ptr<ImageDataStub> image_data)
      : image_data_(std::move(image_data)) {}
  ImageStub(std::unique_ptr<RawImageMemory> raw_image_memory,
            const MultiPlaneImageDataDescriptor& multi_plane_descriptor)
      : raw_image_memory_(std::move(raw_image_memory)),
        multi_plane_descriptor_(multi_plane_descriptor) {}

  const math::Size& GetSize() const override {
    return is_multi_plane_image()
               ? multi_plane_descriptor_->GetPlaneDescriptor(0).size
               : image_data_->GetDescriptor().size;
  }

  bool is_multi_plane_image() const {
    if (image_data_ == NULL) {
      DCHECK(raw_image_memory_ != NULL);
      return true;
    }
    DCHECK(raw_image_memory_ == NULL);
    return false;
  }

  ImageDataStub* GetImageData() {
    DCHECK(!is_multi_plane_image());
    return image_data_.get();
  }

  RawImageMemory* GetRawImageMemory() {
    DCHECK(is_multi_plane_image());
    return raw_image_memory_.get();
  }

  const MultiPlaneImageDataDescriptor& multi_plane_descriptor() const {
    DCHECK(is_multi_plane_image());
    return multi_plane_descriptor_.value();
  }

 private:
  ~ImageStub() override {}

  std::unique_ptr<ImageDataStub> image_data_;

  std::unique_ptr<RawImageMemory> raw_image_memory_;
  base::Optional<MultiPlaneImageDataDescriptor> multi_plane_descriptor_;
};

// Simple class that returns dummy data for metric information modeled on
// Roboto.
class FontStub : public Font {
 public:
  FontStub(const scoped_refptr<Typeface>& typeface, float font_size)
      : typeface_(typeface),
        font_metrics_(Internal::kRobotoAscentSizeMultiplier * font_size,
                      Internal::kRobotoDescentSizeMultiplier * font_size,
                      Internal::kRobotoLeadingSizeMultiplier * font_size,
                      Internal::kRobotoXHeightSizeMultiplier * font_size),
        glyph_bounds_(
            0,
            std::max(
                Internal::kDefaultCharacterRobotoGlyphHeightSizeMultiplier *
                    font_size,
                1.0f),
            Internal::kDefaultCharacterRobotoGlyphWidthSizeMultiplier *
                font_size,
            std::max(
                Internal::kDefaultCharacterRobotoGlyphHeightSizeMultiplier *
                    font_size,
                1.0f)) {}

  TypefaceId GetTypefaceId() const override { return typeface_->GetId(); }

  FontMetrics GetFontMetrics() const override { return font_metrics_; }

  GlyphIndex GetGlyphForCharacter(int32 utf32_character) override {
    return typeface_->GetGlyphForCharacter(utf32_character);
  }

  const math::RectF& GetGlyphBounds(GlyphIndex glyph) override {
    return glyph_bounds_;
  }

  float GetGlyphWidth(GlyphIndex glyph) override {
    return glyph_bounds_.width();
  }

 private:
  ~FontStub() override {}

  const scoped_refptr<Typeface> typeface_;
  const FontMetrics font_metrics_;
  math::RectF glyph_bounds_;
};

// Simple class that returns dummy data for metric information modeled on
// Roboto.
class TypefaceStub : public Typeface {
 public:
  explicit TypefaceStub(const void* data) {}

  TypefaceId GetId() const override { return Internal::kDefaultTypefaceId; }

  uint32 GetEstimatedSizeInBytes() const override {
    return Internal::kDefaultTypefaceSizeInBytes;
  }

  scoped_refptr<Font> CreateFontWithSize(float font_size) override {
    return base::WrapRefCounted(new FontStub(this, font_size));
  }

  GlyphIndex GetGlyphForCharacter(int32 utf32_character) override {
    return Internal::kDefaultGlyphIndex;
  }

 private:
  ~TypefaceStub() override {}
};

class RawImageMemoryStub : public RawImageMemory {
 public:
  typedef std::unique_ptr<uint8_t, base::AlignedFreeDeleter> ScopedMemory;

  RawImageMemoryStub(size_t size_in_bytes, size_t alignment)
      : size_in_bytes_(size_in_bytes) {
    memory_ = ScopedMemory(
        static_cast<uint8_t*>(base::AlignedAlloc(size_in_bytes, alignment)));
  }

  size_t GetSizeInBytes() const override { return size_in_bytes_; }

  uint8_t* GetMemory() override { return memory_.get(); }

 private:
  ~RawImageMemoryStub() override {}

  size_t size_in_bytes_;
  ScopedMemory memory_;
};

class MeshStub : public render_tree::Mesh {
 public:
  MeshStub(std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
           render_tree::Mesh::DrawMode draw_mode)
      : vertices_(std::move(vertices)), draw_mode_(draw_mode) {}

  uint32 GetEstimatedSizeInBytes() const override {
    return static_cast<uint32>(vertices_->size() * 5 * sizeof(float) +
                               sizeof(DrawMode));
  }

  render_tree::Mesh::DrawMode GetDrawMode() const { return draw_mode_; }
  const std::vector<render_tree::Mesh::Vertex>& GetVertices() const {
    return *vertices_.get();
  }

 private:
  const std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices_;
  const render_tree::Mesh::DrawMode draw_mode_;
};

// Return the stub versions defined above for each resource.
class ResourceProviderStub : public ResourceProvider {
 public:
  ResourceProviderStub() : release_image_data_(false) {}
  explicit ResourceProviderStub(bool release_image_data)
      : release_image_data_(release_image_data) {}
  ~ResourceProviderStub() override {}

  base::TypeId GetTypeId() const override {
    return base::GetTypeId<ResourceProviderStub>();
  }

  void Finish() override {}

  bool PixelFormatSupported(PixelFormat pixel_format) override {
    return true;
  }

  bool AlphaFormatSupported(AlphaFormat alpha_format) override {
    return true;
  }

  std::unique_ptr<ImageData> AllocateImageData(
      const math::Size& size, PixelFormat pixel_format,
      AlphaFormat alpha_format) override {
    return std::unique_ptr<ImageData>(
        new ImageDataStub(size, pixel_format, alpha_format));
  }

  scoped_refptr<Image> CreateImage(
      std::unique_ptr<ImageData> source_data) override {
    std::unique_ptr<ImageDataStub> skia_source_data(
        base::polymorphic_downcast<ImageDataStub*>(source_data.release()));
    if (release_image_data_) {
      skia_source_data->ReleaseMemory();
    }
    return base::WrapRefCounted(new ImageStub(std::move(skia_source_data)));
  }

  scoped_refptr<Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget decode_target) override {
    NOTREACHED();
    SbDecodeTargetRelease(decode_target);
    return NULL;
  }

  bool SupportsSbDecodeTarget() override { return false; }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
    return NULL;
  }

  std::unique_ptr<RawImageMemory> AllocateRawImageMemory(
      size_t size_in_bytes, size_t alignment) override {
    RawImageMemory* ptr = new RawImageMemoryStub(size_in_bytes, alignment);
    return std::unique_ptr<RawImageMemory>(ptr);
  }

  scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      std::unique_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) override {
    return base::WrapRefCounted(
        new ImageStub(std::move(raw_image_memory), descriptor));
  }

  bool HasLocalFontFamily(const char* font_family_name) const override {
    return true;
  }

  scoped_refptr<Typeface> GetLocalTypeface(const char* font_family_name,
                                           FontStyle font_style) override {
    return base::WrapRefCounted(new TypefaceStub(NULL));
  }

  scoped_refptr<render_tree::Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) override {
    return base::WrapRefCounted(new TypefaceStub(NULL));
  }

  scoped_refptr<Typeface> GetCharacterFallbackTypeface(
      int32 utf32_character, FontStyle font_style,
      const std::string& language) override {
    return base::WrapRefCounted(new TypefaceStub(NULL));
  }

  void LoadAdditionalFonts() override {}

  scoped_refptr<Typeface> CreateTypefaceFromRawData(
      std::unique_ptr<RawTypefaceDataVector> raw_data,
      std::string* error_string) override {
    if (raw_data == NULL) {
      *error_string = "No data to process";
      return NULL;
    }

    ots::OTSContext context;
    ots::ExpandingMemoryStream sanitized_data(
        raw_data->size(), render_tree::ResourceProvider::kMaxTypefaceDataSize);
    if (!context.Process(&sanitized_data, &((*raw_data)[0]),
                         raw_data->size())) {
      *error_string = "OpenType sanitizer unable to process data";
      return NULL;
    }
    return base::WrapRefCounted(new TypefaceStub(NULL));
  }

  float GetTextWidth(const base::char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     FontProvider* font_provider,
                     FontVector* maybe_used_fonts) override {
    render_tree::GlyphIndex glyph_index;
    const scoped_refptr<render_tree::Font>& font =
        font_provider->GetCharacterFont(Internal::kDefaultCharacter,
                                        &glyph_index);
    if (maybe_used_fonts) {
      maybe_used_fonts->push_back(font);
    }
    return font->GetGlyphWidth(glyph_index) * text_length;
  }

  // Creates a glyph buffer, which is populated with shaped text, and used to
  // render that text.
  scoped_refptr<GlyphBuffer> CreateGlyphBuffer(
      const base::char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      FontProvider* font_provider) override {
    render_tree::GlyphIndex glyph_index;
    const scoped_refptr<render_tree::Font>& font =
        font_provider->GetCharacterFont(Internal::kDefaultCharacter,
                                        &glyph_index);
    const math::RectF& glyph_bounds = font->GetGlyphBounds(glyph_index);
    return base::WrapRefCounted(new GlyphBuffer(
        math::RectF(0, glyph_bounds.y(), glyph_bounds.width() * text_length,
                    glyph_bounds.height())));
  }

  // Creates a glyph buffer, which is populated with shaped text, and used to
  // render that text.
  scoped_refptr<GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<Font>& font) override {
    const math::RectF& glyph_bounds =
        font->GetGlyphBounds(Internal::kDefaultGlyphIndex);
    return base::WrapRefCounted(new GlyphBuffer(math::RectF(
        0, glyph_bounds.y(), glyph_bounds.width() * utf8_string.size(),
        glyph_bounds.height())));
  }

  scoped_refptr<LottieAnimation> CreateLottieAnimation(const char* data,
                                                       size_t length) override {
    return scoped_refptr<LottieAnimation>(NULL);
  }

  // Create a mesh which can map replaced boxes to 3D shapes.
  scoped_refptr<render_tree::Mesh> CreateMesh(
      std::unique_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      render_tree::Mesh::DrawMode draw_mode) override {
    return new MeshStub(std::move(vertices), draw_mode);
  }

  scoped_refptr<Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) override {
    return scoped_refptr<Image>(NULL);
  }

  bool release_image_data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
