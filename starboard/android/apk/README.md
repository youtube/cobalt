# What is this ?

Work in progress prototype to fit a WebView based CoAT build together.

It mostly reuses existing CoAT Java code, fits into the same directory structure
and app framework. No C++ / JNI native code is used.

JavaScript speaks to Android Java code through `WebAppInterface.java`, and `h5vcc`
API polyfills are constructed via JavaScript injected into the loaded app window
( see `injected_script.js` )

# Todo list

[x] Github builds

[x] On-Host tests

[x] Android tests

[x] Split the Webview into separate class

[x] Merge existing h5vcc code into single file

[x] Make host/development server address configurable

[ ] Get platform service response plumbed though

[ ] Get user-agent fields correctly populated

[ ] Startup argument forwarding and parsing via `--esa args`

[ ] Bring over remaining missing functionality from `main` branch.

### Development Notes

APKs are built and published on Github, look in "android" build runs on the branch ( [Link](https://github.com/youtube/cobalt/actions/workflows/android.yaml?query=branch%3Afeature%2Fchrobalt_prototype+event%3Apush) )



Run a local Java test
```
./gradlew test
```

Run Android tests ( needs a connected device )
```
./gradlew connectedAndroidTest
```

https://developer.android.com/studio/test/command-line


Change development host setting:
```
adb shell am broadcast -a dev.cobalt.coat.UPDATE_SETTING --es key "development_host" --es value "192.168.1.100"
```

This dynamically injects a `http://{development_host}:8000/dyn_script.js` into the app context, for
quick turnaround prototyping. JavaScript code edits only need an app reload.
