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

> Cobalt DevTools relies heavily on V8 for its backend implementation. When
> Cobalt is built with MozJs only basic Elements and Console functionality is
> supported, but the UI remains the same as when building with V8 and some
> things don't work.

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

Cobalt has two consoles:
* Overlay console in Cobalt itself (shown with ctrl-O or F1).
* Remote console shown in a connected DevTools session.

Both console UIs show messages logged from JavaScript (with `console.log()`,
etc.), and have a command line to evaluate arbitrary JavaScript in the context
of the page being debugged.

The overlay console also shows non-JavaScript logging from Cobalt itself, which
is mostly interesting to Cobalt developers rather than web app developers.

The console in DevTools is a richer UI that can show evaluated objects with an
expander so you can dig in to their properties. Logging from JavaScript with
`console.log()` can show objects and exceptions as well, in contrast to the
text-only messages shown in the console overlay.

> There may be some things (e.g.  timers) that still need to be hooked up to the
> V8 backend, so please file a bug if something isn't working as expected.

> When built with MozJs instead of V8, the functionality of the console is
> limited to showing only text log messages.

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
