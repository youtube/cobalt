// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/passage_embeddings/cpu_histogram_logger.h"
#include "components/passage_embeddings/core/passage_embeddings_service_controller.h"
#include "components/passage_embeddings/core/passage_embeddings_service_launcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class Process;
}  // namespace base

namespace passage_embeddings {

// Launches the passage embeddings service and manages its lifetime.
class ChromePassageEmbeddingsServiceLauncher
    : public PassageEmbeddingsServiceLauncher {
 public:
  ChromePassageEmbeddingsServiceLauncher();
  ~ChromePassageEmbeddingsServiceLauncher() override;

  PassageEmbeddingsServiceController* controller() { return &controller_; }

  // PassageEmbeddingsServiceLauncher:
  void LaunchService(
      mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver) override;
  void OnServiceDisconnected(bool is_idle) override;
  bool AllowedToLaunch() const override;

 private:
  enum class ServiceState {
    kIdle,
    kLaunching,
    kReady,
  };

  // Initializes `cpu_logger_`; can only be called when the service process is
  // launched and connected.
  void InitializeCpuLogger();

  void OnServiceLaunched(const base::Process& process);

  ServiceState service_state_ = ServiceState::kIdle;

  // Periodically samples and logs the CPU time used by the service process.
  CpuHistogramLogger cpu_logger_;

  PassageEmbeddingsServiceController controller_{*this};

  base::WeakPtrFactory<ChromePassageEmbeddingsServiceLauncher>
      weak_ptr_factory_{this};
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_
