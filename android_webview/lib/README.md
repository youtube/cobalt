# //android\_webview/lib/

This folder holds the native entrypoint for all WebView processes, and is
responsible for delegating to `//android_webview/browser/`,
`//android_webview/renderer/`, etc.. implementations depending on what process
is being started.

## Folder dependencies

`//android_webview/lib/` is analogous to the `app` folder in other content
embedders. As such, it is the only path allowed to depend on native code from
all processes (e.g. both `//android_webview/browser/` and
`//android_webview/renderer/`).
