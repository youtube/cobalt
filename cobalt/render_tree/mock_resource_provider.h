// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDER_TREE_MOCK_RESOURCE_PROVIDER_H_
#define COBALT_RENDER_TREE_MOCK_RESOURCE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/render_tree/typeface.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace render_tree {

class MockResourceProvider : public ResourceProvider {
 public:
  base::TypeId GetTypeId() const OVERRIDE {
    return base::GetTypeId<MockResourceProvider>();
  }

  MOCK_METHOD0(Finish, void());
  MOCK_METHOD1(PixelFormatSupported, bool(PixelFormat pixel_format));
  MOCK_METHOD1(AlphaFormatSupported, bool(AlphaFormat alpha_format));
  MOCK_METHOD3(AllocateImageDataMock,
               ImageData*(const math::Size& size, PixelFormat pixel_format,
                          AlphaFormat alpha_format));
  MOCK_METHOD1(CreateImageMock, Image*(ImageData* pixel_data));
  MOCK_METHOD2(AllocateRawImageMemoryMock,
               RawImageMemory*(size_t size_in_bytes, size_t alignment));
  MOCK_METHOD2(CreateMultiPlaneImageFromRawMemoryMock,
               Image*(RawImageMemory* raw_image_memory,
                      const MultiPlaneImageDataDescriptor& descriptor));
  MOCK_CONST_METHOD1(HasLocalFontFamily, bool(const char* font_family_name));
  MOCK_METHOD2(GetLocalTypefaceMock,
               Typeface*(const char* font_family_name, FontStyle font_style));
  MOCK_METHOD1(GetLocalTypefaceIfAvailableMock,
               Typeface*(const std::string& font_face_name));
  MOCK_METHOD3(GetCharacterFallbackTypefaceMock,
               Typeface*(int32 utf32_character, FontStyle font_style,
                         const std::string& language));
  MOCK_METHOD2(CreateTypefaceFromRawDataMock,
               Typeface*(RawTypefaceDataVector* raw_data,
                         std::string* error_string));
  MOCK_METHOD5(
      CreateGlyphBufferMock,
      render_tree::GlyphBuffer*(const char16* text_buffer, size_t text_length,
                                const std::string& language, bool is_rtl,
                                render_tree::FontProvider* font_provider));
  MOCK_METHOD2(
      CreateGlyphBufferMock,
      render_tree::GlyphBuffer*(const std::string& utf8_string,
                                const scoped_refptr<render_tree::Font>& font));
  MOCK_METHOD6(GetTextWidth,
               float(const char16* text_buffer, size_t text_length,
                     const std::string& language, bool is_rtl,
                     render_tree::FontProvider* font_provider,
                     render_tree::FontVector* maybe_used_fonts));

  MOCK_METHOD2(CreateMeshMock,
               render_tree::Mesh*(std::vector<render_tree::Mesh::Vertex>*,
                                  render_tree::Mesh::DrawMode));

  MOCK_METHOD1(DrawOffscreenImage,
               scoped_refptr<render_tree::Image>(
                   const scoped_refptr<render_tree::Node>& root));

  scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                          PixelFormat pixel_format,
                                          AlphaFormat alpha_format) {
    return scoped_ptr<ImageData>(
        AllocateImageDataMock(size, pixel_format, alpha_format));
  }
  scoped_refptr<Image> CreateImage(scoped_ptr<ImageData> pixel_data) {
    return scoped_refptr<Image>(CreateImageMock(pixel_data.get()));
  }

#if SB_HAS(GRAPHICS)

  scoped_refptr<Image> CreateImageFromSbDecodeTarget(SbDecodeTarget target) {
    UNREFERENCED_PARAMETER(target);
    return NULL;
  }

  bool SupportsSbDecodeTarget() { return false; }

  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() {
    return NULL;
  }

#endif  // SB_HAS(GRAPHICS)

  scoped_ptr<RawImageMemory> AllocateRawImageMemory(size_t size_in_bytes,
                                                    size_t alignment) {
    return scoped_ptr<RawImageMemory>(
        AllocateRawImageMemoryMock(size_in_bytes, alignment));
  }
  scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) {
    return scoped_refptr<Image>(CreateMultiPlaneImageFromRawMemoryMock(
        raw_image_memory.get(), descriptor));
  }
  scoped_refptr<Typeface> GetLocalTypeface(const char* font_family_name,
                                           FontStyle font_style) {
    return scoped_refptr<Typeface>(
        GetLocalTypefaceMock(font_family_name, font_style));
  }
  scoped_refptr<Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) {
    return scoped_refptr<Typeface>(
        GetLocalTypefaceIfAvailableMock(font_face_name));
  }
  scoped_refptr<Typeface> GetCharacterFallbackTypeface(
      int32 utf32_character, FontStyle font_style,
      const std::string& language) {
    return scoped_refptr<Typeface>(GetCharacterFallbackTypefaceMock(
        utf32_character, font_style, language));
  }
  scoped_refptr<Typeface> CreateTypefaceFromRawData(
      scoped_ptr<RawTypefaceDataVector> raw_data, std::string* error_string) {
    return scoped_refptr<Typeface>(
        CreateTypefaceFromRawDataMock(raw_data.get(), error_string));
  }
  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      render_tree::FontProvider* font_provider) {
    return scoped_refptr<render_tree::GlyphBuffer>(CreateGlyphBufferMock(
        text_buffer, text_length, language, is_rtl, font_provider));
  }
  scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<render_tree::Font>& font) {
    return scoped_refptr<render_tree::GlyphBuffer>(
        CreateGlyphBufferMock(utf8_string, font.get()));
  }
  virtual scoped_refptr<Mesh> CreateMesh(
      scoped_ptr<std::vector<Mesh::Vertex> > vertices,
      Mesh::DrawMode draw_mode) {
    return make_scoped_refptr(CreateMeshMock(vertices.get(), draw_mode));
  }
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_MOCK_RESOURCE_PROVIDER_H_
