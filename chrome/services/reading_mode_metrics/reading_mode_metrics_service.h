// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_READING_MODE_METRICS_READING_MODE_METRICS_SERVICE_H_
#define CHROME_SERVICES_READING_MODE_METRICS_READING_MODE_METRICS_SERVICE_H_

#include "chrome/common/read_anything/distillation_evaluator.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ui {
class AXNode;
}  // namespace ui

namespace reading_mode {

struct OriginalStructure;

// Exposes the structural extraction helper solely for unit testing.
void ExtractOriginalStructureForTesting(ui::AXNode* node,
                                        OriginalStructure& structure,
                                        bool inside_code);

class ReadingModeMetricsService : public mojom::DistillationEvaluator {
 public:
  explicit ReadingModeMetricsService(
      mojo::PendingReceiver<mojom::DistillationEvaluator> receiver);
  ~ReadingModeMetricsService() override;

  ReadingModeMetricsService(const ReadingModeMetricsService&) = delete;
  ReadingModeMetricsService& operator=(const ReadingModeMetricsService&) =
      delete;

  // mojom::DistillationEvaluator:
  void Evaluate(const ::ui::AXTreeUpdate& ax_tree_update,
                const std::string& distilled_html,
                EvaluateCallback callback) override;

 private:
  mojo::Receiver<mojom::DistillationEvaluator> receiver_;
};

}  // namespace reading_mode

#endif  // CHROME_SERVICES_READING_MODE_METRICS_READING_MODE_METRICS_SERVICE_H_
