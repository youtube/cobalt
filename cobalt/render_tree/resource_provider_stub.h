// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <string>
#include <vector>

#include "base/memory/aligned_memory.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/image.h"
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
  scoped_array<uint8> memory_;
};

// Simply wraps the ImageDataStub object and also makes it visible to the
// public so that tests can access the pixel data.
class ImageStub : public Image {
 public:
  explicit ImageStub(scoped_ptr<ImageDataStub> image_data)
      : image_data_(image_data.Pass()) {}

  const math::Size& GetSize() const override {
    return image_data_->GetDescriptor().size;
  }

  ImageDataStub* GetImageData() { return image_data_.get(); }

 private:
  ~ImageStub() override {}

  scoped_ptr<ImageDataStub> image_data_;
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
    UNREFERENCED_PARAMETER(glyph);
    return glyph_bounds_;
  }

  float GetGlyphWidth(GlyphIndex glyph) override {
    UNREFERENCED_PARAMETER(glyph);
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
  explicit TypefaceStub(const void* data) { UNREFERENCED_PARAMETER(data); }

  TypefaceId GetId() const override { return Internal::kDefaultTypefaceId; }

  uint32 GetEstimatedSizeInBytes() const override {
    return Internal::kDefaultTypefaceSizeInBytes;
  }

  scoped_refptr<Font> CreateFontWithSize(float font_size) override {
    return make_scoped_refptr(new FontStub(this, font_size));
  }

  GlyphIndex GetGlyphForCharacter(int32 utf32_character) override {
    UNREFERENCED_PARAMETER(utf32_character);
    return Internal::kDefaultGlyphIndex;
  }

 private:
  ~TypefaceStub() override {}
};

class RawImageMemoryStub : public RawImageMemory {
 public:
  typedef scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> ScopedMemory;

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
  MeshStub(scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
           render_tree::Mesh::DrawMode draw_mode)
      : vertices_(vertices.Pass()), draw_mode_(draw_mode) {}

  uint32 GetEstimatedSizeInBytes() const override {
    return static_cast<uint32>(vertices_->size() * 5 * sizeof(float) +
                               sizeof(DrawMode));
  }

  render_tree::Mesh::DrawMode GetDrawMode() const { return draw_mode_; }
  const std::vector<render_tree::Mesh::Vertex>& GetVertices() const {
    return *vertices_.get();
  }

 private:
  const scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices_;
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
    UNREFERENCED_PARAMETER(pixel_format);
    return true;
  }

  bool AlphaFormatSupported(AlphaFormat alpha_format) override {
    UNREFERENCED_PARAMETER(alpha_format);
    return true;
  }

  scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                          PixelFormat pixel_format,
                                          AlphaFormat alpha_format) override {
    return scoped_ptr<ImageData>(
        new ImageDataStub(size, pixel_format, alpha_format));
  }

  scoped_refptr<Image> CreateImage(scoped_ptr<ImageData> source_data) override {
    scoped_ptr<ImageDataStub> skia_source_data(
        base::polymorphic_downcast<ImageDataStub*>(source_data.release()));
    if (release_image_data_) {
      skia_source_data->ReleaseMemory();
    }
    return make_scoped_refptr(new ImageStub(skia_source_data.Pass()));
  }

#if SB_HAS(GRAPHICS)
  scoped_refptr<Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget decode_target) override {
    NOTREACHED();
    SbDecodeTargetRelease(decode_target);
    return NULL;
  }

  bool SupportsSbDecodeTarget() override { return false; }
#endif  // SB_HAS(GRAPHICS)

#if SB_HAS(GRAPHICS)
  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() override {
    return NULL;
  }
#endif  // SB_HAS(GRAPHICS)

  scoped_ptr<RawImageMemory> AllocateRawImageMemory(size_t size_in_bytes,
                                                    size_t alignment) override {
    return scoped_ptr<RawImageMemory>(
        new RawImageMemoryStub(size_in_bytes, alignment));
  }

  scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) override {
    UNREFERENCED_PARAMETER(raw_image_memory);
    UNREFERENCED_PARAMETER(descriptor);
    return scoped_refptr<Image>();
  }

  bool HasLocalFontFamily(const char* font_family_name) const override {
    UNREFERENCED_PARAMETER(font_family_name);
    return true;
  }

  scoped_refptr<Typeface> GetLocalTypeface(const char* font_family_name,
                                           FontStyle font_style) override {
    UNREFERENCED_PARAMETER(font_family_name);
    UNREFERENCED_PARAMETER(font_style);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<render_tree::Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) override {
    UNREFERENCED_PARAMETER(font_face_name);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<Typeface> GetCharacterFallbackTypeface(
      int32 utf32_character, FontStyle font_style,
      const std::string& language) override {
    UNREFERENCED_PARAMETER(utf32_character);
    UNREFERENCED_PARAMETER(font_style);
    UNREFERENCED_PARAMETER(language);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<Typeface> CreateTypefaceFromRawData(
      scoped_ptr<RawTypefaceDataVector> raw_data,
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
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  float GetTextWidth(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     FontProvider* font_provider,
                     FontVector* maybe_used_fonts) override {
    UNREFERENCED_PARAMETER(text_buffer);
    UNREFERENCED_PARAMETER(language);
    UNREFERENCED_PARAMETER(is_rtl);
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
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      FontProvider* font_provider) override {
    UNREFERENCED_PARAMETER(text_buffer);
    UNREFERENCED_PARAMETER(language);
    UNREFERENCED_PARAMETER(is_rtl);
    render_tree::GlyphIndex glyph_index;
    const scoped_refptr<render_tree::Font>& font =
        font_provider->GetCharacterFont(Internal::kDefaultCharacter,
                                        &glyph_index);
    const math::RectF& glyph_bounds = font->GetGlyphBounds(glyph_index);
    return make_scoped_refptr(new GlyphBuffer(
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
    return make_scoped_refptr(new GlyphBuffer(math::RectF(
        0, glyph_bounds.y(), glyph_bounds.width() * utf8_string.size(),
        glyph_bounds.height())));
  }

  // Create a mesh which can map replaced boxes to 3D shapes.
  scoped_refptr<render_tree::Mesh> CreateMesh(
      scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      render_tree::Mesh::DrawMode draw_mode) override {
    return new MeshStub(vertices.Pass(), draw_mode);
  }

  scoped_refptr<Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) override {
    UNREFERENCED_PARAMETER(root);
    return scoped_refptr<Image>(NULL);
  }

  bool release_image_data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
