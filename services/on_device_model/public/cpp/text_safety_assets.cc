// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/text_safety_assets.h"

#include "base/files/file.h"
#include "services/on_device_model/public/mojom/on_device_model_service.mojom.h"

namespace on_device_model {

TextSafetyAssetPaths::TextSafetyAssetPaths() = default;
TextSafetyAssetPaths::TextSafetyAssetPaths(const TextSafetyAssetPaths&) =
    default;
TextSafetyAssetPaths::~TextSafetyAssetPaths() = default;

LanguageDetectionAssetPaths::LanguageDetectionAssetPaths() = default;
LanguageDetectionAssetPaths::LanguageDetectionAssetPaths(
    const LanguageDetectionAssetPaths&) = default;
LanguageDetectionAssetPaths::~LanguageDetectionAssetPaths() = default;

TextSafetyLoaderParams::TextSafetyLoaderParams() = default;
TextSafetyLoaderParams::TextSafetyLoaderParams(const TextSafetyLoaderParams&) =
    default;
TextSafetyLoaderParams::~TextSafetyLoaderParams() = default;

COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP)
mojom::TextSafetyModelParamsPtr LoadTextSafetyParams(
    TextSafetyLoaderParams params) {
  auto result = mojom::TextSafetyModelParams::New();
  if (params.language_paths) {
    result->language_model =
        base::File(params.language_paths->model,
                   base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  if (params.ts_paths) {
    result->safety_model = base::File(
        params.ts_paths->model, base::File::FLAG_OPEN | base::File::FLAG_READ);
  }
  return result;
}

}  // namespace on_device_model
