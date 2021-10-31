---
layout: doc
title: "Cobalt Web Debugging"
---
# Cobalt Web Debugging

## Overview

Cobalt includes the [Chrome
DevTools](https://developers.google.com/web/tools/chrome-devtools/) frontend for
debugging web apps. It's available in the **20.lts.1+** and newer branches of
Cobalt.

Cobalt only supports a subset of what DevTools can do, but we make a point of
hiding UI elements that don't work so everything you see in the UI should work.
As we get more implemented in the backend the respective UI will be enabled in
the frontend.

The following panels are supported:

*   **Elements** - view the DOM tree and CSS styles
*   **Console** - JavaScript "command line" and logging output
*   **Sources** - interactive JavaScript debugging
*   **Performance** - profile JavaScript execution

## Using DevTools

The DevTools frontend is loaded in Chrome from a small HTTP server built into
**non-gold** Cobalt. Even though it looks mostly the same as Chrome's inspector
(it's built from the same source code), Cobalt's DevTools is a separate app,
and Cobalt is *not* a remote target that you can debug with Chrome's built-in
debugger.

After building and running Cobalt as usual, use Chrome on your desktop to load
the start page from port 9222 on the target device where Cobalt is running.
Click through to the only inspectable page shown on the start page.

> If you have trouble connecting:
> * Ensure you have an IP route from your desktop to the target device that
>   allows traffic on the debugging port (default 9222).
> * If you are running Cobalt locally on your desktop, then use
>   http://localhost:9222 since the Linux build only listens to the loopback
>   network interface by default.

If you're not sure what IP address to use, look in the terminal log output for a
message telling you the URL of Cobalt's DevTools (which you may be able to open
with a ctrl-click in many terminal programs):

```
---------------------------------
 Connect to the web debugger at:
 http://192.168.1.1:9222
---------------------------------
```

### Wait for web debugger

If you're debugging the initial page as it's loading you need use the
`--wait_for_web_debugger` switch to tell Cobalt to wait until you attach
DevTools before actually loading the initial URL:

```
out/linux-x64x11_devel/cobalt --wait_for_web_debugger --url="http://test.example.com"
```

When this switch is specified, Cobalt will appear to hang with just a black
window until you load DevTools. In the terminal log output you'll see that
Cobalt is waiting with message like:

```
-------------------------------------
 Waiting for web debugger to connect
-------------------------------------
```

If you're debugging a page in a series of redirects, you can specify a number to
make Cobalt wait before loading the Nth page. If no number is specified with the
switch, the default value is 1 to wait before the initial page load. For
example:

```
out/linux-x64x11_devel/cobalt --wait_for_web_debugger=2 --url="http://test.example.com"
```

## Panels

### Elements

The Elements panel displays the DOM as a tree with expandable nodes to dig into
it. The right side bar shows the CSS styles affecting the selected node in the
DOM. The *Styles* tab shows matching rules, inherited rules, and inline style.
The *Computed* tab shows the computed style for the selected node. The box model
properties are shown graphically in both the *Styles* and *Computed* tabs.

> Cobalt currently only supports a read-only view of the DOM and CSS.

Chrome docs:

*   https://developers.google.com/web/tools/chrome-devtools/dom/
*   https://developers.google.com/web/tools/chrome-devtools/css/

### Console

Cobalt has two types of consoles:

*   Overlay Console: shown at runtime of Cobalt. It has multiple mode that it
    can cycle between as well:
    *   HUD
    *   HUD & Debug Console
    *   Media Console
*   Remote Console: shown in a connected devtools session.

Both console UIs show messages logged from JavaScript (with `console.log()`,
etc.), and have a command line to evaluate arbitrary JavaScript in the context
of the page being debugged.

#### Overlay Console

The overlay console also shows non-JavaScript logging from Cobalt itself, which
is mostly interesting to Cobalt developers rather than web app developers.

The various modes of the overlay console are accessed by repeatedly pressing
"`F1`" or "`Ctrl+O`". They cycle in order between: none, HUD, HUD & Debug, and
Media. Alternatively, initial console state can be set with the
`--debug_console=off|hud|debug|media` command-line switch (`--debug_console=on`
is accepted as a legacy option and maps to "debug" setting).

![Overlay Console mode switching](resources/devtools-overlay-console-flow.png)

##### HUD overlay

This brings up an overlay panel which does not block sending input to the
underlying Cobalt app. It serves to display real-time statistics (e.g. memory
usage) and configuration values (e.g. disabled codecs) of the Cobalt app in a
compact string.

##### Debug Console overlay

This overlay is interactive and it shows messages from Cobalt, along with logs
from Javascript `console.log()`. While it is active, you cannot interact
directly with the underlying page.

Additionally, it can act as a JS interpreter that will evaluate arbitrary
expressions on the page being debugged. The output from these JS commands will
also be printed to the Debug console.

Finally, it has some special debug commands which can be listed by calling
`d.help()`. They are provided by a debug helper object and the list of functions
are invoked by prepending either "`debug`" or "`d`". For example, you can
disable the vp9 codec manually for all future played videos in this session of
Cobalt by sending `debug.disable_media_codecs("vp9")` to the console.

Note: you can clear the disabled media codecs by sending
`debug.disable_media_codecs("")`. The command takes a semicolon separated list
of codecs as the input list of codecs to disable.

##### Media Console overlay

The media console is a specialized console of the debug overlay system, for
playback and media related tasks. The current list of implemented features are:

*   Reading the play/pause state of the primary video
*   Reading the current time and duration of the primary video
*   Reading the playback rate of the primary video
*   Reading the currently disabled codecs for the player
*   Toggling between playing and pausing the primary video
*   Setting the current playback rate between various presets for the primary
    video
*   Toggling the enabled/disabled state of the available codecs

While the media console is shown, it is not possible to interact with the page
below it directly.

Additionally, the console does not show any meaningful information or
interactions when no video is currently playing (all the readouts are blank or
undefined). A status message of “No primary video.” indicates there is no valid
player element on the current page.

In the case of multiple videos playing (such as picture in picture), only the
primary (fullscreen) video’s information is shown and the controls are only
enabled for the primary video.

The list of hotkeys and commands are dynamically generated as they are found to
be available on app startup.

Basic always-enabled commands are (case-sensitive):

*   "`p`" Toggle the play/pause state
*   "`]`" Increase the playback rate
*   "`[`" Decrease the playback rate

The above commands will take effect instantly for the currently playing video.
They have no effect if there is no video playing.

The following commands are dynamically loaded based on the capability of the
system:

*   "`CTRL+NUM`" Enable/disable specific video codec
*   "`ALT+NUM`" Enable/disable specific audio codec

**Important:** Media Console cannot be used to directly select a specific codec for
playback. See the section below for rough outline of steps to work around this.

The list of available codecs for any video is chosen based on the decoders on
the platform, and what formats YouTube itself serves. As a result, the only way
to get a particular codec to play is to disable all the options until the
desired codec is the one that is picked. Simply do the following procedure:

*   Pick the video you want to play.
*   Enable “stats for nerds” (See [help page for
    instructions](https://support.google.com/youtube/answer/7519898)).
*   Write down the codecs that are chosen when playing the video, without any
    codecs disabled (one for video, and one for audio).
*   Disable the default codecs.
*   Replay the same video from the browse screen.
*   Repeat until you identify all codecs that are available for the video, until
    the video is unable to be played.
*   Use the above knowledge to disable the codecs to force the player into
    choosing a particular codec, by process of elimination.

**Important:** Disabled codecs only take effect when a video starts playing.
When you play a video, the current list of disabled codecs is used to select an
arbitrary enabled format. When you seek in the video, the disabled codecs list
does not take effect. Only when you exit the player and re-enter by playing a
video will any toggled codecs be affected.

**Important:** Disabled codecs list is persistent for the app-run. If you
disable “av01”, then until you re-enable it, “av01” formats will never be
chosen.

**Important:** If you disable all the available codecs, no video codec can be
selected and an error dialog will be shown. This means that YouTube does not
have the video in any other formats, outside of the codecs that are disabled.
The player reports that it cannot play the video in any of the available formats
so playback will fail here, which is intended.

#### Remote Console

The console in DevTools is a richer UI that can show evaluated objects with an
expander so you can dig in to their properties. Logging from JavaScript with
`console.log()` can show objects and exceptions as well, in contrast to the
text-only messages shown in the console overlay.

Chrome docs:

*   https://developers.google.com/web/tools/chrome-devtools/console/

### Sources

Source-level JavaScript debugging can be done on the Sources panel. You can
inspect sources, set breakpoints, see call stacks and scoped variables, add
watch expressions, and all that good stuff.

> Source debugging only works when Cobalt is built with V8.

Chrome docs:

*   https://developers.google.com/web/tools/chrome-devtools/javascript/

### Performance

The Performance panel allows you to record a profile of sampled call stacks
while Cobalt is running your JavaScript code. The recorded profile is displayed
in a flame chart showing the relationship and timing of function calls.

> Performance profiling only works when Cobalt is built with V8, and the
> platform implements the `SbThreadSampler` Starboard API.

> The profiler can't currently identify which is the main thread, but you can
> easily see it as the one with the most events.

Chrome docs:

*   https://developers.google.com/web/tools/chrome-devtools/evaluate-performance/

## Tips

*   You can make Cobalt reload the current page by pressing F5 in the Cobalt
    window, or ctrl-R in the remote DevTools. This may be useful for debugging
    startup code in the web app. It may also help in case some source file is
    not appearing in the DevTools Sources panel.

*   The DevTools frontend remembers your breakpoints, so if you need to restart
    Cobalt completely you can just kill it with ctrl-C in the terminal where you
    launched it and re-run it. Then click the *Reconnect DevTools* button shown
    in the DevTools UI or refresh the page to reload the DevTools UI.

*   You can use the `--remote_debugging_port` command line switch to specify a
    remote debugging port other than the default 9222.

*   You can use the `--dev_servers_listen_ip` command line switch to change
    which network interface the remote debugging server is listening to.
