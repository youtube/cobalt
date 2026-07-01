Project: /youtube/cobalt/_project.yaml
Book: /youtube/cobalt/_book.yaml

# Starboard Module Reference: `event.h`

Defines the event system that wraps the Starboard main loop and entry point.

## The Starboard Application Lifecycle

```
               * ----------
               |           |
             Start         |
               |           |
               V           |
         [===========]     |
    ---> [  STARTED  ]     |
   |     [===========]     |
   |           |           |
 Focus       Blur      Preload
   |           |           |
   |           V           |
    ---- [===========]     |
    ---> [  BLURRED  ]     |
   |     [===========]     |
   |           |           |
 Reveal     Conceal        |
   |           |           |
   |           V           |
   |     [===========]     |
    ---- [ CONCEALED ] <---
    ---> [===========]
   |           |
Unfreeze     Freeze
   |           |
   |           V
   |     [===========]
    ---- [  FROZEN   ]
         [===========]
               |
              Stop
               |
               V
         [===========]
         [  STOPPED  ]
         [===========]

```

```
A Starboard application receives either `Start` (|kSbEventTypeStart|) or
`Preload` (|kSbEventTypePreload|) as its first event. `Start` transitions the
application to the `STARTED` state, while `Preload` transitions it to the
`CONCEALED` state.

In the `STARTED` state, the application runs in the foreground and can
perform all standard operations. From the `STARTED` state, the application
can transition to the `BLURRED` state upon receiving a `Blur` event.

In the `BLURRED` state, the application remains visible but has lost focus,
is partially obscured by a modal dialog, or is preparing to shut down. The
application should reduce activity in this state. From the `BLURRED` state,
the application can receive a `Focus` event to return to the `STARTED` state,
or a `Conceal` event to transition to the `CONCEALED` state.

In the `CONCEALED` state, the application behaves like an invisible
background process. It can still access the network and play audio, though
the platform might restrict its CPU and memory resources. The platform can
transition the application from `CONCEALED` to `FROZEN` at any time.

In the `FROZEN` state, the application is invisible. It must immediately
release all graphics and video resources, and stop all background activity
(such as timers and rendering). The application must also flush storage to
ensure data is preserved if the process is terminated. Although the platform
can terminate the process in this state, it ideally sends a `Stop` event
first for a graceful shutdown.

Note that the application always transitions through `BLURRED` and
`CONCEALED` to `FROZEN` before receiving `Stop` or being terminated.

```

## Enums

### SbEventType

An enumeration of all possible event types dispatched directly by the system.
Each event includes a `void*` data argument and must define the type of the
value it points to.

#### Values

*   `kSbEventTypePreload`

    The system can send `kSbEventTypePreload` while in the `UNSTARTED` state to
    push the app into a lower resource consumption state. Applications call
    `SbSystemRequestConceal()` to request this. Only `Reveal` or `Freeze` events
    can follow a `Preload` event. This event has no data argument.
*   `kSbEventTypeStart`

    The first event an application receives during a normal startup.
    Applications must perform initialization, start running, and prepare for
    subsequent events. To terminate, applications must call
    `SbSystemRequestStop()`. The system sends this event only once per process
    launch. The data argument contains `SbEventStartData`.
*   `kSbEventTypeBlur`

    Sent when a dialog is raised or the application enters a
    background-but-visible or partially-obscured state (`BLURRED`). Graphics and
    video resources remain available, but the application should pause
    foreground activity, such as animations and video playback. This event is
    sent only after a `Start` event. Only `Focus` or `Conceal` events can follow
    a `Blur` event. This event has no data argument.
*   `kSbEventTypeFocus`

    Sent when the application returns to the foreground (`STARTED`) from the
    `BLURRED` state. The application should resume foreground activity (such as
    animations and video playback). This event is only sent after a `Blur` or
    `Reveal` event. This event has no data argument.
*   `kSbEventTypeConceal`

    The platform transitions the application to the `CONCEALED` state after
    handling this event. The application must become invisible, though
    background tasks like audio playback and recommendation updates can continue
    running. This event is sent only after a `Blur` or `Reveal` event. Only
    `Freeze` or `Reveal` events can follow a `Conceal` event. On some platforms,
    the platform might terminate the process after a `Conceal` event without
    sending a `Freeze` event.
*   `kSbEventTypeReveal`

    The platform restores the application to the `BLURRED` state from the
    `CONCEALED` state. This is the first event the application receives when
    leaving the `CONCEALED` state, following a `Conceal` or `Unfreeze` event.
    This event has no data argument.
*   `kSbEventTypeFreeze`

    The platform transitions the application to the `FROZEN` state after
    handling this event. The application must stop periodic background work,
    release all graphics and video resources, and flush any pending `SbStorage`
    writes. This event can follow a `Conceal` or `Unfreeze` event. Only
    `Unfreeze` or `Stop` events can follow a `Freeze` event. Some platforms
    terminate the application if it performs work or retains resources after
    freezing. Also, on some platforms, the platform might terminate the process
    after a `Freeze` event without sending a `Stop` event. This event has no
    data argument.
*   `kSbEventTypeUnfreeze`

    The platform restores the application to the `CONCEALED` state from the
    `FROZEN` state. This is the first event the application receives when
    leaving the `FROZEN` state. This event can be received only after a `Freeze`
    event. This event has no data argument.
*   `kSbEventTypeStop`

    The platform shuts down the application after handling this event. This
    event is sent only in the `FROZEN` state, following a `Freeze` event. This
    event has no data argument.
*   `kSbEventTypeInput`

    A user input event, including keyboard, mouse, gesture, or something else.
    SbInputData (from input.h) is passed as the data argument.
*   `kSbEventTypeLink`

    Sent when the system delivers a navigational link. The application should
    handle it by navigating to the corresponding location. The data argument is
    an application-specific, null-terminated string.
*   `kSbEventTypeScheduled`

    Reserved for scheduled callbacks. The system sends this event only in
    response to `SbEventSchedule()`, which invokes the callback directly.
    Consequently, `SbEventHandle` never receives this event. The data type is an
    internally-defined structure.
*   `kSbEventTypeLowMemory`

    An optional event sent by platforms to indicate that the application may
    soon terminate or crash due to low memory. The application can respond by
    reducing memory usage (for example, by running garbage collection or
    flushing caches). Handling this event is optional; it is advisory only.
*   `kSbEventTypeWindowSizeChanged`

    The size or position of a SbWindow has changed. The data is
    SbEventWindowSizeChangedData.
*   `kSbEventTypeOsNetworkDisconnected`

    The platform has detected a network disconnection. There are likely to be
    cases where the platform cannot detect the disconnection but the platform
    should make a best effort to send an event of this type when the network
    disconnects. This event is used to implement window.onoffline DOM event.
*   `kSbEventTypeOsNetworkConnected`

    The platform has detected a network connection. There are likely to be cases
    where the platform cannot detect the connection but the platform should make
    a best effort to send an event of this type when the device is just
    connected to the internet. This event is used to implement window.ononline
    DOM event.
*   `kSbEventDateTimeConfigurationChanged`

    The platform has detected a date and/or time configuration change (such as a
    change in the timezone setting). This should trigger the application to
    re-query the relevant APIs to update the date and time.
*   `kSbEventTypeAccessibilityTextToSpeechSettingsChanged`

    The platform's text-to-speech settings have changed. The data field of this
    SbEvent type is a boolean indicating if text-to-speech is enabled.

## Typedefs

### SbEventCallback

A function that the main Starboard event pump can use for callbacks.

#### Definition

```
typedef void(* SbEventCallback) (void *context)
```

### SbEventDataDestructor

A function that cleanly destroys an event data instance of a specific type.

#### Definition

```
typedef void(* SbEventDataDestructor) (void *data)
```

### SbEventId

An ID that can be used to refer to a scheduled event.

#### Definition

```
typedef uint32_t SbEventId
```

## Structs

### SbEvent

This structure represents a Starboard event and its data.

#### Members

*   `SbEventType type`
*   `int64_t timestamp`
*   `void * data`

### SbEventStartData

Event data for `kSbEventTypeStart` events.

#### Members

*   `char ** argument_values`

    The command-line argument values (argv).
*   `int argument_count`

    The command-line argument count (argc).
*   `const char * link`

    The startup link, if any.

### SbEventWindowSizeChangedData

Event data for `kSbEventTypeWindowSizeChanged` events.

#### Members

*   `SbWindow window`
*   `SbWindowSize size`

## Functions

### SbEventCancel

Cancels the specified `event_id`. This function is a no-op if the event has
already fired. It can be safely called from any thread. However, to guarantee
that the event does not run, you must call it from the main Starboard event loop
thread.

#### Declaration

```
void SbEventCancel(SbEventId event_id)
```

### SbEventHandle

The entry point that Starboard applications must implement. The system owns all
memory pointed to by `event` and its `data` field, reclaiming it after this
function returns. The implementation must copy this data to extend its lifetime.
Assume this behavior for all fields within the `data` object unless explicitly
specified otherwise.

This function is called only from the main Starboard thread. Because other work
might run on this thread, the application should minimize processing here and
dispatch tasks to other threads.

#### Declaration

```
SB_EXPORT_PLATFORM void SbEventHandle(const SbEvent *event)
```

### SbEventIsIdValid

Returns whether the given event handle is valid.

#### Declaration

```
static bool SbEventIsIdValid(SbEventId handle)
```

### SbEventSchedule

Schedules an event `callback` into the main Starboard event loop. Any thread can
call this function, but `callback` is always invoked on the main Starboard
thread, queued with other pending events.

*   `callback`: The callback function to call. Must not be `NULL`.

*   `context`: The context passed to the `callback` function.

*   `delay`: The minimum number of microseconds to wait before calling the
    `callback` function. Set to `0` to call the callback as soon as possible.

#### Declaration

```
SbEventId SbEventSchedule(SbEventCallback callback, void *context, int64_t delay)
```

### SbRunStarboardMain

Serves as the entry point for running the Starboard event loop with the
application event handler in the Starboard library.

#### Declaration

```
int SbRunStarboardMain(int argc, char **argv, SbEventHandleCallback callback)
```
