# Cobalt Performance Test Tooling

This enables automated measurements of Cobalt/Chrobalt & Kimono using some tooling in
//third_party/catapult/telemetry & //third_party/catapult/devil.

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

## Usage devil_script

This automates the commands described in the ADB command section.

```
devil_script.py [-c/--is_cobalt <True/T/t/False/F/f>]
```

Note: This defaults to launching tests on Kimono, but offers a switch to use Cobalt instead.
      This is especially useful if you only need to test functionality in Cobalt and not Kimono.

## Usage Video Playback


### videoplayback_memory_graph

This captures PSS & CPU performance.
This command spits out a `cobalt_performance_data.csv`

```
python3 cobalt/tools/videoplayback_memory_monitor.py --url "https://youtube.com/tv/watch?absolute_experiments=9415422&v=1La4QzGeaaQ" --duration 30
```


### Graphing `cobalt_performance_data.csv`

```
gnuplot -e "datafile='cobalt_performance_data.csv'; outfile='cobalt_performance_data_8k60.png'; cores=4; mem_total=3869" cobalt/tools/videoplayback_memory_graph.gp
```
