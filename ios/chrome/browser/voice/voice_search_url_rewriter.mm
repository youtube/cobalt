// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_url_rewriter.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/voice/speech_input_locale_config.h"
#import "ios/chrome/browser/voice/voice_search_prefs.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool VoiceSearchURLRewriter(GURL* url, web::BrowserState* browser_state) {
  if (!google_util::IsGoogleSearchUrl(*url))
    return false;

  ChromeBrowserState* chrome_browser_state =
      ChromeBrowserState::FromBrowserState(browser_state);
  std::string language =
      chrome_browser_state->GetPrefs()->GetString(prefs::kVoiceSearchLocale);
  GURL rewritten_url(*url);
  // The `hl` parameter will be overriden only if the voice search locale
  // is not empty. If it is empty (indicating that voice search locale
  // uses device language), the `hl` will keep the original value.
  // If there is no `hl` in the query the `spknlang` will use the application
  // locale as a fallback (instead of using the same locale for both `hl`
  // and `spknlang`).
  if (language.empty()) {
    voice::SpeechInputLocaleConfig* locale_config =
        voice::SpeechInputLocaleConfig::GetInstance();
    if (locale_config)
      language = locale_config->GetDefaultLocale().code;
    if (!language.length()) {
      NOTREACHED();
      language = "en-US";
    }
  }
  rewritten_url =
      net::AppendOrReplaceQueryParameter(rewritten_url, "hl", language);
  rewritten_url =
      net::AppendQueryParameter(rewritten_url, "spknlang", language);
  rewritten_url = net::AppendQueryParameter(rewritten_url, "inm", "vs");
  rewritten_url = net::AppendQueryParameter(rewritten_url, "vse", "1");
  *url = rewritten_url;

  // Return false so other URLRewriters can update the url if necessary.
  return false;
}
