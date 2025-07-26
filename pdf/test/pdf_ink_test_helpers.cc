// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/test/pdf_ink_test_helpers.h"

#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "base/values.h"
#include "pdf/pdf_ink_conversions.h"

namespace chrome_pdf {

std::optional<ink::StrokeInputBatch> CreateInkInputBatch(
    base::span<const PdfInkInputData> inputs) {
  ink::StrokeInputBatch input_batch;
  for (const auto& input : inputs) {
    auto result = input_batch.Append(CreateInkStrokeInput(
        ink::StrokeInput::ToolType::kMouse, input.position, input.time));
    if (!result.ok()) {
      return std::nullopt;
    }
  }
  return input_batch;
}

base::Value::Dict CreateSetAnnotationBrushMessageForTesting(
    std::string_view type,
    double size,
    const TestAnnotationBrushMessageParams* params) {
  base::Value::Dict message;
  message.Set("type", "setAnnotationBrush");

  base::Value::Dict data;
  data.Set("type", type);
  data.Set("size", size);
  if (params) {
    base::Value::Dict color;
    color.Set("r", params->color_r);
    color.Set("g", params->color_g);
    color.Set("b", params->color_b);
    data.Set("color", std::move(color));
  }
  message.Set("data", std::move(data));
  return message;
}

base::Value::Dict CreateSetAnnotationModeMessageForTesting(bool enable) {
  base::Value::Dict message;
  message.Set("type", "setAnnotationMode");
  message.Set("enable", enable);
  return message;
}

base::Value::Dict CreateSetAnnotationUndoRedoMessageForTesting(
    TestAnnotationUndoRedoMessageType type) {
  base::Value::Dict message;
  switch (type) {
    case TestAnnotationUndoRedoMessageType::kUndo:
      message.Set("type", "annotationUndo");
      return message;
    case TestAnnotationUndoRedoMessageType::kRedo:
      message.Set("type", "annotationRedo");
      return message;
  }
  NOTREACHED();
}

base::FilePath GetInkTestDataFilePath(std::string_view filename) {
  return base::FilePath(FILE_PATH_LITERAL("ink")).AppendASCII(filename);
}

}  // namespace chrome_pdf
