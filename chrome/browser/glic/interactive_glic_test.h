// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_
#define CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_

#include <map>
#include <sstream>
#include <string_view>

#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_test_environment.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/glic_view.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace base {
// Set up a custom |ScopedObservationTrait| for
// |GlicWindowController::WebUiStateObserver|.
template <>
struct ScopedObservationTraits<glic::GlicWindowController,
                               glic::GlicWindowController::WebUiStateObserver> {
  static void AddObserver(
      glic::GlicWindowController* controller,
      glic::GlicWindowController::WebUiStateObserver* observer) {
    controller->AddWebUiStateObserver(observer);
  }
  static void RemoveObserver(
      glic::GlicWindowController* controller,
      glic::GlicWindowController::WebUiStateObserver* observer) {
    controller->RemoveWebUiStateObserver(observer);
  }
};
}  // namespace base

namespace glic::test {

namespace internal {

// Observes `controller` for changes to state().
class GlicWindowControllerStateObserver
    : public ui::test::PollingStateObserver<GlicWindowController::State> {
 public:
  explicit GlicWindowControllerStateObserver(
      const GlicWindowController& controller);
  ~GlicWindowControllerStateObserver() override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicWindowControllerStateObserver,
                               kGlicWindowControllerState);

// Observers the glic app internal state.
class GlicAppStateObserver : public ui::test::ObservationStateObserver<
                                 mojom::WebUiState,
                                 GlicWindowController,
                                 GlicWindowController::WebUiStateObserver> {
 public:
  explicit GlicAppStateObserver(GlicWindowController* controller);
  ~GlicAppStateObserver() override;
  // GlicWindowController::WebUiStateObserver
  void WebUiStateChanged(mojom::WebUiState state) override;
};

DECLARE_STATE_IDENTIFIER_VALUE(GlicAppStateObserver, kGlicAppState);

}  // namespace internal

DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicHostElementId);
DECLARE_ELEMENT_IDENTIFIER_VALUE(kGlicContentsElementId);
extern const InteractiveBrowserTestApi::DeepQuery kPathToMockGlicCloseButton;
extern const InteractiveBrowserTestApi::DeepQuery kPathToGuestPanel;

// Mixin class that adds a mock glic to the current browser.
// If all you need is the combination of this + interactive browser test, use
// `InteractiveGlicTest` (defined below) instead.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest> &&
           std::derived_from<T, InteractiveBrowserTestApi>)
class InteractiveGlicTestT : public T {
 public:
  // Determines whether this is an attached or detached Glic window.
  enum GlicWindowMode {
    kAttached,
    kDetached,
  };

  // What portions of the glic window should be instrumented on open.
  enum GlicInstrumentMode {
    // Instruments the host as `kGlicHostElementId` and contents as
    // `kGlicContentsElementId`.
    kHostAndContents,
    // Instruments only the host as `kGlicHostElementId`.
    kHostOnly,
    // Does not instrument either.
    kNone
  };

  // Constructor that takes `FieldTrialParams` for the glic flag and then
  // forwards the rest of the args.
  template <typename... Args>
  explicit InteractiveGlicTestT(const base::FieldTrialParams& glic_params,
                                Args&&... args)
      : T(std::forward<Args>(args)...) {
    features_.InitWithFeaturesAndParameters(
        {{features::kGlic, glic_params},
         {features::kTabstripComboButton, {}},
         {features::kGlicKeyboardShortcutNewBadge, {}}},
        {});
  }

  // Default constructor (no forwarded args or field trial parameters).
  InteractiveGlicTestT() : InteractiveGlicTestT(base::FieldTrialParams()) {}

  // Constructor with no field trial params; all arguments are forwarded to the
  // base class.
  template <typename Arg, typename... Args>
    requires(!std::same_as<base::FieldTrialParams, std::remove_cvref_t<Arg>>)
  explicit InteractiveGlicTestT(Arg&& arg, Args&&... args)
      : InteractiveGlicTestT(base::FieldTrialParams(),
                             std::forward<Arg>(arg),
                             std::forward<Args>(args)...) {}

  ~InteractiveGlicTestT() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    T::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &InteractiveGlicTestT<T>::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();

    Test::embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));

    ASSERT_TRUE(Test::embedded_test_server()->Start());

    // Need to set this here rather than in SetUpCommandLine because we need to
    // use the embedded test server to get the right URL and it's not started
    // at that time.
    std::ostringstream path;
    path << "/glic/test_client/index.html";

    // Append the query parameters to the URL.
    bool first_param = true;
    auto encode = [](const std::string_view& value) {
      url::RawCanonOutputT<char> encoded;
      url::EncodeURIComponent(value, &encoded);
      return std::string(encoded.view());
    };
    for (const auto& [key, value] : mock_glic_query_params_) {
      path << (first_param ? "?" : "&");
      first_param = false;
      path << encode(key);
      if (!value.empty()) {
        path << "=" << encode(value);
      }
    }

    auto* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitchASCII(
        ::switches::kGlicGuestURL,
        Test::embedded_test_server()->GetURL(path.str()).spec());

    Browser* browser = InProcessBrowserTest::browser();

    glic_test_environment_ =
        std::make_unique<glic::GlicTestEnvironment>(browser->profile());
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  // Ensures that the WebContents for some combination of glic host and contents
  // are instrumented, per `instrument_mode`.
  auto WaitForAndInstrumentGlic(GlicInstrumentMode instrument_mode) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;

    switch (instrument_mode) {
      case GlicInstrumentMode::kHostAndContents:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicContentsElementId, false),
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller())),
            Api::InAnyContext(Api::Steps(
                Api::InstrumentNonTabWebView(
                    kGlicHostElementId, GlicView::kWebViewElementIdForTesting),
                Api::InstrumentInnerWebContents(kGlicContentsElementId,
                                                kGlicHostElementId, 0),
                Api::WaitForWebContentsReady(kGlicContentsElementId))),
            Api::WaitForState(internal::kGlicWindowControllerState,
                              GlicWindowController::State::kOpen),
            Api::StopObservingState(internal::kGlicWindowControllerState)
            /*, WaitForElementVisible(kPathToGuestPanel)*/);
        break;
      case GlicInstrumentMode::kHostOnly:
        steps = Api::Steps(
            Api::UninstrumentWebContents(kGlicHostElementId, false),
            Api::ObserveState(internal::kGlicWindowControllerState,
                              std::ref(window_controller())),
            Api::InAnyContext(Api::InstrumentNonTabWebView(
                kGlicHostElementId, GlicView::kWebViewElementIdForTesting)),
            Api::WaitForState(
                internal::kGlicWindowControllerState,
                testing::Matcher<GlicWindowController::State>(testing::AnyOf(
                    GlicWindowController::State::kWaitingForGlicToLoad,
                    GlicWindowController::State::kOpen))),
            Api::StopObservingState(internal::kGlicWindowControllerState));
        break;
      case GlicInstrumentMode::kNone:
        // no-op.
        break;
    }

    Api::AddDescriptionPrefix(steps, "WaitForAndInstrumentGlic");
    return steps;
  }

  // Activate one of the glic entrypoints.
  // If `instrument_glic_contents` is true both the host and contents will be
  // instrumented (see `WaitForAndInstrumentGlic()`) else only the host will be
  // instrumented (`WaitForAndInstrumentGlicHostOnly()`).
  auto OpenGlicWindow(GlicWindowMode window_mode,
                      GlicInstrumentMode instrument_mode =
                          GlicInstrumentMode::kHostAndContents) {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    Api::MultiStep steps;
    steps.push_back(
        EnsureGlicWindowState("window must be closed in order to open it",
                              GlicWindowController::State::kClosed));
    // Technically, this toggles the window, but we've already ensured that it's
    // closed.
    steps.push_back(ToggleGlicWindow(window_mode));
    steps =
        Api::Steps(std::move(steps), WaitForAndInstrumentGlic(instrument_mode));
    Api::AddDescriptionPrefix(steps, "OpenGlicWindow");
    return steps;
  }

  // Toggles Glic through one of the entrypoints.
  // Does not wait for Glic to open or close, tests using this should check for
  // the correct window state after toggling.
  auto ToggleGlicWindow(GlicWindowMode window_mode) {
    switch (window_mode) {
      case GlicWindowMode::kAttached:
        return Api::PressButton(kGlicButtonElementId);
      case GlicWindowMode::kDetached:
        return Api::Do(
            [this] { window_controller().ShowDetachedForTesting(); });
    }
  }

  // Ensures a mock glic button is present and then clicks it. Works even if the
  // element is off-screen.
  auto ClickMockGlicElement(
      const WebContentsInteractionTestUtil::DeepQuery& where) {
    auto steps = Api::Steps(
        // Note: Elements on the test client don't need to be in the viewport to
        // be used. Ideally we would wait until the element is visible, but not
        // necessarily on screen. Because we don't have any elements that get
        // hidden on the test client, waiting for body visibility is good
        // enough.
        Api::WaitForElementVisible(kGlicContentsElementId, {"body"}),
        // TODO(dfried): Figure out why Api::CheckJsResultAt() here doesn't
        // work. Error:
        // Interactive test failed on step 28 (ClickMockGlicElement:
        // CheckJsResultAt( {"#contextAccessIndicator"}, " ... with reason
        // kSequenceDestroyed; step type kShown; id ElementIdentifier
        // kGlicContentsElementId.
        Api::ExecuteJsAt(kGlicContentsElementId, where, "(el)=>el.click()"));

    Api::AddDescriptionPrefix(steps, "ClickMockGlicElement");
    return steps;
  }

  // Closes the glic window, which must be open.
  //
  // TODO: this only works if glic is actually loaded; handle the case where the
  // contents pane has either not loaded or failed to load.
  auto CloseGlicWindow() {
    // NOTE: The use of "Api::" here is required because this is a template
    // class with weakly-specified base class; it is not necessary in derived
    // test classes.
    auto steps = Api::InAnyContext(Api::Steps(
        EnsureGlicWindowState("cannot close window if it is not open",
                              GlicWindowController::State::kOpen),
        ClickMockGlicElement(kPathToMockGlicCloseButton),
        Api::WaitForHide(kGlicViewElementId)));
    Api::AddDescriptionPrefix(steps, "CloseGlicWindow");
    return steps;
  }

 protected:
  GlicKeyedService* glic_service() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(
        InProcessBrowserTest::browser()->GetProfile());
  }

  GlicWindowController& window_controller() {
    return glic_service()->window_controller();
  }

  template <typename... M>
  auto EnsureGlicWindowState(const std::string& desc, M&&... matchers) {
    return Api::CheckResult([this]() { return window_controller().state(); },
                            testing::Matcher<GlicWindowController::State>(
                                testing::AnyOf(std::forward<M>(matchers)...)),
                            desc);
  }

  // Adds a query param to the URL that will be used to load the mock glic.
  // Must be called before `SetUpOnMainThread()`. Both `key` and `value` (if
  // specified) will be URL-encoded for safety.
  void add_mock_glic_query_param(const std::string_view& key,
                                 const std::string_view& value = "") {
    mock_glic_query_params_.emplace(key, value);
  }

 private:
  // Because of limitations in the template system, calls to base class methods
  // that are guaranteed by the `requires` clause must still be scoped. These
  // are here for convenience to make the methods above more readable.
  using Api = InteractiveBrowserTestApi;
  using Test = InProcessBrowserTest;

  base::test::ScopedFeatureList features_;

  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<glic::GlicTestEnvironment> glic_test_environment_;
  std::map<std::string, std::string> mock_glic_query_params_;
};

// For most tests, you can alias or inherit from this instead of deriving your
// own `InteractiveGlicTestT<...>`.
using InteractiveGlicTest = InteractiveGlicTestT<InteractiveBrowserTest>;

// For testing IPH associated with glic - i.e. help bubbles that anchor in the
// chrome browser rather than showing up in the glic content itself - inherit
// from this.
using InteractiveGlicFeaturePromoTest =
    InteractiveGlicTestT<InteractiveFeaturePromoTest>;

}  // namespace glic::test

#endif  // CHROME_BROWSER_GLIC_INTERACTIVE_GLIC_TEST_H_
