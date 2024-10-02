# ChromeOS Personalization

## User Types and Profiles

ChromeOS Personalization features interact differently with different user
types. It is important for feature development to consider how the different
user types will be affected.

see: [`//components/user_manager/user_type.h`](../../../components/user_manager/user_type.h)

For a more in depth explanation, see:

[Profiles, Sessions, Users, and more for ChromeOS Personalization](go/chromeos-personalization-user-types)

## Tests

### Background

Personalization App takes a layered approach to testing. There are C++ unit
tests, javascript component browser tests, and javascript full-app browsertests.

* mojom handler unit tests
  * `//chrome/browser/ash/web_applications/personalization_app/*unittest.cc`
  * `unit_tests --gtest_filter=*PersonalizationApp*`
  * primarily to test behavior of mojom handlers
  * heavily mocked out ash environment
    * fake user manager
    * fake wallpaper_controller
    * etc
* component browser tests
  * `personalization_app_component_browsertest.js`
  * `browser_tests --gtest_filter=*PersonalizationAppComponent*`
  * loads test cases from `//chrome/test/data/webui/chromeos/personalization_app/*`
  * Opens an empty browser window, loads javascript necessary to render a
    single Polymer element, and executes javascript tests against that component
  * All mojom calls are faked in javascript
    * any mojom call that reaches
    `personalization_app_mojom_banned_browsertest_fixture.h`
    will immediately fail the test
* controller browser tests
  * `personalization_app_controller_browsertest.js`
  * `browser_tests --gtest_filter=*PersonalizationAppController*`
  * no UI elements, javascript testing of controller functions, reducers, logic
  * All mojom calls are faked in javascript the same way as component browser
  tests
* app browser tests
  * `personalization_app_browsertest.js`
  * `browser_tests --gtest_filter=*PersonalizationApp*BrowserTest`
  * Uses fixture `personalization_app_browsertest_fixture.h`
    * wallpaper mocked out at network layer by mocking out wallpaper fetchers
    via `TestWallpaperFetcherDelegate`
    * all others mock out mojom layer via fake mojom providers
    `FakePersonalizationApp{Ambient,KeyboardBacklight,Theme,User}Provider`
* System Web App integration tests
  * `personalization_app_integration_browsertest.cc`
  * `browser_tests --gtest_filter=*PersonalizationAppIntegration*`
  * Tests that the app install, launches without error
  * Also tests special tricky system UI support for full screen transparency for
  wallpaper preview because they cannot be tested in javascript

### Where should I write my test?

* complex behavior that involves multiple parts of the application and mojom
handlers
  * app browser tests
* a single javascript component
  * component browser tests
* javascript logic and state management
  * controller browser tests
* mojom handling logic
  * mojom handler unit tests

## Environment Setup
### VSCode

- Follow [vscode setup](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/vscode.md).
- Create `tsconfig.json` using [helper script](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/webui/personalization_app/tools/gen_tsconfig.py).
  Please follow the help doc in the header of the helper script.
- Edit `${PATH_TO_CHROMIUM}/src/.git/info/exclude` and add these lines
  ```
  /ash/webui/personalization_app/resources/tsconfig.json
  /chrome/test/data/webui/chromeos/personalization_app/tsconfig.json
  ```
