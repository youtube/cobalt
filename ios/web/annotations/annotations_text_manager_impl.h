// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_IMPL_H_
#define IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_IMPL_H_

#import <UIKit/UIKit.h>

#import "base/observer_list.h"
#import "base/values.h"
#import "ios/web/public/annotations/annotations_text_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol CRWWebViewHandlerDelegate;

namespace web {
class AnnotationsTextObserver;
class WebState;

/**
 * Class in charge of annotations in text.
 */
class AnnotationsTextManagerImpl : public AnnotationsTextManager,
                                   public WebStateObserver {
 public:
  explicit AnnotationsTextManagerImpl(WebState* web_state);
  ~AnnotationsTextManagerImpl() override;

  void AddObserver(AnnotationsTextObserver* observer) override;
  void RemoveObserver(AnnotationsTextObserver* observer) override;
  void DecorateAnnotations(WebState* web_state,
                           base::Value& annotations,
                           int seq_id) override;
  void RemoveDecorations() override;
  void RemoveHighlight() override;

  // JS callback methods.
  void OnTextExtracted(WebState* web_state,
                       const std::string& text,
                       int seq_id);
  void OnDecorated(WebState* web_state, int successes, int annotations);
  void OnClick(WebState* web_state,
               const std::string& text,
               CGRect rect,
               const std::string& data);

  // WebStateObserver methods:
  void PageLoaded(WebState* web_state,
                  PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(WebState* web_state) override;

 private:
  friend class WebStateUserData<AnnotationsTextManagerImpl>;

  void StartExtractingText();

  // A list of observers. Weak references.
  base::ObserverList<AnnotationsTextObserver, true> observers_;

  WebState* web_state_ = nullptr;
  // Id passed on to some callbacks and checked on followup calls to make
  // sure it matches with current manager's state.
  int seq_id_;
};

}  // namespace web

#endif  // IOS_WEB_ANNOTATIONS_ANNOTATIONS_TEXT_MANAGER_IMPL_H_
