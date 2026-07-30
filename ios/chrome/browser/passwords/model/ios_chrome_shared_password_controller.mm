// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/ios_chrome_shared_password_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/passwords/password_suggestion/ui/password_suggestion_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"
#import "url/origin.h"

@implementation IOSChromeSharedPasswordController

#pragma mark - FormSuggestionProvider

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  NSString* pageHost = nil;
  NSString* credentialType = nil;
  if (IsConditionalPasskeyLoginEnabled()) {
    url::Origin pageOrigin =
        url::Origin::Create(webState->GetLastCommittedURL());
    pageHost =
        base::SysUTF8ToNSString(password_manager::GetShownOrigin(pageOrigin));
    credentialType = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUBTEXT);
  }

  SuggestionsReadyCompletion updatedCompletion = ^(
      NSArray<FormSuggestion*>* suggestions,
      id<FormSuggestionProvider> delegate) {
    NSMutableArray* suggestionsCopy =
        [NSMutableArray arrayWithCapacity:suggestions.count];

    for (FormSuggestion* suggestion : suggestions) {
      UIImage* icon = suggestion.icon;
      NSString* description = suggestion.displayDescription;
      SuggestionIconType suggestionIconType = suggestion.suggestionIconType;
      BOOL needsModification = NO;

      if (suggestion.type == autofill::SuggestionType::kBackupPasswordEntry) {
        icon = GetBackupPasswordSuggestionIcon();
        suggestionIconType = SuggestionIconType::kBackupPassword;
        needsModification = YES;
      }

      if (IsConditionalPasskeyLoginEnabled() &&
          (suggestion.type == autofill::SuggestionType::kPasswordEntry ||
           suggestion.type == autofill::SuggestionType::kBackupPasswordEntry)) {
        NSString* host = suggestion.displayDescription;
        if (host.length > 0 && pageHost.length > 0 &&
            ![host isEqualToString:pageHost]) {
          description =
              [NSString stringWithFormat:@"%@ • %@", credentialType, host];
        } else {
          description = credentialType;
        }
        needsModification = YES;
      }

      if (needsModification) {
        FormSuggestion* modifiedSuggestion = [FormSuggestion
                   suggestionWithValue:suggestion.value
                    displayDescription:description
                                  icon:icon
                                  type:suggestion.type
                               payload:suggestion.payload
                        requiresReauth:suggestion.requiresReauth
            acceptanceA11yAnnouncement:suggestion.acceptanceA11yAnnouncement
                              metadata:suggestion.metadata];
        modifiedSuggestion.suggestionIconType = suggestionIconType;
        [suggestionsCopy addObject:modifiedSuggestion];
      } else {
        [suggestionsCopy addObject:suggestion];
      }
    }

    completion(suggestionsCopy, delegate);
  };

  [super retrieveSuggestionsForForm:formQuery
                           webState:webState
                  completionHandler:updatedCompletion];
}

@end
