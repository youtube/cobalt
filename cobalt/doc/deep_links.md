# Cobalt Deep Links

- [Cobalt Deep Links](#cobalt-deep-links)
  - [Deep Links](#deep-links)
  - [Web API](#web-api)
  - [Platform (Starboard) API](#platform-starboard-api)
  - [Behavior Details](#behavior-details)

## Deep Links

For Cobalt, a deep link is a string that can be sent from the platform to an
application running in Cobalt. Generally, it can be used as any string value
signal, but typically deep links are used to specify a view, page, or content
to be shown by the application. While these strings typically are URI-formatted
values, when deep link strings are received by Cobalt they are forwarded to the
running application without separate validation or modification.

Applications should interpret received deep links as superseding previous deep
links. Before a `'deeplink'` event listener is registered by the web
application, any new deep link received by Cobalt overwrites the previously
stored unconsumed link so that only the most recent link is retained. Once a
`'deeplink'` event listener is registered, subsequent deep links are delivered
in order over Mojo IPC to the renderer.

The startup URL passed to Cobalt determines which application Cobalt will load.
Web deep links intended as a signal to the application should not be sent to
Cobalt as a startup URL because that would result in a different application
being loaded. Since a deep link is a string that may originate from an
untrusted source on the device, it should not be used directly to determine
what application Cobalt will load. (In `Gold` release builds, if a non-default
startup URL is passed on the command line and no initial deep link was
provided, Cobalt resets the startup URL to the default YouTube TV URL and
reroutes the custom URL into the initial deep link.)

Deep links are made visible to applications by Cobalt with a Web API that is
separate from the `Location` interface Web API. Cobalt will never directly
navigate as a result of a received deep link, even if the link matches the
current application location (for example, with a query or fragment
identifier). Applications that wish to navigate as a result of incoming deep
links should do so explicitly when they are received.

## Web API

The deep link Web API is exposed on `window.h5vcc.runtime` (`H5vccRuntime`),
which inherits from the standard DOM `EventTarget` interface:
-   `window.h5vcc.runtime.initialDeepLink`: A read-only `DOMString?` property
    containing the initial deep link captured when `window.h5vcc.runtime` was
    instantiated.
-   `window.h5vcc.runtime.ondeeplink` / `addEventListener('deeplink', ...)`:
    Fires a `DeepLinkEvent` (with `event.url` set to the deep link string) when
    subsequent deep links are received.

The WebIDL definitions are located in
`third_party/blink/renderer/modules/cobalt/h5vcc_runtime/h_5_vcc_runtime.idl`
and `deep_link_event.idl`:

```webidl
[
    Exposed=Window,
    SecureContext
]
interface H5vccRuntime : EventTarget {
    readonly attribute DOMString? initialDeepLink;
    attribute EventHandler ondeeplink;
};

[
    Exposed=Window,
    SecureContext
]
interface DeepLinkEvent : Event {
    readonly attribute DOMString url;
};
```

Example usage in JavaScript:

```js
// 1. Read the initial deep link (captured and cleared when h5vcc.runtime is created):
const initialLink = window.h5vcc.runtime.initialDeepLink;
if (initialLink) {
  handleDeepLink(initialLink);
}

// 2. Listen for subsequent deep links arriving while the app is running:
window.h5vcc.runtime.addEventListener('deeplink', (event) => {
  handleDeepLink(event.url);
});
```

## Platform (Starboard) API

Deep links can be passed into Cobalt in two ways:
-   **As the Startup Link (`kSbEventTypeStart` or `kSbEventTypePreload`):**
    When Cobalt is first started or preloaded, a deep link can be passed via the
    `link` member of the `SbEventStartData` structure passed to `SbEventHandle`.
    In `starboard::Application` subclasses
    (`starboard/shared/starboard/application.cc`), this can be set by calling
    `Application::SetStartLink(url)` or passing `--link=<url>` (`kLinkSwitch`)
    on the command line.
-   **As a Deep Link Event (`kSbEventTypeLink`):**
    At any time while Cobalt is running in any lifecycle state (`Concealed`,
    `Frozen`, `Blurred`, or `Started`), a deep link can be dispatched via
    `SbEventHandle` as a `kSbEventTypeLink` event whose `data` payload is a
    null-terminated `const char*` string (or by calling
    `Application::Link(url)`).

> **Lifecycle Note:** `kSbEventTypeLink` has **no lifecycle state-transition
> side-effect**. It delivers the deep link string to the application but does
> not reveal or focus Cobalt on its own. If a deep link is intended to bring a
> preloaded (`Concealed`) or suspended (`Frozen`) Cobalt instance to the
> foreground, the platform must separately dispatch `kSbEventTypeFocus` (or
> `kSbEventTypeReveal` followed by `kSbEventTypeFocus`). See
> [Cobalt Application Lifecycle](lifecycle.md) for details.

## Behavior Details

Both the Startup Link (`SbEventStartData::link`) and subsequent Deep Link
Events (`kSbEventTypeLink`) are managed by `DeepLinkManager` in the browser
process (`cobalt/browser/h5vcc_runtime/deep_link_manager.cc`):

1.  **Storage Before Consumption:**
    Before the renderer instantiates `window.h5vcc.runtime` or registers a
    `'deeplink'` event listener, `DeepLinkManager` stores the most recently
    received deep link in memory (`deep_link_`). Any new `kSbEventTypeLink`
    event that arrives before consumption overwrites `deep_link_` with the
    latest URL.
2.  **Consumption on `window.h5vcc.runtime` Instantiation (`initialDeepLink`):**
    When the web application first accesses `window.h5vcc.runtime`, the
    `H5vccRuntime` constructor synchronously invokes the
    `GetAndClearInitialDeepLinkSync` Mojo IPC, which atomically fetches and
    clears the stored `deep_link_` in `DeepLinkManager` and caches it in the
    renderer's `initial_deep_link_` field.
    -   Subsequent reads of `window.h5vcc.runtime.initialDeepLink` within that
        document context return the cached `initial_deep_link_` string.
    -   Because `initial_deep_link_` is a snapshot taken when
        `window.h5vcc.runtime` is constructed, `initialDeepLink` does **not**
        update when subsequent `kSbEventTypeLink` events arrive; applications
        must register a `'deeplink'` listener (`addEventListener('deeplink', ...)`
        or `ondeeplink`) to receive subsequent deep links.
    -   Because the stored link in `DeepLinkManager` is cleared when
        `window.h5vcc.runtime` is constructed, a consumed deep link will not be
        repeated if the page subsequently reloads or navigates. Conversely, if a
        page redirects or reloads *before* `window.h5vcc.runtime` is accessed,
        the unconsumed link remains in `DeepLinkManager` and is preserved across
        the navigation.
3.  **Real-Time Delivery via `'deeplink'` Event Listeners:**
    When the application registers a `'deeplink'` listener on
    `window.h5vcc.runtime`, `H5vccRuntime` binds a Mojo listener with
    `DeepLinkManager::AddListener`. Any deep link that arrives in
    `DeepLinkManager::OnDeepLink` while one or more listeners are registered is
    immediately dispatched over Mojo (`NotifyDeepLink`) and fired as a
    `DeepLinkEvent` (`event.url`) on `window.h5vcc.runtime`.
