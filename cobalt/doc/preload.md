# Application Preload

Preloading allows Cobalt to start and initialize in the background without
displaying any user interface. This enables a "background-to-foreground"
transition that appears instantaneous to the user when they eventually choose to
launch the application.

## For Web Designers

When an application is preloaded, it starts in a **hidden** state. Standard Web
APIs correctly reflect this state:

-   `document.visibilityState` will be `"hidden"`.
-   `document.hidden` will be `true`.

### Best Practices

-   Avoid starting audio playback or heavy graphical animations while in the
    hidden state.
-   Listen for the `visibilitychange` event on the `document` object to detect
    when the application transitions from preloaded to visible.

## For Device Manufacturers (Starboard Porters)

Preloading is managed through the Starboard lifecycle.

-   **Startup:** To start in preload mode, the Starboard implementation should
    send the `kSbEventTypePreload` event instead of `kSbEventTypeStart`.
    -   In the shared Starboard application framework (`starboard/shared/starboard/application.cc`),
        the `--preload` command-line flag is recognized via the `kPreloadSwitch` constant.
    -   If a platform implementation's `Application::IsPreloadImmediate()` returns true
        (typically by checking `HasPreloadSwitch()`), the application will
        automatically call `DispatchPreload()`.
    -   `DispatchPreload()` then creates and dispatches the `kSbEventTypePreload`
        initial event, which is the signal to the application that it should
        initialize in a hidden state.
-   **Revelation:** To bring a preloaded application to the foreground, the
    platform should send a `kSbEventTypeReveal` signal.
    -   On Linux-based platforms, this is often triggered by sending a `SIGCONT`
        signal to the process.
    -   An example of this mapping can be found in `starboard/shared/signal/suspend_signals.cc`,
        where `SIGCONT` is handled by requesting a focus change.
    -   The shared Starboard application logic in `starboard/shared/starboard/application.cc`
        automatically injects a `kSbEventTypeReveal` event if a focus request is
        received while the application is in the preloaded (concealed) state.
-   **Resource Management:** Cobalt defers the creation of the native window and
    associated graphics resources until the first `Reveal` signal is received.
    This minimizes the memory and CPU footprint of the application while it
    resides in the background.
-   **Splash Screen:** The creation of the splash screen's `WebContents` is also
    skipped when the application is preloaded, further reducing the background
    footprint.

## For Cobalt Developers

The "preload" signal is converted into a generic "visibility" state as soon as
it enters the application layer.

### Implementation Flow

1.  **Entry Point:** `SbEventHandle` (in `cobalt/app/cobalt.cc`) receives
    `kSbEventTypePreload`.
2.  **State Propagation:** An `is_visible` boolean (set to `false`) is passed
    through the constructor chain: `CobaltMainDelegate` ->
    `CobaltContentBrowserClient` -> `CobaltBrowserMainParts` ->
    `ShellBrowserMainParts`.
3.  **Initialization:** `ShellBrowserMainParts::PreMainMessageLoopRun` calls
    `Shell::Initialize`, passing the visibility state.
4.  **Splash Screen Skip:** `Shell::CreateNewWindow` checks the visibility state
    and skips creating the splash screen `WebContents` if the application is
    initially hidden.
5.  **Platform Delegate:** `Shell::Initialize` passes the state to
    `ShellPlatformDelegate::Initialize`. Each platform implementation (e.g.,
    Aura, Views) stores this in a member variable.
6.  **Deferred Resource Creation:** `CreatePlatformWindow` checks `IsVisible()`
    (or `IsConcealed()`) and defers creating the `NativeWindow` and `Widget` if
    the application is not yet visible.
7.  **Revelation:** When `kSbEventTypeReveal` is received, `Shell::OnReveal()` is
    triggered. This calls `ShellPlatformDelegate::RevealShell`, which creates
    the deferred window resources and calls `WasShown()` on the `WebContents`,
    triggering the `visibilitychange` event for the web application.

### Unit Testing

Application lifecycle and visibility transitions are covered by the following
unit tests in the `cobalt_unittests` binary:

-   **`LifecycleTest`** (`cobalt/shell/browser/lifecycle_unittest.cc`): Verifies
    correct window creation and visibility state propagation during startup,
    revelation, and redundant signals.
-   **`SplashScreenTest`** (`cobalt/shell/browser/splash_screen_unittest.cc`):
    Includes tests for ensuring the splash screen is skipped during preloading.

### Integration Testing

A robust integration test is provided in `cobalt/tools/test_preload.sh`. This
test:

1.  Launches Cobalt in preload mode.
2.  Uses the Chrome DevTools Protocol (CDP) to verify that
    `document.visibilityState` is initially `"hidden"`.
3.  Sends a `SIGCONT` signal to reveal the application.
4.  Verifies via CDP that `document.visibilityState` transitions to `"visible"`.
5.  Sends a `SIGPWR` signal to verify clean shutdown.
