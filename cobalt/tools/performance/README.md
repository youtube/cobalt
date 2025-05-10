# Cobalt Performance Test Tooling

This enables automated measurements of Cobalt/Chrobalt & Kimono using some tooling in
//third_party/catapult/telemetry.

## Loading Test Sites

### Some useful test URLs

  * https://www.youtube.com/tv#/watch?v=PMwaIrjiz8w
  * https://www.youtube.com/tv?automationRoutine=browseWatchRoutine

### ADB Commands to load these sites

  * For Cobalt 25 or older

    ```
    adb shell am force-stop dev.cobalt.coat ; sleep 1 ; adb shell am start --esa commandLineArgs '--remote-allow-origins=*,--url="https://www.youtube.com/tv?automationRoutine=browseWatchRoutine"' dev.cobalt.coat
    ```
  * For Cobalt 26 or newer

    ```
    adb shell am force-stop dev.cobalt.coat ; sleep 1 ; adb shell am start --esa commandLineArgs '--remote-allow-origins=*,--url="https://www.youtube.com/tv?automationRoutine=browseWatchRoutine"' dev.cobalt.coat
    ```

## Profiling with Telemetry Tooling

 Telemetry tooling enables the necessary tracing categories in the browser. When the browser starts, Telemetry scripts connect to the devtools' socket and send commands over this connection to pull memory dumps (among other things). When Telemetry issues a "stop tracing" command to the browser, all the memory dumps appear as part of the dumped trace (somewhere in the output directory).

### Listing out possible benchmarks

An example command for a local build of Cobalt for memory leak detection:

```
tools/perf/run_benchmark list \
--compatibility-mode=dont-require-rooted-device --browser=exact \
--device="<DEVICE_NAME>" --browser-executable=/path/to/Cobalt.apk
```

### Running a benchmark

An example command for a local build of Cobalt:

```
tools/perf/run_benchmark run <NAME_OF_BENCHMARK> \
--compatibility-mode=dont-require-rooted-device --browser=exact \
--device="<DEVICE_NAME>" --browser-executable=/path/to/Cobalt.apk
```

### Traces

TBD
