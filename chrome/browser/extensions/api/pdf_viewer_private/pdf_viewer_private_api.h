// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class PdfViewerPrivateIsAllowedLocalFileAccessFunction
    : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pdfViewerPrivate.isAllowedLocalFileAccess",
                             PDFVIEWERPRIVATE_ISALLOWEDLOCALFILEACCESS)

  PdfViewerPrivateIsAllowedLocalFileAccessFunction();
  PdfViewerPrivateIsAllowedLocalFileAccessFunction(
      const PdfViewerPrivateIsAllowedLocalFileAccessFunction&) = delete;
  PdfViewerPrivateIsAllowedLocalFileAccessFunction& operator=(
      const PdfViewerPrivateIsAllowedLocalFileAccessFunction&) = delete;

 protected:
  ~PdfViewerPrivateIsAllowedLocalFileAccessFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

class PdfViewerPrivateIsPdfOcrAlwaysActiveFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pdfViewerPrivate.isPdfOcrAlwaysActive",
                             PDFVIEWERPRIVATE_ISPDFOCRALWAYSACTIVE)

  PdfViewerPrivateIsPdfOcrAlwaysActiveFunction();
  PdfViewerPrivateIsPdfOcrAlwaysActiveFunction(
      const PdfViewerPrivateIsPdfOcrAlwaysActiveFunction&) = delete;
  PdfViewerPrivateIsPdfOcrAlwaysActiveFunction& operator=(
      const PdfViewerPrivateIsPdfOcrAlwaysActiveFunction&) = delete;

 protected:
  ~PdfViewerPrivateIsPdfOcrAlwaysActiveFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

class PdfViewerPrivateSetPdfOcrPrefFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pdfViewerPrivate.setPdfOcrPref",
                             PDFVIEWERPRIVATE_SETPDFOCRPREF)

  PdfViewerPrivateSetPdfOcrPrefFunction();
  PdfViewerPrivateSetPdfOcrPrefFunction(
      const PdfViewerPrivateSetPdfOcrPrefFunction&) = delete;
  PdfViewerPrivateSetPdfOcrPrefFunction& operator=(
      const PdfViewerPrivateSetPdfOcrPrefFunction&) = delete;

 protected:
  ~PdfViewerPrivateSetPdfOcrPrefFunction() override;

  // Override from ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PDF_VIEWER_PRIVATE_PDF_VIEWER_PRIVATE_API_H_
