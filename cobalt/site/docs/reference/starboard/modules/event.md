---
layout: doc
title: "Starboard Module Reference: event.h"
---

Defines the event system that wraps the Starboard main loop and entry point.<br>
## The Starboard Application life cycle


<pre>     ---------- *
    |           |
    |        Preload
    |           |
    |           V
  Start   [ PRELOADING ] ------------
    |           |                    |
    |         Start                  |
    |           |                    |
    |           V                    |
     ----> [ STARTED ] <----         |
                |           |        |
              Pause       Unpause    |
                |           |     Suspend
                V           |        |
     -----> [ PAUSED ] -----         |
    |           |                    |
 Resume      Suspend                 |
    |           |                    |
    |           V                    |
     ---- [ SUSPENDED ] <------------
                |
               Stop
                |
                V
           [ STOPPED ]</pre><br>
The first event that a Starboard application receives is either |Start|
(kSbEventTypeStart) or |Preload| (kSbEventTypePreload). |Start| puts the
application in the |STARTED| state, whereas |Preload| puts the application in
the |PRELOADING| state.<br>
|PRELOADING| can only happen as the first application state. In this state,
the application should start and run as normal, but will not receive any
input, and should not try to initialize graphics resources (via GL or
SbBlitter). In |PRELOADING|, the application can receive |Start| or |Suspend|
events. |Start| will receive the same data that was passed into |Preload|.<br>
In the |STARTED| state, the application is in the foreground and can expect
to do all of the normal things it might want to do. Once in the |STARTED|
state, it may receive a |Pause| event, putting the application into the
|PAUSED| state.<br>
In the |PAUSED| state, the application is still visible, but has lost
focus, or it is partially obscured by a modal dialog, or it is on its way
to being shut down. The application should pause activity in this state.
In this state, it can receive |Unpause| to be brought back to the foreground
state (|STARTED|), or |Suspend| to be pushed further in the background
to the |SUSPENDED| state.<br>
In the |SUSPENDED| state, the application is generally not visible. It
should immediately release all graphics and video resources, and shut down
all background activity (timers, rendering, etc). Additionally, the
application should flush storage to ensure that if the application is
killed, the storage will be up-to-date. The application may be killed at
this point, but will ideally receive a |Stop| event for a more graceful
shutdown.<br>
Note that the application is always expected to transition through |PAUSED|
to |SUSPENDED| before receiving |Stop| or being killed.

## Enums

### SbEventType

An enumeration of all possible event types dispatched directly by the
system. Each event is accompanied by a void* data argument, and each event
must define the type of the value pointed to by that data argument, if any.

**Values**

*   `kSbEventTypePreload` - Applications should perform initialization and prepare to react tosubsequent events, but must not initialize any graphics resources (throughGL or SbBlitter). The intent of this event is to allow the application todo as much work as possible ahead of time, so that when the application isfirst brought to the foreground, it's as fast as a resume.<br>The |kSbEventTypeStart| event may be sent at any time, regardless ofinitialization state. Input events will not be sent in the |PRELOADING|state. This event will only be sent once for a given process launch.SbEventStartData is passed as the data argument.<br>The system may send |kSbEventTypeSuspend| in |PRELOADING| if it wants topush the app into a lower resource consumption state. Applications can alocall SbSystemRequestSuspend() when they are done preloading to requestthis.
*   `kSbEventTypeStart` - The first event that an application receives on startup when startingnormally (i.e. not being preloaded). Applications should performinitialization, start running, and prepare to react to subsequentevents. Applications that wish to run and then exit must call|SbSystemRequestStop()| to terminate. This event will only be sent once fora given process launch. |SbEventStartData| is passed as the dataargument. In case of preload, the |SbEventStartData| will be the same aswhat was passed to |kSbEventTypePreload|.
*   `kSbEventTypePause` - A dialog will be raised or the application will otherwise be put into abackground-but-visible or partially-obscured state (PAUSED). Graphics andvideo resources will still be available, but the application should pauseforeground activity like animations and video playback. Can only bereceived after a Start event. The only events that should be dispatchedafter a Pause event are Unpause or Suspend. No data argument.
*   `kSbEventTypeUnpause` - The application is returning to the foreground (STARTED) after having beenput in the PAUSED (e.g. partially-obscured) state. The application shouldunpause foreground activity like animations and video playback. Can only bereceived after a Pause or Resume event. No data argument.
*   `kSbEventTypeSuspend` - The operating system will put the application into a Suspended state afterthis event is handled. The application is expected to stop periodicbackground work, release ALL graphics and video resources, and flush anypending SbStorage writes. Some platforms will terminate the application ifwork is done or resources are retained after suspension. Can only bereceived after a Pause event. The only events that should be dispatchedafter a Suspend event are Resume or Stop. On some platforms, the processmay also be killed after Suspend without a Stop event. No data argument.
*   `kSbEventTypeResume` - The operating system has restored the application to the PAUSED state fromthe SUSPENDED state. This is the first event the application will receivecoming out of SUSPENDED, and it will only be received after a Suspendevent. The application will now be in the PAUSED state. No data argument.
*   `kSbEventTypeStop` - The operating system will shut the application down entirely after thisevent is handled. Can only be recieved after a Suspend event, in theSUSPENDED state. No data argument.
*   `kSbEventTypeInput` - A user input event, including keyboard, mouse, gesture, or something else.SbInputData (from input.h) is passed as the data argument.
*   `kSbEventTypeUser` - A user change event, which means a new user signed-in or signed-out, or thecurrent user changed. No data argument, call SbUserGetSignedIn() andSbUserGetCurrent() to get the latest changes.
*   `kSbEventTypeLink` - A navigational link has come from the system, and the application shouldconsider handling it by navigating to the corresponding applicationlocation. The data argument is an application-specific, null-terminatedstring.
*   `kSbEventTypeVerticalSync` - The beginning of a vertical sync has been detected. This event is verytiming-sensitive, so as little work as possible should be done on the mainthread if the application wants to receive this event in a timely manner.No data argument.
*   `kSbEventTypeNetworkDisconnect` - The platform has detected a network disconnection. The platform should makea best effort to send an event of this type when the network disconnects,but there are likely to be cases where the platform cannot detect thedisconnection (e.g. if the connection is via a powered hub which becomesdisconnected), so the current network state cannot always be inferred fromthe sequence of Connect/Disconnect events.
*   `kSbEventTypeNetworkConnect` - The platform has detected a network connection. This event may be sent atapplication start-up, and should always be sent if the network reconnectssince a disconnection event was sent.
*   `kSbEventTypeScheduled` - An event type reserved for scheduled callbacks. It will only be sent inresponse to an application call to SbEventSchedule(), and it will call thecallback directly, so SbEventHandle should never receive this eventdirectly. The data type is an internally-defined structure.
*   `kSbEventTypeAccessiblitySettingsChanged` - The platform's accessibility settings have changed. The application shouldquery the accessibility settings using the appropriate APIs to get thenew settings.
*   `kSbEventTypeLowMemory` - An optional event that platforms may send to indicate that the applicationmay soon be terminated (or crash) due to low memory availability. Theapplication may respond by reducing memory consumption by running a GarbageCollection, flushing caches, or something similar. There is no requirementto respond to or handle this event, it is only advisory.
*   `kSbEventTypeWindowSizeChanged` - The size or position of a SbWindow has changed. The data isSbEventWindowSizeChangedData.
*   `kSbEventTypeOnScreenKeyboardShown`
*   `kSbEventTypeOnScreenKeyboardHidden`

## Structs

### SbEvent

Structure representing a Starboard event and its data.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbEventType</code><br>        <code>type</code></td>    <td></td>  </tr>
</table>

### SbEventStartData

Event data for kSbEventTypeStart events.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>int</code><br>        <code>argument_count</code></td>    <td>The command-line argument count (argc).</td>  </tr>
  <tr>
    <td><code>const</code><br>        <code>char* link</code></td>    <td>The startup link, if any.</td>  </tr>
</table>

### SbEventWindowSizeChangedData

Event data for kSbEventTypeWindowSizeChanged events.

**Members**

<table class="responsive">
  <tr><th colspan="2">Members</th></tr>
  <tr>
    <td><code>SbWindow</code><br>        <code>window</code></td>    <td></td>  </tr>
  <tr>
    <td><code>SbWindowSize</code><br>        <code>size</code></td>    <td></td>  </tr>
</table>

## Functions

### SbEventCancel

**Description**

Cancels the specified `event_id`. Note that this function is a no-op
if the event already fired. This function can be safely called from any
thread, but the only way to guarantee that the event does not run anyway
is to call it from the main Starboard event loop thread.

**Declaration**

```
SB_EXPORT void SbEventCancel(SbEventId event_id);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbEventId</code><br>
        <code>event_id</code></td>
    <td> </td>
  </tr>
</table>

### SbEventHandle

**Description**

The entry point that Starboard applications MUST implement. Any memory
pointed at by `event` or the `data` field inside `event` is owned by the
system, and that memory is reclaimed after this function returns, so the
implementation must copy this data to extend its life. This behavior should
also be assumed of all fields within the `data` object, unless otherwise
explicitly specified.<br>
This function is only called from the main Starboard thread. There is no
specification about what other work might happen on this thread, so the
application should generally do as little work as possible on this thread,
and just dispatch it over to another thread.

**Declaration**

```
SB_IMPORT void SbEventHandle(const SbEvent* event);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>const SbEvent*</code><br>
        <code>event</code></td>
    <td> </td>
  </tr>
</table>

### SbEventIsIdValid

**Description**

Returns whether the given event handle is valid.

**Declaration**

```
static SB_C_FORCE_INLINE bool SbEventIsIdValid(SbEventId handle) {
  return handle != kSbEventIdInvalid;
}
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbEventId</code><br>
        <code>handle</code></td>
    <td> </td>
  </tr>
</table>

### SbEventSchedule

**Description**

Schedules an event `callback` into the main Starboard event loop.
This function may be called from any thread, but `callback` is always
called from the main Starboard thread, queued with other pending events.

**Declaration**

```
SB_EXPORT SbEventId SbEventSchedule(SbEventCallback callback,
                                    void* context,
                                    SbTime delay);
```

**Parameters**



<table class="responsive">
  <tr><th colspan="2">Parameters</th></tr>
  <tr>
    <td><code>SbEventCallback</code><br>        <code>callback</code></td>
    <td>The callback function to be called.</td>
  </tr>
  <tr>
    <td><code>void*</code><br>        <code>context</code></td>
    <td>The context that is passed to the <code>callback</code> function.</td>
  </tr>
  <tr>
    <td><code>SbTime</code><br>        <code>delay</code></td>
    <td>The minimum number of microseconds to wait before calling the
<code>callback</code> function. Set <code>delay</code> to <code>0</code> to call the callback as soon as
possible.</td>
  </tr>
</table>

