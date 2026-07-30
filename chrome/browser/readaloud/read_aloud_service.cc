// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/web_contents.h"

namespace readaloud {

ReadAloudService::ReadAloudService(Profile* profile) : profile_(profile) {}

ReadAloudService::~ReadAloudService() = default;

void ReadAloudService::Shutdown() {
  viewer_handle_.reset();
}

void ReadAloudService::DistillPage(content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }

  dom_distiller::DomDistillerService* service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  if (!service) {
    return;
  }

  viewer_handle_ = service->ViewUrlIgnoreCache(

      this,
      service->CreateDefaultDistillerPageWithHandle(
          std::make_unique<dom_distiller::SourcePageHandleWebContents>(
              web_contents, /*owned=*/false)),
      web_contents->GetLastCommittedURL());
}

void ReadAloudService::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  viewer_handle_.reset();
}

void ReadAloudService::OnArticleUpdated(
    dom_distiller::ArticleDistillationUpdate article_update) {}

}  // namespace readaloud
