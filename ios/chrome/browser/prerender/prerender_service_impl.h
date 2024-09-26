// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_IMPL_H_

#import "ios/chrome/browser/prerender/prerender_service.h"

#import <Foundation/Foundation.h>

class ChromeBrowserState;

// Implementation of PrerenderService.
class PrerenderServiceImpl : public PrerenderService {
 public:
  // TODO(crbug.com/754050): Convert this constructor to take lower-level
  // objects instead of the entire ChromeBrowserState.  This will make unit
  // testing much simpler.
  PrerenderServiceImpl(ChromeBrowserState* browser_state);
  ~PrerenderServiceImpl() override;

 private:
  // PrerenderService:
  void SetDelegate(id<PreloadControllerDelegate> delegate) override;
  void StartPrerender(const GURL& url,
                      const web::Referrer& referrer,
                      ui::PageTransition transition,
                      web::WebState* web_state_to_replace,
                      bool immediately) override;
  bool MaybeLoadPrerenderedURL(const GURL& url,
                               ui::PageTransition transition,
                               Browser* browser) override;
  bool IsLoadingPrerender() override;
  void CancelPrerender() override;
  bool HasPrerenderForUrl(const GURL& url) override;
  bool IsWebStatePrerendered(web::WebState* web_state) override;

  // KeyedService:
  void Shutdown() override;

  PreloadController* controller_ = nil;
  bool loading_prerender_ = false;
};

#endif  // IOS_CHROME_BROWSER_PRERENDER_PRERENDER_SERVICE_IMPL_H_
