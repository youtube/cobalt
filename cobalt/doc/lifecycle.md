# Application Lifecycle

In order to meet common needs of applications running on CE devices, Cobalt
implements a well-defined web application lifecycle, managing resources and
notifying the application as appropriate.

## Application States

Starboard Application State | Page Visibility State | Window Focused
:-------------------------- | :-------------------- | :-------------
*Preloading*                | prerender             | false
*Started*                   | visible               | true
*Paused*                    | visible               | false
*Suspended*                 | hidden                | false

### Preloading

The application is not visible, and will receive no input, but is running. Only
possible to enter as the start state. May transition to *Started* or *Suspended*
at any time.

#### Expectations for the web application

Initialize as much as possible to get to an interactive state. There is no
official signal for an application that has finished preloading.

#### Expectations for the porter

For applications that can be preloaded, the platform should send
`kSbEventTypePreload` as the first Starboard event instead of
`kSbEventTypeStart`. `src/starboard/shared/starboard/application.cc` subclasses
can opt-in to already implemented support for the `--preload` command-line
switch.

The platform should then send `kSbEventTypeStart` when the application is first
brought to the foreground. In Linux desktop (linux-x64x11), this can be done by
sending a `SIGCONT` to the process that is in the *Preloading* state.

If the platform wants to only give applications a certain amount of time to
preload, they can send `kSbEventTypeSuspend` to halt preloading and move to the
*Suspended* state. In Linux desktop, this can be done by sending SIGUSR1 to the
process that is in the *Preloading* state.

### Started

The application is running, visible, and interactive. The normal foreground
application state. May be the start state, can be entered from *Preloading*, or
*Paused*.

May only transition to *Paused*. In Linux desktop, this happens anytime the
top-level Cobalt X11 window loses focus. Linux transition back to *Started* when
the top-level Cobalt X11 window gains focus again.

### Paused

The application may be fully visible, partially visible, or completely obscured,
but it has lost input focus, so will receive no input events. It has been
allowed to retain all its resources for a very quick return to *Started*, and
the application is still running. May be entered from or transition to *Started*
or *Suspended*.

### Suspended

The application is not visible, and, once *Suspended*, will not run any
code. All graphics and media resources will be revoked until resumed, so the
application should expect all images to be lost, all caches to be cleared, and
all network requests to be aborted. The application may be terminated in this
state without notification.

#### Expectations for the web application

The application should **shut down** playback, releasing resources. On resume,
all resources need to be reloaded, and playback should be reinitialized where it
left off, or at the nearest key frame.

#### Expectations for the porter

The platform Starboard implementation **must always** send events in the
prescribed order - meaning, for example, that it should never send a
`kSbEventTypeSuspend` event unless in the *Preloading* or *Paused* states.

Currently, Cobalt does not manually stop JavaScript execution when it goes into
the *Suspended* state. In Linux desktop, it expects that a `SIGSTOP` will be
raised, causing all the threads not to get any more CPU time until resumed. This
will be fixed in a future version of Cobalt.

## Implementing the Application Lifecycle (for the porter)

Most porters will want to subclass either `starboard::shared::Application` (in
`src/starboard/shared/starboard/application.cc`) or
`starboard::shared::QueueApplication` (in
`src/starboard/shared/starboard/queue_application.cc`), as these are reference
classes that rigorously implement the Starboard application lifecycle. They are
optional, and platforms can directly dispatch events to SbEventHandle(), but it
is then up to them to ensure that events are **always** sent in the correct
state as specified in the Starboard documentation.

`starboard::shared::Application` guarantees the correct ordering by implementing
a small state machine that ignores invalid application state transitions, and
inserts any necessary transitions to make them valid. For example, you can call
`starboard::shared::Application::Suspend()`, and if you are in *Paused*, it will
just dispatch a `kSbEventTypeSuspend` event. But if you call `Suspend()` in the
*Started* state, it will dispatch `kSbEventTypePause` and then
`kSbEventTypeSuspend` events. If you call `Suspend()` in the *Suspended* state,
it just does nothing.

To control starting up in the *Preloading* state, `Application` subclasses must
override two functions:

``` c++
class MyApplication : public shared::starboard::QueueApplication {
  // [ ... ]
  bool IsStartImmediate() SB_OVERRIDE;
  bool IsPreloadImmediate() SB_OVERRIDE;
  // [ ... ]
}
```

To start up in the *Preloading* state, `IsStartImmediate()` should return
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
  bool IsStartImmediate() SB_OVERRIDE { return !HasPreloadSwitch(); }
  bool IsPreloadImmediate() SB_OVERRIDE { return HasPreloadSwitch(); }
```
