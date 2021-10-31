// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#if defined(HAVE_CORETEXT)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(HAVE_FREETYPE)
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#elif defined(HAVE_WIN32)
#define NOMINMAX
#include <Windows.h>
#endif

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "test-context.h"
#include "ots-memory-stream.h"

namespace {

int Usage(const char *argv0) {
  std::fprintf(stderr, "Usage: %s <ttf file>\n", argv0);
  return 1;
}

bool DumpResults(const uint8_t *result1, const size_t len1,
                 const uint8_t *result2, const size_t len2) {
  std::ofstream out1("out1.ttf", std::ofstream::out | std::ofstream::binary);
  std::ofstream out2("out2.ttf", std::ofstream::out | std::ofstream::binary);
  if (!out1.good() || !out2.good())
    return false;

  out1.write(reinterpret_cast<const char*>(result1), len1);
  out2.write(reinterpret_cast<const char*>(result2), len2);
  if (!out1.good() || !out2.good())
    return false;

  return true;
}

// Platform specific implementations.
bool VerifyTranscodedFont(uint8_t *result, const size_t len);

#if defined(HAVE_CORETEXT)
bool VerifyTranscodedFont(uint8_t *result, const size_t len) {
  CFDataRef data = CFDataCreate(0, result, len);
  if (!data) {
    return false;
  }

  CGDataProviderRef dataProvider = CGDataProviderCreateWithCFData(data);
  CGFontRef cgFontRef = CGFontCreateWithDataProvider(dataProvider);
  CGDataProviderRelease(dataProvider);
  CFRelease(data);
  if (!cgFontRef) {
    return false;
  }

  size_t numGlyphs = CGFontGetNumberOfGlyphs(cgFontRef);
  CGFontRelease(cgFontRef);
  if (!numGlyphs) {
    return false;
  }
  return true;
}

#elif defined(HAVE_FREETYPE)
bool VerifyTranscodedFont(uint8_t *result, const size_t len) {
  FT_Library library;
  FT_Error error = ::FT_Init_FreeType(&library);
  if (error) {
    return false;
  }
  FT_Face dummy;
  error = ::FT_New_Memory_Face(library, result, len, 0, &dummy);
  if (error) {
    return false;
  }
  ::FT_Done_Face(dummy);
  ::FT_Done_FreeType(library);
  return true;
}

#elif defined(HAVE_WIN32)
// Windows
bool VerifyTranscodedFont(uint8_t *result, const size_t len) {
  DWORD num_fonts = 0;
  HANDLE handle = AddFontMemResourceEx(result, len, 0, &num_fonts);
  if (!handle) {
    return false;
  }
  RemoveFontMemResourceEx(handle);
  return true;
}

#else
bool VerifyTranscodedFont(uint8_t *result, const size_t len) {
  std::fprintf(stderr, "Can't verify the transcoded font on this platform.\n");
  return false;
}

#endif

}  // namespace

int main(int argc, char **argv) {
  if (argc != 2) return Usage(argv[0]);

  std::ifstream ifs(argv[1], std::ifstream::binary);
  if (!ifs.good()) {
    std::fprintf(stderr, "Failed to read file!\n");
    return 1;
  }
  std::vector<uint8_t> in((std::istreambuf_iterator<char>(ifs)),
                          (std::istreambuf_iterator<char>()));

  // A transcoded font is usually smaller than an original font.
  // However, it can be slightly bigger than the original one due to
  // name table replacement and/or padding for glyf table.
  //
  // However, a WOFF font gets decompressed and so can be *much* larger than
  // the original.
  std::unique_ptr<uint8_t> result(new uint8_t[in.size() * 8]);
  ots::MemoryStream output(result.get(), in.size() * 8);

  ots::TestContext context(0);

  if (!context.Process(&output, in.data(), in.size())) {
    std::fprintf(stderr, "Failed to sanitize file!\n");
    return 1;
  }
  const size_t result_len = output.Tell();

  std::unique_ptr<uint8_t> result2(new uint8_t[result_len]);
  ots::MemoryStream output2(result2.get(), result_len);
  if (!context.Process(&output2, result.get(), result_len)) {
    std::fprintf(stderr, "Failed to sanitize previous output!\n");
    return 1;
  }
  const size_t result2_len = output2.Tell();

  bool dump_results = false;
  if (result2_len != result_len) {
    std::fprintf(stderr, "Outputs differ in length\n");
    dump_results = true;
  } else if (std::memcmp(result2.get(), result.get(), result_len)) {
    std::fprintf(stderr, "Outputs differ in content\n");
    dump_results = true;
  }

  if (dump_results) {
    std::fprintf(stderr, "Dumping results to out1.tff and out2.tff\n");
    if (!DumpResults(result.get(), result_len, result2.get(), result2_len)) {
      std::fprintf(stderr, "Failed to dump output files.\n");
      return 1;
    }
  }

  // Verify that original font can be opened by the font renderer. If this
  // fails then no point in verfying the transcoded font.
  if (!VerifyTranscodedFont(in.data(), in.size())) {
    std::fprintf(stderr, "Failed to verify the original font\n");
    return 0;
  }

  // Verify that the transcoded font can be opened by the font renderer for
  // Linux (FreeType2), Mac OS X, or Windows.
  if (!VerifyTranscodedFont(result.get(), result_len)) {
    std::fprintf(stderr, "Failed to verify the transcoded font\n");
    return 1;
  }

  return 0;
}
