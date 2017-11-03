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

#ifndef COBALT_RENDER_TREE_RESOURCE_PROVIDER_H_
#define COBALT_RENDER_TREE_RESOURCE_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/base/type_id.h"
#include "cobalt/render_tree/font.h"
#include "cobalt/render_tree/font_provider.h"
#include "cobalt/render_tree/glyph_buffer.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/render_tree/mesh.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/typeface.h"
#include "starboard/decode_target.h"

namespace cobalt {
namespace render_tree {

// A ResourceProvider is a thread-safe class that is usually provided by
// a specific render_tree consumer.  Its purpose is to generate render_tree
// resources that can be attached to a render_tree that would subsequently be
// submitted to that specific render_tree consumer.  While it depends on the
// details of the ResourceProvider, it is very likely that resources created by
// a ResourceProvider that came from a specific render_tree consumer should only
// be submitted back to that same render_tree consumer.  The object should be
// thread-safe since it will be very common for resources to be created on one
// thread, but consumed on another.
class ResourceProvider {
 public:
  typedef std::vector<uint8_t> RawTypefaceDataVector;

  // This matches the max size in WebKit
  static const size_t kMaxTypefaceDataSize = 30 * 1024 * 1024;  // 30 MB

  virtual ~ResourceProvider() {}

  // Returns an ID that is unique to the ResourceProvider type.  This can be
  // used to polymorphically identify what type of ResourceProvider this is.
  virtual base::TypeId GetTypeId() const = 0;

  // Blocks until it can be guaranteed that all resource-related operations have
  // completed.  This might be important if we would like to ensure that memory
  // allocations or deallocations have occurred before proceeding with a memory
  // intensive operation.
  virtual void Finish() = 0;

  // Returns true if AllocateImageData() supports the given |pixel_format|.
  virtual bool PixelFormatSupported(PixelFormat pixel_format) = 0;

  // Returns true if AllocateImageData() supports the given |alpha_format|.
  virtual bool AlphaFormatSupported(AlphaFormat alpha_format) = 0;

  // This method can be used to create an ImageData object.
  virtual scoped_ptr<ImageData> AllocateImageData(const math::Size& size,
                                                  PixelFormat pixel_format,
                                                  AlphaFormat alpha_format) = 0;

  // This function will consume an ImageData object produced by a call to
  // AllocateImageData(), wrap it in a render_tree::Image that can be
  // used in a render tree, and return it to the caller.
  virtual scoped_refptr<Image> CreateImage(
      scoped_ptr<ImageData> pixel_data) = 0;

#if SB_HAS(GRAPHICS)
  // This function will consume an SbDecodeTarget object produced by
  // SbDecodeTargetCreate(), wrap it in a render_tree::Image that can be used
  // in a render tree, and return it to the caller.
  virtual scoped_refptr<Image> CreateImageFromSbDecodeTarget(
      SbDecodeTarget target) = 0;

  // Whether SbDecodeTargetIsSupported or not.
  virtual bool SupportsSbDecodeTarget() = 0;

  // Return the SbDecodeTargetGraphicsContextProvider associated with the
  // ResourceProvider, if it exists.  Returns NULL if SbDecodeTarget is not
  // supported.
  virtual SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider() = 0;
#endif  // SB_HAS(GRAPHICS)

  // Returns a raw chunk of memory that can later be passed into a function like
  // CreateMultiPlaneImageFromRawMemory() in order to create a texture.
  // If possible, the memory returned will be GPU memory that can be directly
  // addressable by the GPU as a texture.
  // Creating textures through this method is discouraged since you must be
  // aware of your platform's image alignment/pitch requirements in order to
  // create a valid texture.  The function is useful in situations where, for
  // example, a video decoder requires a raw chunk of memory to decode, but is
  // not able to provide image format information until after it begins
  // decoding.
  virtual scoped_ptr<RawImageMemory> AllocateRawImageMemory(
      size_t size_in_bytes, size_t alignment) = 0;

  // Constructs a multi-plane image from a single contiguous chunk of raw
  // image memory.  Data for all planes of the image must lie within
  // raw_image_memory, and the descriptor's plane offset member will describe
  // where a particular plane's data lies relative to raw_image_memory.
  // Note that use of this function is discouraged, if possible, since filling
  // out the descriptor requires knowledge of the specific platform's texture
  // alignment/pitch requirements.
  virtual scoped_refptr<Image> CreateMultiPlaneImageFromRawMemory(
      scoped_ptr<RawImageMemory> raw_image_memory,
      const MultiPlaneImageDataDescriptor& descriptor) = 0;

  // Given a font family name, this method returns whether or not a local font
  // matching the name exists.
  virtual bool HasLocalFontFamily(const char* font_family_name) const = 0;

  // Given a set of typeface information, this method returns the locally
  // available typeface that best fits the specified parameters. In the case
  // where no typeface is found that matches the font family name, the default
  // typeface is returned.
  virtual scoped_refptr<Typeface> GetLocalTypeface(const char* font_family_name,
                                                   FontStyle font_style) = 0;

  // Given a set of typeface information, this method returns the locally
  // available typeface that best fits the specified parameters. In the case
  // where no typeface is found that matches the font family name, NULL is
  // returned.
  //
  // Font's typeface (aka face name) is combination of a style and a font
  // family.  Font's style consists of weight, and a slant (but not size).
  virtual scoped_refptr<Typeface> GetLocalTypefaceByFaceNameIfAvailable(
      const char* font_face_name) = 0;

  // Given a UTF-32 character, a set of typeface information, and a language,
  // this method returns the best-fit locally available fallback typeface that
  // provides a glyph for the specified character. In the case where no fallback
  // typeface is found that supports the character, the default typeface is
  // returned.
  virtual scoped_refptr<Typeface> GetCharacterFallbackTypeface(
      int32 utf32_character, FontStyle font_style,
      const std::string& language) = 0;

  // Given raw typeface data in either TrueType, OpenType or WOFF data formats,
  // this method creates and returns a new typeface. The typeface is not cached
  // by the resource provider. If the creation fails, the error is written out
  // to the error string.
  // Note that kMaxFontDataSize represents a hard cap on the raw data size.
  // Anything larger than that is guaranteed to result in typeface creation
  // failure.
  virtual scoped_refptr<Typeface> CreateTypefaceFromRawData(
      scoped_ptr<RawTypefaceDataVector> raw_data,
      std::string* error_string) = 0;

  // Given a UTF-16 text buffer, a font provider, and other shaping parameters,
  // this method shapes the text using fonts from the list and returns the glyph
  // buffer that will render it.
  // - |language| is an ISO 639-1 code used to enable the shaper to make
  //   more accurate decisions when character combinations that can produce
  //   different outcomes are encountered during shaping.
  // - If |is_rtl| is set to true, then the glyphs in the glyph buffer
  //   will be returned in reversed order.
  // - |font_list| is used to provide the shaper with a font-glyph combination
  //   for any requested character. The available fonts and the strategy used in
  //   determining the best font-glyph combination are encapsulated within the
  //   FontProvider object.
  virtual scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const char16* text_buffer, size_t text_length,
      const std::string& language, bool is_rtl,
      render_tree::FontProvider* font_provider) = 0;

  // Given a UTF-8 string and a single font, this method shapes the string
  // using the font and returns the glyph buffer that will render it.
  virtual scoped_refptr<render_tree::GlyphBuffer> CreateGlyphBuffer(
      const std::string& utf8_string,
      const scoped_refptr<render_tree::Font>& font) = 0;

  // Given a UTF-16 text buffer, a font provider, and other shaping parameters,
  // this method shapes the text using fonts from the list and returns the
  // width of the shaped text.
  // - |language| is an ISO 639-1 code used to enable the shaper to make
  //   more accurate decisions when character combinations that can produce
  //   different outcomes are encountered during shaping.
  // - If |is_rtl| is set to true, then the shaping will be generated in reverse
  //   order.
  // - |font_list| is used to provide the shaper with a font-glyph combination
  //   for any requested character. The available fonts and the strategy used in
  //   determining the best font-glyph combination are encapsulated within the
  //   FontProvider object.
  // - |maybe_used_fonts| is an optional parameter used to collect all fonts
  //   that were used in generating the width for the text.
  // NOTE: While shaping is done on the text in order to produce an accurate
  // width, a glyph buffer is never generated, so this method should be
  // faster than CreateGlyphBuffer().
  virtual float GetTextWidth(const char16* text_buffer, size_t text_length,
                             const std::string& language, bool is_rtl,
                             render_tree::FontProvider* font_provider,
                             render_tree::FontVector* maybe_used_fonts) = 0;

  // Consumes a list of vertices and returns a Mesh instance.
  virtual scoped_refptr<Mesh> CreateMesh(
      scoped_ptr<std::vector<Mesh::Vertex> > vertices,
      Mesh::DrawMode draw_mode) = 0;

  virtual scoped_refptr<Image> DrawOffscreenImage(
      const scoped_refptr<render_tree::Node>& root) = 0;
};

}  // namespace render_tree
}  // namespace cobalt

#endif  // COBALT_RENDER_TREE_RESOURCE_PROVIDER_H_
