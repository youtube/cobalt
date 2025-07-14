# Cobalt CVals

## Overview

CVals are globally accessible, thread-safe, key-value pairs within Cobalt, which
are primarily used for monitoring state and tracking performance and memory.
Each CVal is defined with a unique key and registered with the global
CValManager. The CValManager can subsequently be queried for the current value
of the key, which is returned as a string. CVals come in two varieties:

*   **PublicCVals** - active in all builds.
*   **DebugCVals** - only enabled in non-Gold builds.

## Usage

### Debug Console

The debug console can be toggled between three modes (off, on, and hud) by
pressing Ctrl-O. It displays the current values of registered CVals, updating
them at 60 Hz. The initially displayed values are determined by
`DEFAULT_ACTIVE_SET` in `debug_values/console_values.js`. However, this is
modifiable in the debug console hud.

The registered CVals can be viewed by entering `debug.cvalList()` into the debug
console. Additional CVals can be registered via
`debug.cvalAdd(substringToMatch)` and can be removed via
`debug.cvalRemove(substringToMatch)`. The current registered list can be saved
as a new set via `debug.cvalSave(set_key)`. At that point, the saved set can
later be loaded as the active set via the command, `debug.cvalLoad(set_key)`.

A full list of the commands available via the debug console is shown by entering
`debug.help()`.

### JavaScript

CVals are exposed to JavaScript via the H5vccCVal object. This provides an
interface for requesting all of the keys, which are returned as an array, and
for retrieving a specific CVal value, which is returned as an optional string,
via its CVal key. While the H5vccCVal object is usable in any build, only public
CVals are queryable in Gold builds.

Here are examples of its usage:

```
  > h5vcc.cVal.keys()
  < H5vccCValKeyList[151]

  > h5vcc.cVal.keys().length
  < 151

  > h5vcc.cVal.keys().item(8)
  < "Count.MainWebModule.Layout.Box"

  > h5vcc.cVal.getValue("Count.MainWebModule.Layout.Box")
  < "463"
```

## Tracking Categories

### Cobalt

#### PublicCVals

*   **Cobalt.Lifetime** - The total number of milliseconds that Cobalt has been
    running.

#### DebugCVals

*   **Cobalt.Server.DevTools** - The IP address and port of the DevTools server,
    if it is running.
*   **Cobalt.Server.WebDriver** - The IP address and port of the Webdriver
    server, if it is running.

### Count

#### PublicCVals

*   **Count.WEB.EventListeners** - The total number of EventListeners in
    existence globally. This includes ones that are pending garbage collection.
*   **Count.DOM.Nodes** - The total number of Nodes in existence globally. This
    includes ones that are pending garbage collection.
*   **Count.MainWebModule.DOM.HtmlElement** - The total number of HtmlElements
    in the MainWebModule. This includes elements that are not in the document
    and ones that are pending garbage collection.
*   **Count.MainWebModule.DOM.HtmlElement.Document** - The total number of
    HtmlElements that are in the MainWebModule’s document.
*   **Count.MainWebModule.DOM.HtmlScriptElement.Execute** - The total number of
    HtmlScriptElement executions that have run since Cobalt started.
*   **Count.MainWebModule.Layout.Box** - The total number of Boxes that are in
    the MainWebModule’s current layout.

#### DebugCVals

*   **Count.WEB.ActiveJavaScriptEvents** - The number of JavaScript events that
    are currently running.
*   **Count.DOM.Attrs** - The total number of Attrs in existence globally. This
    includes ones that are pending garbage collection.
*   **Count.DOM.StringMaps** - The total number of StringMaps in existence
    globally. This includes ones that are pending garbage collection.
*   **Count.DOM.TokenLists** - The total number of TokenLists in existence
    globally. This includes ones that are pending garbage collection.
*   **Count.DOM.HtmlCollections** - The total number of HtmlCollections in
    existence globally. This includes ones that are pending garbage collection.
*   **Count.DOM.NodeLists** - The total number of NodeLists in existence
    globally. This includes ones that are pending garbage collection.
*   **Count.DOM.NodeMaps** - The total number of NodeMaps in existence globally.
    This includes ones that are pending garbage collection.
*   **Count.MainWebModule.[RESOURCE_CACHE_TYPE].PendingCallbacks** - The number
    of loading completed resources that have pending callbacks.
*   **Count.MainWebModule.[RESOURCE_CACHE_TYPE].Resource.Requested** - The total
    number of resources that have ever been requested.
*   **Count.MainWebModule.[RESOURCE_CACHE_TYPE].Resource.Loaded** - The total
    number of resources that have ever been successfully loaded.
*   **Count.MainWebModule.[RESOURCE_CACHE_TYPE].Resource.Loading** - The number
    of resources that are currently loading.
*   **Count.Renderer.Rasterize.NewRenderTree** - The total number of new render
    trees that have been rasterized.
*   **Count.VersionCompatibilityViolation** - The total number of Cobalt version
    compatibility violations encountered.
*   **Count.XHR** - The total number of xhr::XMLHttpRequest in existence
    globally.
*   **Count.MainWebModule.AnimatedImage.Active** - The total number of currently
    active animated image decoders. Same image from a single URL source rendered
    multiple times across the content counts as one decoder.
*   **Count.MainWebModule.AnimatedImage.DecodedFrames** - Total number of decoded
    frames across all active image decoders. This number resets only when
    WebModule gets re-created - e.g. page reload, navigation.
*   **Count.MainWebModule.AnimatedImage.DecodingUnderruns** - Total number of
    frames when animated image decoding has fallen behind real-time, as defined
    by image frame exposure times. This indicates a CPU bottleneck.
*   **Count.MainWebModule.AnimatedImage.DecodingOverruns** - Total number of
    frames when animated image decoding has been attempted too early, before
    next frame exposure time. This indicates a timing issue in platform.

### Event

The Event category currently consists of counts and durations (in microseconds)
for input events, which are tracked from when the event is first injected into
the window, until a render tree is generated from it. Each event type is tracked
separately; current event types are: *KeyDown*, *KeyUp*, *PointerDown*,
*PointerUp*.

#### PublicCVals

*   **Event.MainWebModule.[EVENT_TYPE].ProducedRenderTree** - Whether or not the
    event produced a render tree.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement** - The total
    number of HTML elements after the event produces its first render tree.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.Created** - The
    number of HTML elements created during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.Destroyed** - The
    number of HTML elements destroyed during the event. NOTE: This number will
    only be non-zero if GC occurs during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.Document** - The
    number of HTML elements in the document after the event produces its first
    render tree.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.Document.Added** -
    The number of HTML elements added to the document during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.Document.Removed** -
    The number of HTML elements removed from the document during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.UpdateMatchingRules** -
    The number of HTML elements that had their matching rules updated during the
    event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.UpdateComputedStyle** -
    The number of HTML elements that had their computed style updated during the
    event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.GenerateHtmlElementComputedStyle** -
    The number of HTML elements that had their computed style fully generated
    during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].DOM.HtmlElement.GeneratePseudoElementComputedStyle** -
    The number of pseudo elements that had their computed style fully generated
    during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box** - The total number of
    Layout boxes after the event produces its first render tree.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box.Created** - The number
    of Layout boxes that were created during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box.Destroyed** - The number
    of Layout boxes that were destroyed during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box.UpdateSize** - The
    number of times UpdateSize() is called during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box.RenderAndAnimate** - The
    number of times RenderAndAnimate() is called during the event.
*   **Event.Count.MainWebModule.[EVENT_TYPE].Layout.Box.UpdateCrossReferences** -
    The number of times UpdateCrossReferences() is called during the event.
*   **Event.Duration.MainWebModule.[EVENT_TYPE]** - The total duration of the
    event from the keypress being injected until the first render tree is
    rendered. If the event does not trigger a re-layout, then it only includes
    the event injection time.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].DOM.InjectEvent** - The time
    taken to inject the event into the window object. This mainly consists of
    JavaScript time.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].DOM.RunAnimationFrameCallbacks** -
    The time taken to run animation frame callbacks during the event. This
    mainly consists of JavaScript time.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].DOM.UpdateComputedStyle** - The
    time taken to update the computed styles of all HTML elements (which also
    includes updating their matching rules). This will track closely with the
    event DOM counts.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].Layout.BoxTree** - The time
    taken to fully lay out the box tree.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].Layout.BoxTree.BoxGeneration** -
    The time taken to generate the boxes within the box tree. This will track
    closely with the number of boxes created during the event. This is included
    within the BoxTree time.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].Layout.BoxTree.UpdateUsedSizes** -
    The time taken to update the sizes of the boxes within the box tree, which
    is when all of the boxes are laid out. This is included within the BoxTree
    time.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].Layout.RenderAndAnimate** - The
    time taken to generate the render tree produced by the event.
*   **Event.Duration.MainWebModule.[EVENT_TYPE].Renderer.Rasterize** - The time
    taken to rasterize the event’s render tree after it is submitted to the
    renderer.
*   **Event.Duration.MainWebModule.DOM.VideoStartDelay** - The delay from the
    start of the event until the start of a video. NOTE1: This is not set until
    after the event completes, so it is not included in the value dictionary.
    NOTE2: This is not tracked by event type, so each new event will reset this
    value.
*   **Event.Time.MainWebModule.[EVENT_TYPE].Start** - Time when the event
    started.

#### DebugCVals

*   **Event.MainWebModule.IsProcessing** - Whether or not an event is currently
    being processed.
*   **Event.MainWebModule.[EVENT_TYPE].ValueDictionary** - All counts and
    durations for this event as a JSON string. This is used by the
    tv_testcase_event_recorder to minimize the number of CVal requests it makes
    to retrieve all of the data for an event.

### MainWebModule

#### DebugCVals

*   **MainWebModule.IsRenderTreeRasterizationPending** - Whether or not a render
    tree has been produced but not yet rasterized.
*   **MainWebModule.Layout.IsRenderTreePending** - Whether or not the layout is
    scheduled to produce a new render tree.

### Media

#### PublicCVals

We have various SbPlayer metrics available which need to be enabled first using:
`h5vcc.settings.set("Media.EnableMetrics", 1)`

*   **Media.SbPlayerCreateTime.Minimum**
*   **Media.SbPlayerCreateTime.Median**
*   **Media.SbPlayerCreateTime.Maximum**
*   **Media.SbPlayerDestructionTime.Minimum**
*   **Media.SbPlayerDestructionTime.Median**
*   **Media.SbPlayerDestructionTime.Maximum**
*   **Media.SbPlayerWriteSamplesTime.Minimum**
*   **Media.SbPlayerWriteSamplesTime.Median**
*   **Media.SbPlayerWriteSamplesTime.Maximum**

#### DebugCVals

Media Pipeline exposes many State values, however we support multiple pipelines
at once, so we have to query for the current pipeline number to access them.
This can be done with `h5vcc.cVal.keys().filter(key =>
key.startsWith("Media.Pipeline.") && key.endsWith("MaxVideoCapabilities") &&
h5vcc.cVal.getValue(key).length === 0)` (If a Pipeline has MaxVideoCapabilities,
it implies that the Pipeline is a 10x player, i.e. a secondary small player, so
if MaxVideoCapabilities.length is 0, then it must be the primary pipeline.)
This will give back an answer like `Media.Pipeline.3.MaxVideoCapabilities` so
the main pipeline in this example is `3`. With the current pipeline number you
can then access (Switching out pipeline for the one returned from previous
query):

*   **Media.Pipeline.3.Started**
*   **Media.Pipeline.3.Suspended**
*   **Media.Pipeline.3.Stopped**
*   **Media.Pipeline.3.Ended**
*   **Media.Pipeline.3.PlayerState**
*   **Media.Pipeline.3.Volume**
*   **Media.Pipeline.3.PlaybackRate**
*   **Media.Pipeline.3.Duration**
*   **Media.Pipeline.3.LastMediaTime**
*   **Media.Pipeline.3.MaxVideoCapabilities**
*   **Media.Pipeline.3.SeekTime**
*   **Media.Pipeline.3.FirstWrittenAudioTimestamp**
*   **Media.Pipeline.3.FirstWrittenVideoTimestamp**
*   **Media.Pipeline.3.LastWrittenAudioTimestamp**
*   **Media.Pipeline.3.LastWrittenVideoTimestamp**
*   **Media.Pipeline.3.VideoWidth**
*   **Media.Pipeline.3.VideoHeight**
*   **Media.Pipeline.3.IsAudioEosWritten**
*   **Media.Pipeline.3.IsVideoEosWritten**
*   **Media.Pipeline.3.PipelineStatus**
*   **Media.Pipeline.3.CurrentCodec**
*   **Media.Pipeline.3.ErrorMessage**

### Memory

#### PublicCVals

*   **Memory.CPU.Free** - The total CPU memory (in bytes) potentially available
    to this application minus the amount being used by the application.
*   **Memory.CPU.Used** - The total physical CPU memory (in bytes) used by this
    application.
*   **Memory.GPU.Free** - The total GPU memory (in bytes) potentially available
    to this application minus the amount being used by the application. NOTE: On
    many platforms, GPU memory information is unavailable.
*   **Memory.GPU.Used** - The total physical CPU memory (in bytes) used by this
    application. NOTE: On many platforms, GPU memory information is unavailable.
*   **Memory.GPU.BufferBytes** - Currently allocated GPU memory estimate, allocated to buffer objects.
*   **Memory.GPU.TextureBytes** - Currently allocated GPU memory estimate, allocated to texture objects.
*   **Memory.GPU.RenderBufferBytes** - Currently allocated GPU memory estimate, allocated to renderbuffer objects.
*   **Memory.JS** - The total memory being used by JavaScript.
*   **Memory.Font.LocalTypefaceCache.Capacity** - The capacity in bytes of the
    font cache for use with local typefaces. This is a hard cap that can never
    be exceeded.
*   **Memory.Font.LocalTypefaceCache.Size** - The current size in bytes of the
    font cache for use with local typefaces.
*   **Memory.MainWebModule.DOM.HtmlScriptElement.Execute** - The total size in
    bytes of all scripts executed by HtmlScriptElements since Cobalt started.
*   **Memory.MainWebModule.[RESOURCE_CACHE_TYPE].Capacity** - The capacity in
    bytes of the resource cache specified by RESOURCE_CACHE_TYPE. When the
    resource cache exceeds the capacity, unused resources being purged from it.
*   **Memory.MainWebModule.[RESOURCE_CACHE_TYPE].Resource.Loaded** - The total
    size in bytes of all resources ever loaded by the resource cache specified
    by RESOURCE_CACHE_TYPE.
*   **Memory.MainWebModule.[RESOURCE_CACHE_TYPE].Size** - The total number of
    bytes currently used by the resource cache specified by RESOURCE_CACHE_TYPE.

#### DebugCVals

*   **Memory.XHR** - The total memory allocated to xhr::XMLHttpRequest objects.
*   **Memory.CachedSoftwareRasterizer.CacheUsage** - Total memory occupied by
    cached software-rasterized surfaces.
*   **Memory.CachedSoftwareRasterizer.FrameCacheUsage** - Total memory occupied
    by cache software-rasterizer surfaces that were referenced this frame.

### Renderer

#### PublicCVals

*   **Renderer.HasActiveAnimations** - Whether or not the current render tree
    has animations.
*   **Renderer.Rasterize.DurationInterval.\*** - Tracks the duration of
    intervals between rasterization calls during all animations and updates its
    stats with a new set of entries every 60 calls. Given that it only updates
    every 60 samples, it typically includes multiple animations. This provides
    an accurate view of the framerate over those samples and is the value used
    with the FPS overlay.
    *   **Renderer.Rasterize.DurationInterval.Cnt** - The number of intervals
        included in the stats. Should always be 60.
    *   **Renderer.Rasterize.DurationInterval.Avg** - The average duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.DurationInterval.Min** - The minimum duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.DurationInterval.Max** - The maximum duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.DurationInterval.Pct.25th** - The 25th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.DurationInterval.Pct.50th** - The 50th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.DurationInterval.Pct.75th** - The 75th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.DurationInterval.Pct.95th** - The 95th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.DurationInterval.Std** - The standard deviation of
        the intervals between the animation rasterizations included in the set.
*   **Renderer.Rasterize.AnimationsInterval.\*** - Tracks the duration of
    intervals between rasterization calls during a single animation and updates
    its stats when the animation ends. The stats include all of the animation’s
    rasterization intervals. This provides an accurate view of the framerate
    during the animation.
    *   **Renderer.Rasterize.AnimationsInterval.Cnt** - The number of intervals
        included in the stats. Accounts for all rasterizations that occurred
        during the animation.
    *   **Renderer.Rasterize.AnimationsInterval.Avg** - The average duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Min** - The minimum duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Max** - The maximum duration of
        the intervals between the animation rasterizations included in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Pct.25th** - The 25th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Pct.50th** - The 50th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Pct.75th** - The 75th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Pct.95th** - The 95th percentile
        duration of the intervals between the animation rasterizations included
        in the set.
    *   **Renderer.Rasterize.AnimationsInterval.Std** - The standard deviation
        of the intervals between the animation rasterizations included in the
        set.

#### DebugCVals

*   **Renderer.SubmissionQueueSize** - The current size of the renderer
    submission queue. Each item in the queue contains a render tree and
    associated animations.
*   **Renderer.ToSubmissionTime** - The current difference in milliseconds
    between the layout's clock and the renderer's clock.
*   **Renderer.Rasterize.Animations.\*** - Tracks the duration of each
    rasterization call during a single animation and updates its stats when the
    animation ends. The stats are drawn from all of the animation’s
    rasterizations. Given that this only tracks the time spent in the
    rasterizer, it does not provide as accurate a picture of the framerate as
    DurationInterval and AnimationsInterval.
    *   **Renderer.Rasterize.Animations.Cnt** - The number of rasterization
        durations included in the stats. Accounts for all rasterizations that
        occurred during the animation.
    *   **Renderer.Rasterize.Animations.Avg** - The average duration of the
        rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Min** - The minimum duration of the
        rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Max** - The maximum duration of the
        rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Pct.25th** - The 25th percentile
        duration of the rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Pct.50th** - The 50th percentile
        duration of the rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Pct.75th** - The 75th percentile
        duration of the rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Pct.95th** - The 95th percentile
        duration of the rasterizations included in the set.
    *   **Renderer.Rasterize.Animations.Std** - The standard deviation of the
        rasterization durations included in the set.

### Time

#### PublicCVals

*   **Time.Cobalt.Start** - Time when Cobalt was launched.
*   **Time.Browser.Navigate** - Time when the BrowserModule’s last Navigate
    occurred.
*   **Time.Browser.OnLoadEvent** - Time when the BrowserModule’s last
    OnLoadEvent occurred.
*   **Time.MainWebModule.DOM.HtmlScriptElement.Execute** - Time when an
    HtmlScriptElement was last executed.
*   **Time.Renderer.Rasterize.Animations.Start** - Time when the Renderer last
    started playing animations.
*   **Time.Renderer.Rasterize.Animations.End** - Time when the Renderer last
    stopped playing animations.
*   **Time.Renderer.Rasterize.NewRenderTree** - Time when the most recent render
    tree was first rasterized.

### Starboard

#### PublicCVals

*   **Starboard.FileWrite.Success** - Count of successful file writes.
*   **Starboard.FileWrite.Fail** - Count of failed file writes.
*   **Starboard.FileWrite.BytesWritten** - Bytes written to file.
*   **Starboard.StorageWriteRecord.Success** - Count of successful storage
    record writes.
*   **Starboard.StorageWriteRecord.Fail** - Count of failed storage record
    writes.
*   **Starboard.StorageWriteRecord.BytesWritten** - Bytes written to storage
    record.

### CPU

#### PublicCVals

*  **CPU.Total.Usage.IntervalSeconds.[INTERVAL_SECONDS]** - CPU usage in
   seconds during the last time interval of `INTERVAL_SECONDS`. The intervals
   are defined and enabled using `h5vcc.settings`.
```
windows.h5vcc.settings.set('cpu_usage_tracker_intervals_enabled', 1);
windows.h5vcc.settings.set('cpu_usage_tracker_intervals', [
  {type: 'total', seconds: 1},
  {type: 'total', seconds: 3},
  {type: 'total', seconds: 4},
  {type: 'total', seconds: 120},
  {type: 'per_thread', seconds: 1},
  {type: 'per_thread', seconds: 3},
  {type: 'per_thread', seconds: 5},
  {type: 'per_thread', seconds: 60}
]);
```
*  **CPU.PerThread.Usage.IntervalSeconds.[INTERVAL_SECONDS]** - CPU usage of all
   running threads. This is expressed as JSON in the following form. `id` is the
   thread ID. `name` is the thread name (not unique). `stime` is the number of
   ticks/jiffies spent in kernel mode. `utime` is the number of
   ticks/jiffies spent in user mode. `usage_seconds` is the sum of `stime` and
   `utime` converted to seconds. The usage in the last interval of
   `INTERVAL_SECONDS` can be determined by computing the difference of
   `usage_seconds` in `current` and `previous`.

```
{
  current: [
    {
      "id": 123,
      "name": "Thread Name",
      "stime": 100,
      "utime": 200,
      "usage_seconds": 3.0
    }
  ],
  previous: [
    {
      "id": 123,
      "name": "Thread Name",
      "stime": 300,
      "utime": 400,
      "usage_seconds": 7.0
    }
  ]
}
```
*  **CPU.Total.Usage.OneTime** - CPU usage in seconds between setting the
   one-time tracking on and then off.
```
window.h5vcc.settings.set('cpu_usage_tracker_one_time_tracking', 1);
...
window.h5vcc.settings.set('cpu_usage_tracker_one_time_tracking', 0);
```
*  **CPU.PerThread.Usage.OneTime** - CPU usage of all running threads. Same
   format as interval-base per-thread CPU usage CVals. The `previous` is
   captured when one-time tracking is enabled. The CVal is updated with
   `previous` and `current` when one-time tracking is disabled.
