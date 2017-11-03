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

  const ImageDataDescriptor& GetDescriptor() const OVERRIDE {
    return descriptor_;
  }

  void ReleaseMemory() { memory_.reset(); }
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

  TypefaceId GetTypefaceId() const OVERRIDE { return typeface_->GetId(); }

  FontMetrics GetFontMetrics() const OVERRIDE { return font_metrics_; }

  GlyphIndex GetGlyphForCharacter(int32 utf32_character) OVERRIDE {
    return typeface_->GetGlyphForCharacter(utf32_character);
  }

  const math::RectF& GetGlyphBounds(GlyphIndex glyph) OVERRIDE {
    UNREFERENCED_PARAMETER(glyph);
    return glyph_bounds_;
  }

  float GetGlyphWidth(GlyphIndex glyph) OVERRIDE {
    UNREFERENCED_PARAMETER(glyph);
    return glyph_bounds_.width();
  }

 private:
  ~FontStub() OVERRIDE {}

  const scoped_refptr<Typeface> typeface_;
  const FontMetrics font_metrics_;
  math::RectF glyph_bounds_;
};

// Simple class that returns dummy data for metric information modeled on
// Roboto.
class TypefaceStub : public Typeface {
 public:
  explicit TypefaceStub(const void* data) { UNREFERENCED_PARAMETER(data); }

  TypefaceId GetId() const OVERRIDE { return Internal::kDefaultTypefaceId; }

  uint32 GetEstimatedSizeInBytes() const OVERRIDE {
    return Internal::kDefaultTypefaceSizeInBytes;
  }

  scoped_refptr<Font> CreateFontWithSize(float font_size) OVERRIDE {
    return make_scoped_refptr(new FontStub(this, font_size));
  }

  GlyphIndex GetGlyphForCharacter(int32 utf32_character) OVERRIDE {
    UNREFERENCED_PARAMETER(utf32_character);
    return Internal::kDefaultGlyphIndex;
  }

 private:
  ~TypefaceStub() OVERRIDE {}
};

class RawImageMemoryStub : public RawImageMemory {
 public:
  typedef scoped_ptr_malloc<uint8_t, base::ScopedPtrAlignedFree> ScopedMemory;

  RawImageMemoryStub(size_t size_in_bytes, size_t alignment)
      : size_in_bytes_(size_in_bytes) {
    memory_ = ScopedMemory(
        static_cast<uint8_t*>(base::AlignedAlloc(size_in_bytes, alignment)));
  }

  size_t GetSizeInBytes() const OVERRIDE { return size_in_bytes_; }

  uint8_t* GetMemory() OVERRIDE { return memory_.get(); }

 private:
  ~RawImageMemoryStub() OVERRIDE {}

  size_t size_in_bytes_;
  ScopedMemory memory_;
};

class MeshStub : public render_tree::Mesh {
 public:
  MeshStub(scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
           render_tree::Mesh::DrawMode draw_mode)
      : vertices_(vertices.Pass()), draw_mode_(draw_mode) {}

  uint32 GetEstimatedSizeInBytes() const OVERRIDE {
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
  ~ResourceProviderStub() OVERRIDE {}

  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<ResourceProviderStub>();
  }

  void Finish() OVERRIDE {}

  bool PixelFormatSupported(PixelFormat pixel_format) OVERRIDE {
    UNREFERENCED_PARAMETER(pixel_format);
    return true;
  }

  bool AlphaFormatSupported(AlphaFormat alpha_format) OVERRIDE {
    UNREFERENCED_PARAMETER(alpha_format);
    return true;
  }

  scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                          PixelFormat pixel_format,
                                          AlphaFormat alpha_format) OVERRIDE {
    return scoped_ptr<ImageData>(
        new ImageDataStub(size, pixel_format, alpha_format));
  }

  scoped_refptr<Image> CreateImage(scoped_ptr<ImageData> source_data) OVERRIDE {
    scoped_ptr<ImageDataStub> skia_source_data(
        base::polymorphic_downcast<ImageDataStub*>(source_data.release()));
    if (release_image_data_) {
      skia_source_data->ReleaseMemory();
    }
    return make_scoped_refptr(new ImageStub(skia_source_data.Pass()));
  }

#if SB_HAS(GRAPHICS)
  scoped_refptr<Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget decode_target) OVERRIDE {
    NOTREACHED();
    SbDecodeTargetRelease(decode_target);
    return NULL;
  }

  bool SupportsSbDecodeTarget() OVERRIDE { return false; }
#endif  // SB_HAS(GRAPHICS)

#if SB_HAS(GRAPHICS)
  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() OVERRIDE {
    return NULL;
  }
#endif  // SB_HAS(GRAPHICS)

  scoped_ptr<RawImageMemory> AllocateRawImageMemory(size_t size_in_bytes,
                                                    size_t alignment) OVERRIDE {
    return scoped_ptr<RawImageMemory>(
        new RawImageMemoryStub(size_in_bytes, alignment));
  }

  scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) OVERRIDE {
    UNREFERENCED_PARAMETER(raw_image_memory);
    UNREFERENCED_PARAMETER(descriptor);
    return scoped_refptr<Image>();
  }

  bool HasLocalFontFamily(const char* font_family_name) const OVERRIDE {
    UNREFERENCED_PARAMETER(font_family_name);
    return true;
  }

  scoped_refptr<Typeface> GetLocalTypeface(const char* font_family_name,
                                           FontStyle font_style) OVERRIDE {
    UNREFERENCED_PARAMETER(font_family_name);
    UNREFERENCED_PARAMETER(font_style);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<render_tree::Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) OVERRIDE {
    UNREFERENCED_PARAMETER(font_face_name);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<Typeface> GetCharacterFallbackTypeface(
      int32 utf32_character, FontStyle font_style,
      const std::string& language) OVERRIDE {
    UNREFERENCED_PARAMETER(utf32_character);
    UNREFERENCED_PARAMETER(font_style);
    UNREFERENCED_PARAMETER(language);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  scoped_refptr<Typeface> CreateTypefaceFromRawData(
      scoped_ptr<RawTypefaceDataVector> raw_data,
      std::string* error_string) OVERRIDE {
    UNREFERENCED_PARAMETER(raw_data);
    UNREFERENCED_PARAMETER(error_string);
    return make_scoped_refptr(new TypefaceStub(NULL));
  }

  float GetTextWidth(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     FontProvider* font_provider,
                     FontVector* maybe_used_fonts) OVERRIDE {
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
      FontProvider* font_provider) OVERRIDE {
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
      const scoped_refptr<Font>& font) OVERRIDE {
    const math::RectF& glyph_bounds =
        font->GetGlyphBounds(Internal::kDefaultGlyphIndex);
    return make_scoped_refptr(new GlyphBuffer(math::RectF(
        0, glyph_bounds.y(), glyph_bounds.width() * utf8_string.size(),
        glyph_bounds.height())));
  }

  // Create a mesh which can map replaced boxes to 3D shapes.
  scoped_refptr<render_tree::Mesh> CreateMesh(
      scoped_ptr<std::vector<render_tree::Mesh::Vertex> > vertices,
      render_tree::Mesh::DrawMode draw_mode) OVERRIDE {
    return new MeshStub(vertices.Pass(), draw_mode);
  }

  scoped_refptr<Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) OVERRIDE {
    UNREFERENCED_PARAMETER(root);
    return scoped_refptr<Image>(NULL);
  }

  bool release_image_data_;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RESOURCE_PROVIDER_STUB_H_
