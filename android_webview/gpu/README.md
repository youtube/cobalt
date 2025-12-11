# //android\_webview/gpu/

This folder holds WebView's gpu-specific code.

WebView's gpu code always runs in the browser process.

## Folder dependencies

Like with other content embedders, `//android_webview/gpu/` can depend on
`//android_webview/common/` but not `//android_webview/browser/`. It can also
depend on content layer (and lower layers) as other embedders would (ex. can
depend on `//content/public/gpu/`, `//content/public/common/`).
