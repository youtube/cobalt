// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"

#include "base/no_destructor.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_launcher.h"

namespace passage_embeddings {

PassageEmbeddingsServiceController*
GetChromePassageEmbeddingsServiceController() {
  static base::NoDestructor<ChromePassageEmbeddingsServiceLauncher> instance;
  return instance->controller();
}

}  // namespace passage_embeddings
