// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/form_suggestion_provider.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/affiliation/mock_affiliation_service.h"
#import "components/password_manager/core/browser/password_manager_test_utils.h"
#import "components/password_manager/core/browser/test_password_store.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/favicon/favicon_service_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Expose the internal disconnect function for testing purposes
@interface PasswordSuggestionBottomSheetMediator ()

- (void)disconnect;

@end

// Test provider that records invocations of its interface methods.
@interface PasswordSuggestionBottomSheetMediatorTestSuggestionProvider
    : NSObject <FormSuggestionProvider>

@property(weak, nonatomic, readonly) FormSuggestion* suggestion;
@property(weak, nonatomic, readonly) NSString* formName;
@property(weak, nonatomic, readonly) NSString* fieldIdentifier;
@property(weak, nonatomic, readonly) NSString* frameID;
@property(nonatomic, assign) BOOL selected;
@property(nonatomic, assign) BOOL askedIfSuggestionsAvailable;
@property(nonatomic, assign) BOOL askedForSuggestions;
@property(nonatomic, assign) SuggestionProviderType type;
@property(nonatomic, readonly) autofill::PopupType suggestionType;

// Creates a test provider with default suggesstions.
+ (instancetype)providerWithSuggestions;

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions;

@end

@implementation PasswordSuggestionBottomSheetMediatorTestSuggestionProvider {
  NSArray<FormSuggestion*>* _suggestions;
  NSString* _formName;
  autofill::FormRendererId _uniqueFormID;
  NSString* _fieldIdentifier;
  autofill::FieldRendererId _uniqueFieldID;
  NSString* _frameID;
  FormSuggestion* _suggestion;
}

@synthesize selected = _selected;
@synthesize askedIfSuggestionsAvailable = _askedIfSuggestionsAvailable;
@synthesize askedForSuggestions = _askedForSuggestions;

+ (instancetype)providerWithSuggestions {
  NSArray<FormSuggestion*>* suggestions = @[
    [FormSuggestion suggestionWithValue:@"foo"
                     displayDescription:nil
                                   icon:@""
                             identifier:0
                         requiresReauth:NO],
    [FormSuggestion suggestionWithValue:@"bar"
                     displayDescription:nil
                                   icon:@""
                             identifier:1
                         requiresReauth:NO]
  ];
  return [[PasswordSuggestionBottomSheetMediatorTestSuggestionProvider alloc]
      initWithSuggestions:suggestions];
}

- (instancetype)initWithSuggestions:(NSArray<FormSuggestion*>*)suggestions {
  self = [super init];
  if (self) {
    _suggestions = [suggestions copy];
    _type = SuggestionProviderTypeUnknown;
  }
  return self;
}

- (NSString*)formName {
  return _formName;
}

- (NSString*)fieldIdentifier {
  return _fieldIdentifier;
}

- (NSString*)frameID {
  return _frameID;
}

- (FormSuggestion*)suggestion {
  return _suggestion;
}

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  self.askedIfSuggestionsAvailable = YES;
  completion([_suggestions count] > 0);
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  self.askedForSuggestions = YES;
  completion(_suggestions, self);
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
               uniqueFormID:(autofill::FormRendererId)uniqueFormID
            fieldIdentifier:(NSString*)fieldIdentifier
              uniqueFieldID:(autofill::FieldRendererId)uniqueFieldID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  self.selected = YES;
  _suggestion = suggestion;
  _formName = [formName copy];
  _uniqueFormID = uniqueFormID;
  _fieldIdentifier = [fieldIdentifier copy];
  _uniqueFieldID = uniqueFieldID;
  _frameID = [frameID copy];
  completion();
}

@end

class PasswordSuggestionBottomSheetMediatorTest : public PlatformTest {
 protected:
  PasswordSuggestionBottomSheetMediatorTest()
      : test_web_state_(std::make_unique<web::FakeWebState>()),
        web_state_list_(&web_state_list_delegate_),
        chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {}

  void SetUp() override {
    GURL url("http://foo.com");
    test_web_state_->SetCurrentURL(url);

    TestChromeBrowserState::Builder builder;
    builder.AddTestingFactory(ios::FaviconServiceFactory::GetInstance(),
                              ios::FaviconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromeFaviconLoaderFactory::GetInstance(),
        IOSChromeFaviconLoaderFactory::GetDefaultFactory());
    builder.AddTestingFactory(ios::HistoryServiceFactory::GetInstance(),
                              ios::HistoryServiceFactory::GetDefaultFactory());
    builder.AddTestingFactory(
        IOSChromePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                web::BrowserState, password_manager::TestPasswordStore>));
    chrome_browser_state_ = builder.Build();

    consumer_ =
        OCMProtocolMock(@protocol(PasswordSuggestionBottomSheetConsumer));

    params_.form_name = "form";
    params_.field_identifier = "field_id";
    params_.field_type = "select-one";
    params_.type = "type";
    params_.value = "value";
    params_.input_missing = false;

    suggestion_providers_ = @[];
  }

  void TearDown() override { [mediator_ disconnect]; }

  void CreateMediator() {
    FormSuggestionTabHelper::CreateForWebState(test_web_state_.get(),
                                               suggestion_providers_);

    web_state_list_.InsertWebState(0, std::move(test_web_state_),
                                   WebStateList::INSERT_ACTIVATE,
                                   WebStateOpener());

    password_manager::MockAffiliationService affiliation_service_;
    store_ =
        base::WrapRefCounted(static_cast<password_manager::TestPasswordStore*>(
            IOSChromePasswordStoreFactory::GetForBrowserState(
                chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS)
                .get()));
    presenter_ = std::make_unique<password_manager::SavedPasswordsPresenter>(
        &affiliation_service_, store_, /*accont_store=*/nullptr);

    mediator_ = [[PasswordSuggestionBottomSheetMediator alloc]
           initWithWebStateList:&web_state_list_
                  faviconLoader:IOSChromeFaviconLoaderFactory::
                                    GetForBrowserState(
                                        chrome_browser_state_.get())
                    prefService:chrome_browser_state_->GetPrefs()
                         params:params_
        savedPasswordsPresenter:presenter_.get()];
  }

  void CreateMediatorWithSuggestions() {
    suggestion_providers_ =
        @[ [PasswordSuggestionBottomSheetMediatorTestSuggestionProvider
            providerWithSuggestions] ];
    CreateMediator();
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<web::FakeWebState> test_web_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  scoped_refptr<password_manager::TestPasswordStore> store_;
  std::unique_ptr<password_manager::SavedPasswordsPresenter> presenter_;
  id consumer_;
  NSArray<id<FormSuggestionProvider>>* suggestion_providers_;
  autofill::FormActivityParams params_;
  PasswordSuggestionBottomSheetMediator* mediator_;
};

// Tests PasswordSuggestionBottomSheetMediator can be initialized.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, Init) {
  CreateMediator();
  EXPECT_TRUE(mediator_);
}

// Tests consumer when no suggestion is available.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, NoSuggestion) {
  CreateMediator();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ dismiss]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests consumer when suggestions are available.
TEST_F(PasswordSuggestionBottomSheetMediatorTest, WithSuggestions) {
  CreateMediatorWithSuggestions();
  EXPECT_TRUE(mediator_);

  OCMExpect([consumer_ setSuggestions:[OCMArg isNotNil]]);
  [mediator_ setConsumer:consumer_];
  EXPECT_OCMOCK_VERIFY(consumer_);
}
