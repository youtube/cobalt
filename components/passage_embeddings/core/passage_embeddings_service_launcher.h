// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_
#define COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace passage_embeddings {

// Interface for launching the passage embeddings service.
class PassageEmbeddingsServiceLauncher {
 public:
  PassageEmbeddingsServiceLauncher() = default;
  virtual ~PassageEmbeddingsServiceLauncher() = default;

  PassageEmbeddingsServiceLauncher(const PassageEmbeddingsServiceLauncher&) =
      delete;
  PassageEmbeddingsServiceLauncher& operator=(
      const PassageEmbeddingsServiceLauncher&) = delete;

  // Launches the passage embeddings service.
  virtual void LaunchService(
      mojo::PendingReceiver<mojom::PassageEmbeddingsService> receiver) = 0;

  // Called when the passage embeddings service is disconnected.
  // `is_idle` is true if the disconnection was caused by an idle timeout.
  virtual void OnServiceDisconnected(bool is_idle) = 0;

  // Returns true if the passage embeddings service is currently allowed to
  // launch the service.
  virtual bool AllowedToLaunch() const = 0;
};

}  // namespace passage_embeddings

#endif  // COMPONENTS_PASSAGE_EMBEDDINGS_CORE_PASSAGE_EMBEDDINGS_SERVICE_LAUNCHER_H_
