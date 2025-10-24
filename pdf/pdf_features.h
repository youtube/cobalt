// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the pdf
// module.

#ifndef PDF_PDF_FEATURES_H_
#define PDF_PDF_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "pdf/buildflags.h"

static_assert(BUILDFLAG(ENABLE_PDF), "ENABLE_PDF not set to true");

namespace chrome_pdf::features {

BASE_DECLARE_FEATURE(kAccessiblePDFForm);
BASE_DECLARE_FEATURE(kPdfGetSaveDataInBlocks);
BASE_DECLARE_FEATURE(kPdfIncrementalLoading);
BASE_DECLARE_FEATURE(kPdfOopif);
BASE_DECLARE_FEATURE(kPdfPartialLoading);
BASE_DECLARE_FEATURE(kPdfPortfolio);
BASE_DECLARE_FEATURE(kPdfSaveOriginalFromMemory);
BASE_DECLARE_FEATURE(kPdfSearchify);
BASE_DECLARE_FEATURE(kPdfSearchifySave);
BASE_DECLARE_FEATURE(kPdfTags);
BASE_DECLARE_FEATURE(kPdfUseShowSaveFilePicker);
BASE_DECLARE_FEATURE(kPdfUseSkiaRenderer);
BASE_DECLARE_FEATURE(kPdfXfaSupport);

#if BUILDFLAG(ENABLE_PDF_INK2)
BASE_DECLARE_FEATURE(kPdfInk2);
extern const base::FeatureParam<bool> kPdfInk2TextAnnotations;
extern const base::FeatureParam<bool> kPdfInk2TextHighlighting;
#endif

// Sets whether the OOPIF PDF policy enables the OOPIF PDF viewer. Otherwise,
// GuestView PDF viewer will be used. The policy is enabled by default.
void SetIsOopifPdfPolicyEnabled(bool is_oopif_pdf_policy_enabled);

// Returns whether the OOPIF PDF viewer should be used, otherwise the GuestView
// PDF viewer should be used.
bool IsOopifPdfEnabled();

// Returns whether PDF Searchify and PDF Searchify Save features are enabled.
bool IsPdfSearchifySaveEnabled();

}  // namespace chrome_pdf::features

#endif  // PDF_PDF_FEATURES_H_
