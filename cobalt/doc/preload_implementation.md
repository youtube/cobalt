# Cobalt Preloading Implementation

This document describes the implementation of the preloading feature in Cobalt. Preloading allows the application to start and load the initial web page in the background (hidden) state, improving the perceived startup time when the user eventually launches the app.

## Overview

The preloading feature is triggered by a Starboard event `kSbEventTypePreload` or by passing the `--preload` command-line switch. When active:

1.  The browser starts up but **defers the creation of the native window**.
2.  The `WebContents` (the web page) is loaded but set to `PRERENDER` visibility state.
3.  The application waits for a `kSbEventTypeReveal` event (e.g., triggered by `SIGCONT` or a platform-specific mechanism) to create the window and show the content.
4.  The application can also be concealed back to a windowless state via `kSbEventTypeConceal` (e.g., triggered by `SIGUSR1`).

## Detailed Implementation

### 1. Command Line Switch

A new command-line switch `--preload` is defined in `cobalt/browser/switches.h` and `cobalt/shell/common/shell_switches.h`.

### 2. Application Entry Point (`cobalt/app/cobalt.cc`)

The `SbEventHandle` function is updated to handle the preloading lifecycle:

*   **`kSbEventTypePreload`**: When this event is received, the `--preload` switch is appended to the argument list passed to `InitCobalt`. This puts the entire browser process into "preload mode".
*   **`kSbEventTypeReveal`**: Iterates over all active shells and calls `Shell::GetPlatform()->Reveal(shell)`. It also updates the `WebContents` visibility to shown.
*   **`kSbEventTypeConceal`**: Iterates over all active shells and calls `Shell::GetPlatform()->Conceal(shell)`. It also updates the `WebContents` visibility to hidden.

### 3. Shell Initialization (`cobalt/shell/browser/shell.cc`)

In the `Shell` constructor:
*   It checks for the `switches::kPreload` switch.
*   If present, it calls `web_contents_->WasHidden()` to ensure the renderer knows it is not visible initially.

### 4. Platform Delegate (`ShellPlatformDelegate`)

The `ShellPlatformDelegate` interface in `cobalt/shell/browser/shell_platform_delegate.h` was extended with two new methods:

*   `virtual void Conceal(Shell* shell);`
*   `virtual void Reveal(Shell* shell);`

These methods are responsible for destroying and recreating the native platform window.

#### Implementation for Views (`cobalt/shell/browser/shell_platform_delegate_views.cc`)

The primary logic resides here for Linux/Desktop builds:

*   **`CreatePlatformWindow`**: Checks for `switches::kPreload`. If present, it sets a `preloaded_` flag and **returns immediately**, skipping the creation of `views::Widget`. This effectively starts the browser without a window.
*   **`Reveal`**: Checks if the `window_widget` exists. If not (first reveal or after conceal), it:
    *   Creates a new `views::Widget` and `ShellView`.
    *   Attaches the existing `Shell` and `WebContents` to this new window.
    *   Shows the window.
*   **`Conceal`**:
    *   Calls `ReleaseShell()` on the `ShellView` to ensure the `Shell` object is not destroyed when the window is closed.
    *   Closes and destroys the `window_widget`.
    *   Sets `window_widget` to `nullptr`.
*   **Safeguards**: Methods like `SetContents`, `EnableUIControl`, `SetAddressBarURL`, etc., were updated to check if `window_widget` is null before accessing it, to prevent crashes when running in the windowless preload state.

#### Other Platforms

*   **Aura (`shell_platform_delegate_aura.cc`)**: Updated to support `Conceal`/`Reveal` logic (creation/destruction of `aura::Window`).
*   **Android & iOS**: Added empty implementations for `Conceal` and `Reveal` to satisfy linking requirements.

### 6. Window Lifecycle Management & Crash Fix (`PlatformWindowStarboard` & `CobaltContentBrowserClient`)

To prevent crashes when the window is destroyed and recreated (Conceal/Reveal cycle), a mechanism was added to notify the browser client about window destruction.

*   **`ui::PlatformWindowStarboard` (`platform_window_starboard.h/cc`)**:
    *   Added `SetWindowDestroyedCallback` static method.
    *   In the destructor (`~PlatformWindowStarboard`), the registered callback is invoked. This ensures any listeners know when the underlying `SbWindow` is gone.

*   **`CobaltContentBrowserClient` (`cobalt/browser/cobalt_content_browser_client.h/cc`)**:
    *   Added `OnSbWindowDestroyed` method.
    *   Registers this method via `PlatformWindowStarboard::SetWindowDestroyedCallback` in the constructor.
    *   In `OnSbWindowDestroyed`, it resets the `cached_sb_window_` handle to 0.
    *   This prevents `OnSbWindowCreated` (called during `Reveal`) from hitting a `CHECK` failure because it thinks a window already exists.

## Lifecycle Flow

1.  **Start (Preload)**:
    *   `./cobalt_loader --preload`
    *   Browser initializes. `CreatePlatformWindow` is called but does nothing.
    *   Page loads in background. Renderer visibility is set to hidden.
2.  **Reveal**:
    *   `SIGCONT` (handled by `suspend_signals.cc` -> `Focus` -> `Application::Focus` -> ... -> `kSbEventTypeReveal`)
    *   `ShellPlatformDelegate::Reveal` creates the window.
    *   `WebContents` is shown.
    *   `PlatformWindowStarboard` created -> calls `CobaltContentBrowserClient::OnSbWindowCreated` -> caches `SbWindow`.
3.  **Conceal**:
    *   `SIGUSR1` (handled by `suspend_signals.cc` -> `Conceal` -> `SbSystemRequestConceal` -> ... -> `kSbEventTypeConceal`)
    *   `ShellPlatformDelegate::Conceal` destroys the window.
    *   `WebContents` is hidden.
    *   `PlatformWindowStarboard` destroyed -> calls `CobaltContentBrowserClient::OnSbWindowDestroyed` -> clears `cached_sb_window_`.

This architecture allows Cobalt to maintain the browser state and DOM in memory while releasing the heavy resources associated with the native window system when concealed or preloading.
