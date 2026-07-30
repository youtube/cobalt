// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_

namespace passage_embeddings {

class PassageEmbeddingsServiceController;

// Returns the PassageEmbeddingsServiceController singleton for use within
// chrome.
PassageEmbeddingsServiceController*
GetChromePassageEmbeddingsServiceController();

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_CHROME_PASSAGE_EMBEDDINGS_SERVICE_CONTROLLER_H_
