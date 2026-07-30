// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_
#define CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/task_tracker.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

class Profile;

namespace readaloud {

// Central lifecycle and state orchestrator for Read Aloud.
class ReadAloudService : public KeyedService,
                         public dom_distiller::ViewRequestDelegate {
 public:
  explicit ReadAloudService(Profile* profile);

  ReadAloudService(const ReadAloudService&) = delete;
  ReadAloudService& operator=(const ReadAloudService&) = delete;

  ~ReadAloudService() override;

  // KeyedService:
  void Shutdown() override;

  // Triggers distillation of a webpage using DomDistillerService.
  void DistillPage(content::WebContents* web_contents);

  // dom_distiller::ViewRequestDelegate:
  void OnArticleReady(
      const dom_distiller::DistilledArticleProto* article_proto) override;
  void OnArticleUpdated(
      dom_distiller::ArticleDistillationUpdate article_update) override;

  dom_distiller::ViewerHandle* GetViewerHandleForTesting() const {
    return viewer_handle_.get();
  }

 private:
  raw_ptr<Profile> profile_;
  std::unique_ptr<dom_distiller::ViewerHandle> viewer_handle_;

  base::WeakPtrFactory<ReadAloudService> weak_factory_{this};
};

}  // namespace readaloud

#endif  // CHROME_BROWSER_READALOUD_READ_ALOUD_SERVICE_H_
