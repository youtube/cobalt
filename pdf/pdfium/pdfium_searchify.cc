// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_searchify.h"

#include <math.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/numerics/angle_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_api_wrappers.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_mem_buffer_file_write.h"
#include "pdf/pdfium/pdfium_ocr.h"
#include "pdf/pdfium/pdfium_searchify_font.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/pdfium/public/cpp/fpdf_scopers.h"
#include "third_party/pdfium/public/fpdf_edit.h"
#include "third_party/pdfium/public/fpdf_save.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace chrome_pdf {

namespace {

std::vector<uint32_t> Utf8ToCharcodes(const std::string& string) {
  std::u16string utf16_str = base::UTF8ToUTF16(string);
  std::vector<uint32_t> charcodes;
  charcodes.reserve(utf16_str.size());
  for (auto c : utf16_str) {
    charcodes.push_back(c);
  }
  return charcodes;
}

// The coordinate systems between OCR and PDF are different. OCR's origin is at
// top-left, so we need to convert them to PDF's bottom-left.
SearchifyBoundingBoxOrigin ConvertToPdfOrigin(const gfx::Rect& rect,
                                              float angle,
                                              float coordinate_system_height) {
  const float theta = base::DegToRad(angle);
  const float x = rect.x() - (sinf(theta) * rect.height());
  const float y =
      coordinate_system_height - (rect.y() + cosf(theta) * rect.height());
  return {.point = {x, y}, .theta = -theta};
}

// Project the text object's origin to the baseline's origin.
SearchifyBoundingBoxOrigin ProjectToBaseline(
    const gfx::PointF& origin_point,
    const SearchifyBoundingBoxOrigin& baseline_origin) {
  const float sin_theta = sinf(baseline_origin.theta);
  const float cos_theta = cosf(baseline_origin.theta);
  // The length between `origin` and `baseline_origin`.
  const float length =
      (origin_point.x() - baseline_origin.point.x()) * cos_theta +
      (origin_point.y() - baseline_origin.point.y()) * sin_theta;
  return {.point = {baseline_origin.point.x() + length * cos_theta,
                    baseline_origin.point.y() + length * sin_theta},
          .theta = baseline_origin.theta};
}

gfx::SizeF GetRenderedImageSize(FPDF_PAGEOBJECT image) {
  FS_QUADPOINTSF quadpoints;
  if (!FPDFPageObj_GetRotatedBounds(image, &quadpoints)) {
    return gfx::SizeF();
  }

  return gfx::SizeF(
      hypotf(quadpoints.x1 - quadpoints.x2, quadpoints.y1 - quadpoints.y2),
      hypotf(quadpoints.x2 - quadpoints.x3, quadpoints.y2 - quadpoints.y3));
}

bool CalculateImageWithoutScalingMatrix(FPDF_PAGEOBJECT image,
                                        const gfx::SizeF& rendered_size,
                                        FS_MATRIX& image_matrix) {
  if (!FPDFPageObj_GetMatrix(image, &image_matrix)) {
    return false;
  }
  image_matrix.a /= rendered_size.width();
  image_matrix.b /= rendered_size.width();
  image_matrix.c /= rendered_size.height();
  image_matrix.d /= rendered_size.height();
  return true;
}

// Returns the transformation matrix needed to move a word to where it is
// positioned on the image.
FS_MATRIX CalculateWordMoveMatrix(const SearchifyBoundingBoxOrigin& word_origin,
                                  int word_bounding_box_width,
                                  bool word_is_rtl) {
  const float sin_theta = sinf(word_origin.theta);
  const float cos_theta = cosf(word_origin.theta);
  FS_MATRIX move_matrix(cos_theta, sin_theta, -sin_theta, cos_theta,
                        word_origin.point.x(), word_origin.point.y());
  if (word_is_rtl) {
    move_matrix.a = -move_matrix.a;
    move_matrix.b = -move_matrix.b;
    move_matrix.e += cos_theta * word_bounding_box_width;
    move_matrix.f += sin_theta * word_bounding_box_width;
  }
  return move_matrix;
}

// Returns the newly created text object, or nullptr on error.
FPDF_PAGEOBJECT AddWordOnImage(FPDF_DOCUMENT document,
                               FPDF_PAGE page,
                               FPDF_FONT font,
                               const screen_ai::mojom::WordBox& word,
                               base::span<const FS_MATRIX> transform_matrices) {
  ScopedFPDFPageObject text(
      FPDFPageObj_CreateTextObj(document, font, word.bounding_box.height()));
  CHECK(text);

  std::vector<uint32_t> charcodes = Utf8ToCharcodes(word.word);
  if (charcodes.empty()) {
    DLOG(ERROR) << "Got empty word";
    return nullptr;
  }
  bool result =
      FPDFText_SetCharcodes(text.get(), charcodes.data(), charcodes.size());
  CHECK(result);

  // Make text invisible
  result =
      FPDFTextObj_SetTextRenderMode(text.get(), FPDF_TEXTRENDERMODE_INVISIBLE);
  CHECK(result);

  const gfx::SizeF text_object_size = GetImageSize(text.get());
  CHECK_GT(text_object_size.width(), 0);
  CHECK_GT(text_object_size.height(), 0);
  const FS_MATRIX text_scale_matrix(
      word.bounding_box.width() / text_object_size.width(), 0, 0,
      word.bounding_box.height() / text_object_size.height(), 0, 0);
  CHECK(FPDFPageObj_TransformF(text.get(), &text_scale_matrix));

  for (const auto& matrix : transform_matrices) {
    FPDFPageObj_TransformF(text.get(), &matrix);
  }

  FPDF_PAGEOBJECT text_ptr = text.get();
  FPDFPage_InsertObject(page, text.release());
  return text_ptr;
}

double IsInRange(double v, double min_value, double max_value) {
  CHECK_LT(min_value, max_value);
  return v >= min_value && v <= max_value;
}

// Returns the rectangle covering the space between the bounding rectangles of
// two consecutive words.
gfx::Rect GetSpaceRect(const gfx::Rect& rect1, const gfx::Rect& rect2) {
  if (rect1.IsEmpty() || rect2.IsEmpty()) {
    return gfx::Rect();
  }

  // Return empty if the two rects intersect.
  gfx::Rect r1 = rect1;
  if (r1.InclusiveIntersect(rect2)) {
    return gfx::Rect();
  }

  // Compute the angle of text flow from `rect1` to `rect2`, to decide where the
  // space rectangle should be.
  gfx::Vector2dF vec(rect2.CenterPoint() - rect1.CenterPoint());
  double text_flow_angle = base::RadToDeg(vec.SlopeAngleRadians());

  int x;
  int y;
  int width;
  int height;

  if (IsInRange(text_flow_angle, -45, 45)) {
    // Left to Right
    x = rect1.right();
    y = std::min(rect1.y(), rect2.y());
    width = rect2.x() - x;
    height = std::max(rect1.bottom(), rect2.bottom()) - y;
  } else if (IsInRange(text_flow_angle, 45, 135)) {
    // Top to Bottom.
    x = std::min(rect1.x(), rect2.x());
    y = rect1.bottom();
    width = std::max(rect1.right(), rect2.right()) - x;
    height = rect2.y() - y;
  } else if (IsInRange(text_flow_angle, 135, 180) ||
             IsInRange(text_flow_angle, -180, -135)) {
    // Right to Left.
    x = rect2.right();
    y = std::min(rect1.y(), rect2.y());
    width = rect1.x() - x;
    height = std::max(rect1.bottom(), rect2.bottom()) - y;
  } else {
    CHECK(IsInRange(text_flow_angle, -135, -45));
    // Bottom to Top.
    x = std::min(rect1.x(), rect2.x());
    y = rect2.bottom();
    width = std::max(rect1.right(), rect2.right()) - x;
    height = rect1.y() - y;
  }

  // To avoid returning an empty rectangle, width and height are set to at least
  // one.
  width = std::max(1, width);
  height = std::max(1, height);

  return gfx::Rect(x, y, width, height);
}

// If OCR has recognized a space character between two consecutive words,
// inserts a new word between them to represent it, and returns the vector of
// words and spaces.
std::vector<screen_ai::mojom::WordBox> GetWordsAndSpaces(
    const std::vector<screen_ai::mojom::WordBoxPtr>& words) {
  std::vector<screen_ai::mojom::WordBox> words_and_spaces;

  size_t original_word_count = words.size();
  if (original_word_count) {
    words_and_spaces.reserve(original_word_count * 2 - 1);
  }

  for (size_t i = 0; i < original_word_count; i++) {
    auto& current_word = words[i];
    words_and_spaces.push_back(*current_word);
    if (current_word->has_space_after && i + 1 < original_word_count) {
      gfx::Rect space_rect =
          GetSpaceRect(current_word->bounding_box, words[i + 1]->bounding_box);
      if (!space_rect.IsEmpty()) {
        words_and_spaces.push_back(screen_ai::mojom::WordBox(
            /*word=*/" ", /*dictionary_word=*/false, current_word->language,
            /*has_space_after=*/false, space_rect,
            current_word->bounding_box_angle, current_word->direction,
            /*confidence=*/1));
      }
    }
  }

  return words_and_spaces;
}

}  // namespace

std::vector<uint8_t> PDFiumSearchify(
    base::span<const uint8_t> pdf_buffer,
    base::RepeatingCallback<screen_ai::mojom::VisualAnnotationPtr(
        const SkBitmap& bitmap)> perform_ocr_callback) {
  ScopedFPDFDocument document = LoadPdfData(pdf_buffer);
  if (!document) {
    DLOG(ERROR) << "Failed to load document";
    return {};
  }
  int page_count = FPDF_GetPageCount(document.get());
  if (page_count == 0) {
    DLOG(ERROR) << "Got zero page count";
    return {};
  }
  ScopedFPDFFont font = CreateFont(document.get());
  CHECK(font);
  for (int page_index = 0; page_index < page_count; page_index++) {
    ScopedFPDFPage page(FPDF_LoadPage(document.get(), page_index));
    if (!page) {
      DLOG(ERROR) << "Failed to load page";
      continue;
    }
    int object_count = FPDFPage_CountObjects(page.get());
    for (int object_index = 0; object_index < object_count; object_index++) {
      // GetImageForOcr() checks for null `image`.
      FPDF_PAGEOBJECT image = FPDFPage_GetObject(page.get(), object_index);
      SkBitmap bitmap = GetImageForOcr(document.get(), page.get(), image);
      // The object is not an image or failed to get the bitmap from the image.
      if (bitmap.empty()) {
        continue;
      }
      auto annotation = perform_ocr_callback.Run(bitmap);
      if (!annotation) {
        DLOG(ERROR) << "Failed to get OCR annotation on the image";
        return {};
      }
      AddTextOnImage(document.get(), page.get(), font.get(), image,
                     std::move(annotation),
                     gfx::Size(bitmap.width(), bitmap.height()));
    }
    if (!FPDFPage_GenerateContent(page.get())) {
      DLOG(ERROR) << "Failed to generate content";
      return {};
    }
  }
  PDFiumMemBufferFileWrite output_file_write;
  if (!FPDF_SaveAsCopy(document.get(), &output_file_write, 0)) {
    DLOG(ERROR) << "Failed to save the document";
    return {};
  }
  return output_file_write.TakeBuffer();
}

std::vector<FPDF_PAGEOBJECT> AddTextOnImage(
    FPDF_DOCUMENT document,
    FPDF_PAGE page,
    FPDF_FONT font,
    FPDF_PAGEOBJECT image,
    screen_ai::mojom::VisualAnnotationPtr annotation,
    const gfx::Size& image_pixel_size) {
  const gfx::SizeF image_rendered_size = GetRenderedImageSize(image);
  if (image_rendered_size.IsEmpty()) {
    DLOG(ERROR) << "Failed to get image rendered dimensions";
    return {};
  }

  // The transformation matrices is applied as follows:
  std::array<FS_MATRIX, 3> transform_matrices;
  // Move text object to the corresponding text position on the full image.
  FS_MATRIX& move_matrix = transform_matrices[0];
  // Scale from full image size to rendered image size on the PDF.
  FS_MATRIX& image_scale_matrix = transform_matrices[1];
  // Apply the image's transformation matrix on the PDF page without the
  // scaling matrix.
  FS_MATRIX& image_without_scaling_matrix = transform_matrices[2];

  image_scale_matrix = {
      image_rendered_size.width() / image_pixel_size.width(),   0, 0,
      image_rendered_size.height() / image_pixel_size.height(), 0, 0};
  if (!CalculateImageWithoutScalingMatrix(image, image_rendered_size,
                                          image_without_scaling_matrix)) {
    DLOG(ERROR) << "Failed to get image matrix";
    return {};
  }

  size_t estimated_word_count = 0;
  for (const auto& line : annotation->lines) {
    // Assume there are spaces between each two words.
    if (line->words.size()) {
      estimated_word_count += line->words.size() * 2 - 1;
    }
  }
  std::vector<FPDF_PAGEOBJECT> added_text_objects;
  added_text_objects.reserve(estimated_word_count);

  for (const auto& line : annotation->lines) {
    SearchifyBoundingBoxOrigin baseline_origin =
        ConvertToPdfOrigin(line->baseline_box, line->baseline_box_angle,
                           image_pixel_size.height());

    std::vector<screen_ai::mojom::WordBox> words_and_spaces =
        GetWordsAndSpaces(line->words);

    for (const auto& word : words_and_spaces) {
      if (word.bounding_box.IsEmpty()) {
        continue;
      }

      SearchifyBoundingBoxOrigin origin =
          ConvertToPdfOrigin(word.bounding_box, word.bounding_box_angle,
                             image_pixel_size.height());
      move_matrix = CalculateWordMoveMatrix(
          ProjectToBaseline(origin.point, baseline_origin),
          word.bounding_box.width(),
          word.direction ==
              screen_ai::mojom::Direction::DIRECTION_RIGHT_TO_LEFT);
      added_text_objects.push_back(
          AddWordOnImage(document, page, font, word, transform_matrices));
    }
  }
  return added_text_objects;
}

SearchifyBoundingBoxOrigin ConvertToPdfOriginForTesting(
    const gfx::Rect& rect,
    float angle,
    float coordinate_system_height) {
  return ConvertToPdfOrigin(rect, angle, coordinate_system_height);
}

FS_MATRIX CalculateWordMoveMatrixForTesting(
    const SearchifyBoundingBoxOrigin& origin,
    int word_bounding_box_width,
    bool word_is_rtl) {
  return CalculateWordMoveMatrix(origin, word_bounding_box_width, word_is_rtl);
}

gfx::Rect GetSpaceRectForTesting(const gfx::Rect& rect1,  // IN-TEST
                                 const gfx::Rect& rect2) {
  return GetSpaceRect(rect1, rect2);
}

std::vector<screen_ai::mojom::WordBox> GetWordsAndSpacesForTesting(  // IN-TEST
    const std::vector<screen_ai::mojom::WordBoxPtr>& words) {
  return GetWordsAndSpaces(words);
}

ScopedFPDFFont CreateFont(FPDF_DOCUMENT document) {
  std::vector<uint8_t> cid_to_gid_map(CreateCidToGidMap());
  return ScopedFPDFFont(
      FPDFText_LoadCidType2Font(document, kPdfTtf, kPdfTtfSize, kToUnicodeCMap,
                                cid_to_gid_map.data(), cid_to_gid_map.size()));
}

}  // namespace chrome_pdf
