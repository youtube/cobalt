---
layout: doc
title: "Application Lifecycle"
---
# Application Lifecycle

In order to meet common needs of applications running on CE devices, Cobalt
implements a well-defined web application lifecycle, managing resources and
notifying the application as appropriate.

## Summary of changes in Cobalt 22

The application lifecycle has some changes from Cobalt 22:

### States:

* The *Paused* state is renamed to *Blurred*.
* The *Suspended* state is replaced by *Concealed* and *Frozen*.
* The *Preloading* state is removed, and *Concealed* is used instead.
  Note: The corresponding attribute value 'prerender' for
  document.visibilityState is also removed.

The new *Concealed* state is used for applications that are not visible but may
use CPU or network resources. This state is used to both replace the
*Preloading* state, and as an intermediate state between *Blurred* and
*Frozen*.

The *Frozen* state most closely resembles the previous *Suspended* state,
during which applications do not have network access.

### State Changes:

* The *Pause* event is renamed to *Blur*.
* The *Unpause* event is renamed to *Focus*.
* The *Suspend* event is replaced by *Conceal* and *Freeze*.
* The *Resume* event is replaced by *Unfreeze* and *Reveal*.

Most platforms should only need to replace 'Pause' with 'Blur', 'Unpause' with
'Focus', 'Suspend' with 'Freeze', and 'Resume' with 'Reveal'.

Since there is no longer a special *Preloading* state, applications should no
longer use the *Start* event when a preloaded application is brought to the
foreground. Instead, the same event(s) used for backgrounded applications
(*Concealed* or *Frozen*) should be used.

### Application 'Backgrounding' and 'Foregrounding'.

To signal that the application is being 'backgrounded', the use of *Suspend*
should be replaced with *Freeze*.

To signal that the application is being 'foregrounded', the use of *Unpause*
should be replaced with *Focus*.

Note: If a platform is using *Resume* (*Reveal*) to signal that an application
is being 'foregrounded', then that may result in unexpected application
behavior, unless a subsequent *Unpause* (*Focus*) is also used when the
application receives input focus.

More details about lifecycle states and state changes can be found in
`src/starboard/event.h`.

### Deprecated `SbEventType` values.

The `SbEventType` enum is defined in `src/starboard/event.h`.

* The `kSbEventTypePause` value is renamed to `kSbEventTypeBlur`.
* The `kSbEventTypeUnpause` value is renamed to `kSbEventTypeFocus`.
* The `kSbEventTypeSuspend` value is replaced by `kSbEventTypeConceal` and
  `kSbEventTypeFreeze`.
* The `kSbEventTypeResume` value is replaced by `kSbEventTypeUnfreeze` and
  `kSbEventTypeReveal`.

The corresponding helper functions in
`starboard::shared::starboard::Application` (implemented in
`starboard/shared/starboard/application.cc`) that inject events with these
values have been updated correspondingly:

* The `Pause()` method is renamed to `Blur()`.
* The `Unpause()` method is renamed to `Focus()`.
* The `Suspend()` method is replaced by `Conceal()` and
  `Freeze()`.
* The `Resume()` method is replaced by `Unfreeze()` and
  `Reveal()`.

Platforms that inject events themselves should be updated to use renamed event
type values, and platforms that use the helper functions should be updated to
call the corresponding renamed helper functions.

### Deprecated `SbSystemRequest` functions.

The `SbSytemRequest` functions are declared in `src/starboard/system.h`

* The `SbSystemRequestPause` event is renamed to `SbSystemRequestBlur`
* The `SbSystemRequestUnpause` event is renamed to `SbSystemRequestFocus`
* The `SbSystemRequestSuspend` event is replaced by `SbSystemRequestConceal`
  and `SbSystemRequestFreeze`
* The `SbSystemRequestResume` event is replaced by `SbSystemRequestUnfreeze`
  and `SbSystemRequestReveal`

## Application States

Starboard Application State | Page Visibility State | Window Focused
:-------------------------- | :-------------------- | :-------------
*Started*                   | visible               | true
*Blurred*                   | visible               | false
*Concealed*                 | hidden                | false
*Frozen*                    | hidden                | false

When transitioning between *Concealed* and *Frozen*, the document.onfreeze and
document.onresume events from the Page LifeCycle Web API will be dispatched.

### Started

The application is running, visible, and interactive. The normal foreground
application state. May be the start state, or can be entered from *Blurred*.

May only transition to *Blurred*. In Linux desktop, this happens anytime the
top-level Cobalt X11 window loses focus. Linux transition back to *Started*
when the top-level Cobalt X11 window gains focus again.

### Blurred

The application may be fully visible, partially visible, or completely
obscured, but it has lost input focus, so will receive no input events. It has
been allowed to retain all its resources for a very quick return to *Started*,
and the application is still running. May be entered from or transition to
*Started* or *Concealed* at any time.

### Concealed

The application is not visible and will receive no input, but is running. Can
be entered as the start state. May be entered from or transition to *Blurred*
or *Frozen* at any time. The application may be terminated in this state
without notification.

Upon entering, all graphics resources will be revoked until revealed, so the
application should expect all images to be lost, and all caches to be cleared.

#### Expectations for the web application

The application should **shut down** playback, releasing resources. On resume,
all resources need to be reloaded, and playback should be reinitialized where
it left off, or at the nearest key frame.

### Frozen

The application is not visible and will receive no input, and, once *Frozen*,
will not run any code. May be entered from or transition to *Concealed* at any
time. The application may be terminated in this state without notification.

Upon entering, all graphics and media resources will be revoked until resumed,
so the application should expect all images to be lost, all caches to be
cleared, and all network requests to be aborted.

#### Expectations for the porter

Currently, Cobalt does not manually stop JavaScript execution when it goes into
the *Frozen* state. In Linux desktop, it expects that a `SIGSTOP` will be
raised, causing all the threads not to get any more CPU time until resumed.
This will be fixed in a future version of Cobalt.

### Application Startup Expectations for the porter

The starboard application lifecycle, with descriptions of the states and the
state changes can be found in `src/starboard/event.h`.

For applications that can be preloaded, the platform should send
`kSbEventTypePreload` as the first Starboard event instead of
`kSbEventTypeStart`. Subclasses of
`src/starboard/shared/starboard/application.cc` can opt-in to use the already
implemented support for the `--preload` command-line switch.

If started with `kSbEventTypePreload`, the platform can at any time send
`kSbEventTypeFocus` when the application brought to the foreground.
In Linux desktop (linux-x64x11), this can be done by sending a `SIGCONT` to the
process that is in the *Preloading* state (see
`starboard/shared/signal/suspend_signals.cc`)

If the platform wants to only give applications a certain amount of time to
preload, they can send `SbSystemRequestFreeze` to halt preloading and move to
the *Frozen* state. In Linux desktop, this can be done by sending SIGUSR1 to
the process that is in the *Preloading* state.

## Implementing the Application Lifecycle (for the porter)

The platform Starboard implementation **must always** send events in the
prescribed order - meaning, for example, that it should never send a
`kSbEventTypeConceal` event unless in the *Blurred* state.

Most porters will want to subclass either `starboard::shared::Application` (in
`src/starboard/shared/starboard/application.cc`) or
`starboard::shared::QueueApplication` (in
`src/starboard/shared/starboard/queue_application.cc`), as these are reference
classes that rigorously implement the Starboard application lifecycle. They are
optional, and platforms can directly dispatch events to SbEventHandle(), but it
is then up to them to ensure that events are **always** sent in the correct
state as specified in the Starboard documentation.

`starboard::shared::Application` (in
`starboard/shared/starboard/application.cc`) guarantees the correct ordering by
implementing a small state machine that ignores invalid application state
transitions, and inserts any necessary transitions to make them valid. For
example, you can call `starboard::shared::Application::Conceal()`, and if you
are in *Blurred*, it will just dispatch a `kSbEventTypeConceal` event. But if
you call `Conceal()` in the *Started* state, it will first dispatch
`kSbEventTypeBlur`, followed by a `kSbEventTypeConceal` event. If you call
`Conceal()` in the *Concealed* state, it just does nothing.

This behavior can be ensured by only dispatching events to SbEventHandle()
using `Application::DispatchAndDelete()` either directly, or indirectly such
as by using `Application::RunLoop()` with the default implementation of
`Application::DispatchNextEvent()`.

To control starting up in the *Concealed* state for preloading, `Application`
subclasses must override two functions:

``` c++
class MyApplication : public shared::starboard::QueueApplication {
  // [ ... ]
  bool IsStartImmediate() override;
  bool IsPreloadImmediate() override;
  // [ ... ]
}
```

To start up in the *Concealed* state, `IsStartImmediate()` should return
`false` and `IsPreloadImmediate()` should return `true`.

To start up in the *Starting* state (which is the default), `IsStartImmediate()`
should return `true` and `IsPreloadImmediate()` will not be called.

To delay starting up until some later event, `IsStartImmediate()` and
`IsPreloadImmediate()` should both return `false`. No initial event will be
automatically sent to the application, and it is then up to the porter to
dispatch a `kSbEventTypeStart` or `kSbEventTypePreload` event as the first
event. This is useful if you need to wait for an asynchronous system activity to
complete before starting Cobalt.

To support the `--preload` command-line argument:

``` c++
  bool IsStartImmediate() override { return !HasPreloadSwitch(); }
  bool IsPreloadImmediate() override { return HasPreloadSwitch(); }
```
