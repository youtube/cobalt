// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/readaloud/read_aloud_service.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/dom_distiller/content/browser/distiller_page_web_contents.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "content/public/browser/web_contents.h"

namespace readaloud {

ReadAloudService::ReadAloudService(Profile* profile) : profile_(profile) {}

ReadAloudService::~ReadAloudService() = default;

void ReadAloudService::SetDelegate(std::unique_ptr<Delegate> delegate) {
  delegate_ = std::move(delegate);
}

void ReadAloudService::Play() {}
void ReadAloudService::Pause() {}
void ReadAloudService::Stop() {}
void ReadAloudService::SeekToWordIndex(int word_index) {}
void ReadAloudService::Seek(base::TimeDelta absolute_time) {}
void ReadAloudService::SeekRelative(base::TimeDelta offset) {}
void ReadAloudService::SetPlaybackRate(float rate) {}
void ReadAloudService::SetVoice(std::string_view voice_id) {}
void ReadAloudService::PreviewVoice(std::string_view voice_id) {}
void ReadAloudService::StopVoicePreview() {}
void ReadAloudService::SetPlaybackMode(PlaybackMode mode) {}
void ReadAloudService::SetHighlightingEnabled(bool enabled) {}
void ReadAloudService::SendFeedback(FeedbackType feedback_type) {}
void ReadAloudService::CheckReadability(const GURL& url) {}

void ReadAloudService::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();
  if (delegate_) {
    delegate_->OnNativeDestroyed();
    delegate_.reset();
  }
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

  distillation_start_time_ = base::TimeTicks::Now();

  viewer_handle_ = service->ViewUrlIgnoreCache(
      this,
      service->CreateDefaultDistillerPageWithHandle(
          std::make_unique<dom_distiller::SourcePageHandleWebContents>(
              web_contents, /*owned=*/false)),
      web_contents->GetLastCommittedURL());
}

void ReadAloudService::OnArticleReady(
    const dom_distiller::DistilledArticleProto* article_proto) {
  if (!distillation_start_time_.is_null()) {
    bool success = article_proto && !article_proto->pages().empty();
    base::UmaHistogramTimes("ReadAloud.Distillation.Duration",
                            base::TimeTicks::Now() - distillation_start_time_);
    base::UmaHistogramBoolean("ReadAloud.Distillation.Success", success);
    distillation_start_time_ = base::TimeTicks();
  }
  viewer_handle_.reset();
}

void ReadAloudService::OnArticleUpdated(
    dom_distiller::ArticleDistillationUpdate article_update) {}

}  // namespace readaloud
