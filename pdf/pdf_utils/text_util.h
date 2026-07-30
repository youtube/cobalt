// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDF_UTILS_TEXT_UTIL_H_
#define PDF_PDF_UTILS_TEXT_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace chrome_pdf {

// Returns whether `ch` is a word boundary.
bool IsWordBoundary(uint32_t ch);

// Convert a UTF-16 string to a blob for use in FPDFPageObjMark_SetBlobParam().
// That function takes unsigned char. Also for that purpose this function adds
// a BOM to the beginning.
std::vector<unsigned char> ToUTF16BEBlob(const std::u16string& str);

}  // namespace chrome_pdf

#endif  // PDF_PDF_UTILS_TEXT_UTIL_H_
