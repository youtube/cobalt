# Cobalt Deep Links

- [Cobalt Deep Links](#cobalt-deep-links)
  - [Deep Links](#deep-links)
  - [Web API](#web-api)
  - [Platform (Starboard) API](#platform-starboard-api)
  - [Behavior details](#behavior-details)
## Deep Links

For Cobalt, a deep link is a string that can be sent from the platform to an
application running in Cobalt. Generally, it can be used as any string value
signal, but typically deep links are used to specify a view, page, or content
to be shown by the application. While these strings typically are URI formatted
values, when deep link strings are received by Cobalt they are forwarded to the
running application without separate validation or modification.

Applications should interpret received deep links as superseding previous deep
links. Deep links received by Cobalt in rapid succession are not guaranteed to
all be delivered to the application. On a busy or slow device, intermediate
deep links can be dropped before they are delivered to the application.

The startup URL passed to Cobalt determines which application Cobalt will load.
Web deep links intended as a signal to the application should not be sent to
Cobalt as a startup URL because that would result in a different application
being loaded. Since a deep link is a string that may originate from an
untrusted source on the device, it should not be used directly to determine
what application Cobalt will load.

Deep links are made visible to applications by Cobalt with a Web API that is
separate from the Location interface web API. Cobalt will never directly
navigate as a result of a received deep link, even if the link matches the
current application location, for example with a query or fragment identifier.
Applications that wish to navigate as a result of incoming deep links should do
so explicitly when they are received.

## Web API

The deep link Web API consists of two parts: The
`h5vcc.runtime.initialDeepLink` property and `h5vcc.runtime.onDeepLink` event
target.

Applications can read the value of `initialDeepLink`, and/or they can use
`h5vcc.runtime.onDeepLink.addListener(foo)` to register callback functions to
be called when deep links are received.

A deep link is considered 'consumed' when it is read from `initialDeepLink`, or
when it is reported to a callback function registered to `onDeepLink`.

The IDL for this Cobalt specific interface can be found in cobalt/h5vcc, and is
repeated below.

```
interface H5vccRuntime {
  readonly attribute DOMString initialDeepLink;
  readonly attribute H5vccDeepLinkEventTarget onDeepLink;
}
interface H5vccDeepLinkEventTarget {
  void addListener(H5vccDeepLinkEventCallback callback);
};
callback H5vccDeepLinkEventCallback = void(DOMString link);
interface H5vcc {
  readonly attribute H5vccRuntime runtime;
}
```

## Platform (Starboard) API

Deep links can be passed into Cobalt in two ways:
 * As the 'Startup Link':
   * When Cobalt is first started, a deep link can be passed in with the
     initial event (either `kSbEventTypePreload` or `kSbEventTypeStart`). This
     can be achieved by calling `Application::SetStartLink` or by using and
     overload of `Application::Run` that has the 'link_data' parameter to start
     Cobalt. The value passed in there is then passed into Cobalt via the
     'link' member of the SbEventStartData event parameter, constructed in
     `Application::CreateInitialEvent`.
 * As a 'Deep Link Event':
   * At any time while Cobalt is running, it can be sent as the string value
     passed with a kSbEventTypeLink event. The `Application::Link` method can
     be called to inject such an event.

On many platforms, the 'Startup Link' value can also be set with the `--link`
command-line parameter (See `kLinkSwitch` in `Application::Run`).

The `Application` class mentioned above can be found at
`starboard/shared/starboard/application.cc`.

## Behavior details

Both the 'Startup Link' and 'Deep Link Event' values are treated the same by
Cobalt: A 'Startup Link' is treated as a 'Deep Link Event' that was received
immediately at startup. For the application, it is transparent whether a deep
link was received as a 'Startup Link' or arrived from a 'Deep Link Event'. Deep
link values of either type are made available as soon as they are known, with
the same Web API interface.

The most recently received deep link is remembered by Cobalt until it is
consumed by the application. This includes deep links received by Cobalt while
the application is still being fetched and loaded, including during page
redirects or reloads, and after the application is loaded, if it has not
consumed the deep link.

Deep link values are considered consumed when the application either reads them
from the `initialDeepLink` attribute or receives them in a callback to an
`onDeepLink` listener. An application can use either or both reads of
`initialDeepLink` or `onDeepLink` listeners to consume the most recently
received deep link value.

Calls to `onDeepLink` listeners are done as soon as deep links are available.
Specifically, they can be called before `document.onreadystatechange`,
`document.onload` & `window.onload` event handlers are called. As a result,
deep link values can be consumed in synchronously loaded JavaScript that
executes before the `document.onload` event.

Until the first `onDeepLink` listener is added, the `initialDeepLink` property
will return the most recently received deep link value. When an `onDeepLink`
listener is added, the `initialDeepLink` value will no longer be updated, even
when additional deep link events are received subsequently.

An application can decide to never register an `onDeepLink` listener and poll
the `initialDeepLink` value instead. This will then always return the value of
the most recently received deep link.

An application can decide to register an `onDeepLink` listener without reading
the `initialDeepLink` value. Upon registering, the most recently received deep
link, which may be the 'Startup Link' or from a 'Deep Link Event', will be
reported to the listener.

If a deep link value is consumed, it will not be made available again if the
page is navigated (e.g. redirected or reloaded). When a deep link is consumed
before a page redirect or reload, the deep link will not be repeated later.

If a deep link value is not consumed, it will be made available again if the
page is navigated (e.g. redirected or reloaded). A deep link will not be lost
if a page redirect or reload is done without consuming it.
