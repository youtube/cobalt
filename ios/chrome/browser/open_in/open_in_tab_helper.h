// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_

#include <string>

#import "ios/chrome/browser/open_in/open_in_tab_helper_delegate.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace net {
class HttpResponseHeaders;
}  //  namespace net

namespace content_type {

// .pptx extension.
extern const char kMimeTypeMicrosoftPowerPointOpenXML[];

// .docx extension.
extern const char kMimeTypeMicrosoftWordOpenXML[];

// .xlsx extension.
extern const char kMimeTypeMicrosoftExcelOpenXML[];

// .pdf extension.
extern const char kMimeTypePDF[];

// .doc extension.
extern const char kMimeTypeMicrosoftWord[];

// .jpeg or .jpg extension.
extern const char kMimeTypeJPEG[];

// .png extension.
extern const char kMimeTypePNG[];

// .ppt extension.
extern const char kMimeTypeMicrosoftPowerPoint[];

// .rtf extension.
extern const char kMimeTypeRTF[];

// .svg extension.
extern const char kMimeTypeSVG[];

// .xls extension.
extern const char kMimeTypeMicrosoftExcel[];

}  // namespace content_type

// Enum used to determine the MIME type of a previewed file. Entries should
// always keep synced with the IOS.OpenIn.MimeType UMA histogram.
enum class OpenInMimeType {
  kMimeTypeNotHandled = 0,
  kMimeTypePDF = 1,
  kMimeTypeMicrosoftWord = 2,
  kMimeTypeMicrosoftWordOpenXML = 3,
  kMimeTypeJPEG = 4,
  kMimeTypePNG = 5,
  kMimeTypeMicrosoftPowerPoint = 6,
  kMimeTypeMicrosoftPowerPointOpenXML = 7,
  kMimeTypeRTF = 8,
  kMimeTypeSVG = 9,
  kMimeTypeMicrosoftExcel = 10,
  kMimeTypeMicrosoftExcelOpenXML = 11,
  kMaxValue = kMimeTypeMicrosoftExcelOpenXML,
};

@class OpenInController;

// A tab helper that observes WebState and shows open in button for PDF
// documents.
class OpenInTabHelper : public web::WebStateObserver,
                        public web::WebStateUserData<OpenInTabHelper> {
 public:
  OpenInTabHelper(const OpenInTabHelper&) = delete;
  OpenInTabHelper& operator=(const OpenInTabHelper&) = delete;

  ~OpenInTabHelper() override;

  // Sets the OpenInTabHelper delegate. `delegate` will be in charge of enabling
  // the openIn view. `delegate` is not retained by TabHelper.
  void SetDelegate(id<OpenInTabHelperDelegate> delegate);

  // Returns true if the displayed content should be downloaded.
  static bool ShouldDownload(web::WebState* web_state);

  // Returns the suggested file name of the displayed content.
  std::u16string GetFileNameSuggestion();

 private:
  friend class web::WebStateUserData<OpenInTabHelper>;

  explicit OpenInTabHelper(web::WebState* web_state);

  // Handles exportable files and shows open in button if content mime type is
  // PDF.
  void HandleExportableFile();

  // Tests that files are exportable and returns their MIME type.
  OpenInMimeType GetUmaResult(const std::string& mime_type) const;

  // WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;
  void DidFinishNavigation(web::WebState* web_state,
                           web::NavigationContext* navigation_context) override;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  web::WebState* web_state_ = nullptr;

  // Headers of the last response received for the current navigation.
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  // Used to enable/disable openIn UI.
  __weak id<OpenInTabHelperDelegate> delegate_ = nil;

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_OPEN_IN_OPEN_IN_TAB_HELPER_H_
