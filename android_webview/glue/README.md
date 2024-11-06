# //android\_webview/glue/

This folder contains a shim layer between the public frameworks APIs
([`android.webkit.*`](https://developer.android.com/reference/android/webkit/package-summary))
and WebView's implementation, and allows them to (mostly) not directly depend
on each other.

## Folder Dependencies

`//android_webview/java/` must not depend on this directory.

## See Also

- [//android_webview/support_library/](/android_webview/support_library/README.md)
