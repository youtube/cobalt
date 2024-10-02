// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/app_controller_mac.h"

#include "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/profile_picker.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "net/base/mac/url_conversions.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest_mac.h"
#include "ui/base/page_transition_types.h"

namespace {

Profile& CreateAndWaitForProfile(const base::FilePath& profile_dir) {
  Profile& profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(), profile_dir);
  return profile;
}

Profile& CreateAndWaitForGuestProfile() {
  return CreateAndWaitForProfile(ProfileManager::GetGuestProfilePath());
}

void SetGuestProfileAsLastProfile() {
  AppController* ac = base::mac::ObjCCast<AppController>(
      [[NSApplication sharedApplication] delegate]);
  ASSERT_TRUE(ac);
  // Create the guest profile, and set it as the last used profile.
  Profile& guest_profile = CreateAndWaitForGuestProfile();
  [ac setLastProfile:&guest_profile];
  Profile* profile = [ac lastProfileIfLoaded];
  ASSERT_TRUE(profile);
  EXPECT_EQ(guest_profile.GetPath(), profile->GetPath());
  EXPECT_TRUE(profile->IsGuestSession());
  // Also set the last used profile path preference. If the profile does need to
  // be read from disk for some reason this acts as a backstop.
  g_browser_process->local_state()->SetString(
      prefs::kProfileLastUsed, guest_profile.GetPath().BaseName().value());
}

}  // namespace

using AuthSessionBrowserTest = InProcessBrowserTest;

@interface MockASWebAuthenticationSessionRequest : NSObject {
  base::scoped_nsobject<NSUUID> _uuid;
  base::scoped_nsobject<NSURL> _initialURL;

  base::scoped_nsobject<NSURL> _callbackURL;
  base::scoped_nsobject<NSError> _cancellationError;
}

// ASWebAuthenticationSessionRequest:

@property(readonly, nonatomic) NSURL* URL;
@property(readonly, nonatomic) BOOL shouldUseEphemeralSession;
@property(nullable, readonly, nonatomic, copy) NSString* callbackURLScheme;
@property(readonly, nonatomic) NSUUID* UUID;

- (void)completeWithCallbackURL:(NSURL*)url;
- (void)cancelWithError:(NSError*)error;

// Utilities:

@property(readonly, nonatomic) NSURL* callbackURL;
@property(readonly, nonatomic) NSError* cancellationError;

@end

@implementation MockASWebAuthenticationSessionRequest

- (instancetype)initWithInitialURL:(NSURL*)initialURL {
  if (self = [super init]) {
    _uuid.reset([[NSUUID alloc] init]);
    _initialURL.reset(initialURL, base::scoped_policy::RETAIN);
  }
  return self;
}

- (NSURL*)URL {
  return _initialURL;
}

- (BOOL)shouldUseEphemeralSession {
  return false;
}

- (NSString*)callbackURLScheme {
  // Use occasional capital letters to test the canonicalization of schemes.
  return @"mAkEiTsO";
}

- (NSUUID*)UUID {
  return _uuid.get();
}

- (void)completeWithCallbackURL:(NSURL*)url {
  _callbackURL.reset(url, base::scoped_policy::RETAIN);
}

- (void)cancelWithError:(NSError*)error {
  _cancellationError.reset(error, base::scoped_policy::RETAIN);
}

- (NSURL*)callbackURL {
  return _callbackURL.get();
}

- (NSError*)cancellationError {
  return _cancellationError.get();
}

@end

// Tests that an OS request to cancel an auth session works.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, OSCancellation) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Ask the app controller to stop handling our session request.

    [session_handler cancelWebAuthenticationSessionRequest:request];

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the user cancellation callback.

    EXPECT_EQ(nil, session_request.get().callbackURL);
    ASSERT_NE(nil, session_request.get().cancellationError);
    EXPECT_EQ(ASWebAuthenticationSessionErrorDomain,
              session_request.get().cancellationError.domain);
    EXPECT_EQ(ASWebAuthenticationSessionErrorCodeCanceledLogin,
              session_request.get().cancellationError.code);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

// Tests that a user request to cancel an auth session works.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserCancellation) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user closing the window.

    browser->window()->Close();

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the user cancellation callback.

    EXPECT_EQ(nil, session_request.get().callbackURL);
    ASSERT_NE(nil, session_request.get().cancellationError);
    EXPECT_EQ(ASWebAuthenticationSessionErrorDomain,
              session_request.get().cancellationError.domain);
    EXPECT_EQ(ASWebAuthenticationSessionErrorCodeCanceledLogin,
              session_request.get().cancellationError.code);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

// Tests that the session works even if the profile is not already loaded.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, ProfileNotLoaded) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    // Clear the last profile. It will be set by default since NSApp in browser
    // tests can activate.
    AppController* ac = base::mac::ObjCCast<AppController>(
        [[NSApplication sharedApplication] delegate]);
    ASSERT_TRUE(ac);
    [ac setLastProfile:nullptr];

    // Use a profile that is not loaded yet.
    const std::string kProfileName = "Profile 2";
    g_browser_process->local_state()->SetString(prefs::kProfileLastUsed,
                                                kProfileName);
    const base::FilePath kProfilePath =
        browser()->profile()->GetPath().DirName().Append(kProfileName);
    ASSERT_FALSE(
        g_browser_process->profile_manager()->GetProfileByPath(kProfilePath));

    // Ask the app controller to start handling our session request.
    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);
    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect the profile to be loaded and browser window to be opened.
    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_TRUE(
        g_browser_process->profile_manager()->GetProfileByPath(kProfilePath));
    EXPECT_EQ(start_browser_count + 1, browser_list->size());
    EXPECT_EQ(browser->profile()->GetPath(), kProfilePath);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

// Tests that the profile picker is shown instead if the profile is unavailable.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, ProfileNotAvailable) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    // Use the guest profile, but mark it as disallowed.
    SetGuestProfileAsLastProfile();
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetBoolean(prefs::kBrowserGuestModeEnabled, false);

    // The profile picker is initially closed.
    base::RunLoop run_loop;
    ProfilePicker::AddOnProfilePickerOpenedCallbackForTesting(
        run_loop.QuitClosure());
    ASSERT_FALSE(ProfilePicker::IsOpen());

    // Ask the app controller to start handling our session request.
    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);
    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect the profile picker to be opened, no browser was created, and the
    // session was cancelled.
    run_loop.Run();
    EXPECT_TRUE(ProfilePicker::IsOpen());
    EXPECT_EQ(start_browser_count, browser_list->size());
    EXPECT_EQ(nil, session_request.get().callbackURL);
    ASSERT_NE(nil, session_request.get().cancellationError);
    EXPECT_EQ(ASWebAuthenticationSessionErrorDomain,
              session_request.get().cancellationError.domain);
    EXPECT_EQ(ASWebAuthenticationSessionErrorCodePresentationContextInvalid,
              session_request.get().cancellationError.code);

  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

// Tests that a successful auth session works via direct navigation.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessDirect) {
  if (@available(macOS 10.15, *)) {
    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user successfully logging in with a non-redirected load of
    // a URL with the expected scheme.

    GURL success_url("makeitso://enterprise");
    browser->tab_strip_model()->GetWebContentsAt(0)->GetController().LoadURL(
        success_url, content::Referrer(), ui::PAGE_TRANSITION_GENERATED,
        std::string());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

namespace {

std::unique_ptr<net::test_server::HttpResponse> RedirectionRequestHandler(
    const GURL& redirection_url,
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_FOUND);
  http_response->AddCustomHeader("Location", redirection_url.spec());
  return http_response;
}

}  // namespace

// Tests that a successful auth session works via a redirect.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessEventualRedirect) {
  if (@available(macOS 10.15, *)) {
    GURL success_url("makeitso://cerritos");

    net::EmbeddedTestServer embedded_test_server;
    embedded_test_server.RegisterRequestHandler(
        base::BindRepeating(RedirectionRequestHandler, success_url));
    ASSERT_TRUE(embedded_test_server.Start());

    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:[NSURL URLWithString:@"about:blank"]]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Simulate the user successfully logging in with a redirected load of a URL
    // with the expected scheme.

    GURL url = embedded_test_server.GetURL("/something");
    browser->tab_strip_model()->GetWebContentsAt(0)->GetController().LoadURL(
        url, content::Referrer(), ui::PAGE_TRANSITION_GENERATED, std::string());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}

// Tests that a successful auth session works if the success scheme comes on a
// redirect from the initial navigation.
IN_PROC_BROWSER_TEST_F(AuthSessionBrowserTest, UserSuccessInitialRedirect) {
  if (@available(macOS 10.15, *)) {
    GURL success_url("makeitso://titan");

    net::EmbeddedTestServer embedded_test_server;
    embedded_test_server.RegisterRequestHandler(
        base::BindRepeating(RedirectionRequestHandler, success_url));
    ASSERT_TRUE(embedded_test_server.Start());

    auto* browser_list = BrowserList::GetInstance();
    size_t start_browser_count = browser_list->size();

    GURL url = embedded_test_server.GetURL("/something");
    base::scoped_nsobject<MockASWebAuthenticationSessionRequest>
        session_request([[MockASWebAuthenticationSessionRequest alloc]
            initWithInitialURL:net::NSURLWithGURL(url)]);
    id<ASWebAuthenticationSessionWebBrowserSessionHandling> session_handler =
        ASWebAuthenticationSessionWebBrowserSessionManager.sharedManager
            .sessionHandler;
    ASSERT_NE(nil, session_handler);

    // Ask the app controller to start handling our session request.

    id request = session_request.get();
    [session_handler beginHandlingWebAuthenticationSessionRequest:request];

    // Expect a browser window to be opened.

    Browser* browser = ui_test_utils::WaitForBrowserToOpen();
    EXPECT_EQ(start_browser_count + 1, browser_list->size());

    // Expect the browser window to close.

    ui_test_utils::WaitForBrowserToClose(browser);
    EXPECT_EQ(start_browser_count, browser_list->size());

    // Expect there to have been the success callback.

    ASSERT_NE(nil, session_request.get().callbackURL);
    EXPECT_EQ(nil, session_request.get().cancellationError);
    EXPECT_NSEQ(net::NSURLWithGURL(success_url),
                session_request.get().callbackURL);
  } else {
    GTEST_SKIP() << "ASWebAuthenticationSessionRequest is only available on "
                    "macOS 10.15 and higher.";
  }
}
