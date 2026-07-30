# Dictation Test Extension

This extension is used for testing the dictation implementation in `chrome/browser/dictation/`.

It implements a dictation provider using the dictationPrivate private extension API. When triggered by the browser, the extension returns a transcription.

This is loaded for browser tests, which can set a particular string to be returned as the transcript, to simulate speech recognition.

This can also be run interactively. To use it, run chrome with `--enable-features=Dictation --allowlisted-extension-id=dfihfgggpgemecjdjahibncmmjlfjggp` and load it as an unpacked extension. The extension's options page allows you to set a canned response as the transcription (like in the automated tests). Otherwise, the extension will use the microphone (like what a production implementation would do, but with a different recognition service).
