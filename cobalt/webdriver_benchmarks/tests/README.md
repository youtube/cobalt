# Cobalt Webdriver-driven Benchmarks

This directory contains a set of webdriver-driven benchmarks
for Cobalt.

Each file should contain a set of tests in Python "unittest" format.

All tests included within [performance.py](performance.py) will be run on the
build system. Results can be recorded in the build results database.

## Running the tests

In most cases, you will want to run all performance tests, and you can do so by
executing the script [performance.py](performance.py). You can call
`python performance.py --help` to see a list of commandline parameters to call
it with.  For example, to run tests on the raspi-2 QA build, you should run the
following command:

```
python performance.py -p raspi-2 -c qa -d $RASPI_ADDR
```

Where `RASPI_ADDR` is set to the IP of the target Raspberry Pi device.

To run individual tests, simply execute the script directly. For all tests,
platform configuration will be inferred from the environment if set. Otherwise,
it must be specified via commandline parameters.

## Creating a new test

 1. If appropriate, create a new file borrowing the boilerplate from
    an existing simple file, such as
    [browse_horizontal.py](performance/browse_horizontal.py).

 2. Add the file name to the tests added within
    [performance.py](performance.py), causing it run when
    [performance.py](performance.py) is run.

 3. If this file contains internal names or details, consider adding it
    to the "EXCLUDE.FILES" list.

 4. Use the `record_test_result*` methods in `tv_testcase_util` where
    appropriate.

 5. Results must be added to the build results database schema. See
    the internal
    [README-Updating-Result-Schema.md](README-Updating-Result-Schema.md) file.

## Testing against specific loaders/labels

To run the benchmarks against any desired loader, a --url command line parameter
can be provided. This will be the url that the tests will run against.

It should have the following format:

```
python performance.py -p raspi-2 -c qa -d $RASPI_ADDR --url https://www.youtube.com/tv?loader=nllive
```

## Benchmark Results

The results will be printed to stdout.  You should redirect output to a file
if you would like to store the results.  Each line of the benchmark output
prefixed with `webdriver_benchmark TEST_RESULT:` provides the result of one
measurment.  Those lines have the following format:

```
webdriver_benchmark TEST_RESULT: result_name result_value
```

where `result_name` is the name of the result and `result_value` is a number
providing the measured result for that metric.  For example,

```
webdriver_benchmark TEST RESULT: wbBrowseHorizontalDurLayoutBoxTreeUpdateUsedSizesUsPct50 3061.5
```

gives the 50th percentile of the duration Cobalt took to update the box tree's
used sizes, on a horizontal scroll event, in microseconds.

Note that most time-based measurements are in microseconds.

### Interesting Timing-Related Benchmarks
Some particularly interesting timing-related benchmark results are:

#### Startup
 - `wbStartupDurLaunchToBrowseUs`: Measures the time it takes to launch Cobalt
   and load the desired URL. The measurement ends when all images finish loading
   and the final render tree is produced. This is only run once so it will be
   noiser than other values but provides the most accurate measurement of the
   requirement startup time.
 - `wbStartupDurLaunchToUsableUs`: Measures the time it takes to launch Cobalt,
   and fully load the desired URL, including loading all scripts. The
   measurement ends when the Player JavaScript code finishes loading on the
   Browse page, which is the point when Cobalt is fully usable. This is only run
   once so it will be noiser than other values but provides the most accurate
   measurement of the time when Cobalt is usable.
 - `wbStartupDurNavigateToBrowseUs*`: Measures the time it takes to navigate to
   the desired URL when Cobalt is already loaded. The measurement ends when all
   images finish loading and the final render tree is produced. This is run
   multiple times, so it will be less noisy than `wbStartupDurLaunchToBrowseUs`,
   but it does not include Cobalt initialization so it is not as accurate of a
   measurement.
 - `wbStartupDurNavigateToUsableUs*`: Measures the time it takes to navigate to
   the desired URL when Cobalt is already loaded, including loading all scripts.
   The measurement ends when the Player JavaScript code finishes loading on the
   Browse page, which is the point when Cobalt is fully usable. This is run
   multiple times, so it will be less noisy than `wbStartupDurLaunchToUsableUs`,
   but it does not include Cobalt initialization so it is not as accurate of a
   measurement.

#### Browse Horizontal Scroll Events (Tile-to-Tile)
 - `wbBrowseHorizontalDurTotalUs*`: Measures the latency (i.e. JavaScript
   execution time + layout time) during horizontal scroll events from keypress
   until the render tree is submitted to the rasterize thread. It does not
   include the time taken to rasterize the render tree so it will be smaller
   than the observed latency.
 - `wbBrowseHorizontalDurAnimationsStartDelayUs*`: Measures the input latency
   during horizontal scroll events from the keypress until the animation starts.
   This is the most accurate measure of input latency.
 - `wbBrowseHorizontalDurAnimationsEndDelayUs*`: Measures the latency during
   horizontal scroll events from the keypress until the animation ends.
 - `wbBrowseHorizontalDurFinalRenderTreeDelayUs*`: Measures the latency during
   horizontal scroll events from the keypress until the final render tree is
   rendered and processing stops.
 - `wbBrowseHorizontalDurRasterizeAnimationsUs*`: Measures the time it takes to
   render each frame of the animation triggered by a horizontal scroll event.
   The inverse of this number is the framerate.

#### Browse Vertical Scroll Events (Shelf-to-Shelf)
 - `wbBrowseVerticalDurTotalUs*`: Measures the latency (i.e. JavaScript
   execution time + layout time) during vertical scroll events from keypress
   until the render tree is submitted to the rasterize thread. It does not
   include the time taken to rasterize the render tree so it will be smaller
   than the observed latency.
 - `wbBrowseVerticalDurAnimationsStartDelayUs*`: Measures the input latency
   during vertical scroll events from the keypress until the animation starts.
   This is the most accurate measure of input latency.
 - `wbBrowseVerticalDurAnimationsEndDelayUs*`: Measures the latency during
   vertical scroll events from the keypress until the animation ends.
 - `wbBrowseVerticalDurFinalRenderTreeDelayUs*`: Measures the latency during
   vertical scroll events from the keypress until the final render tree is
   rendered and processing stops.
 - `wbBrowseVerticalDurRasterizeAnimationsUs*`: Measures the time it takes to
   render each frame of the animation triggered by a vertical scroll event.
   The inverse of this number is the framerate.

#### Browse-to-Watch
 - `wbBrowseToWatchDurVideoStartDelayUs*`: Measures the browse-to-watch time.
 - `wbBrowseToWatchDurInitialRenderTreeDelayUs*`: Measures the latency from the
   start of the event until the screen turning black prior to the video
   starting.
 - `wbBrowseToWatchDurSpinnerStartDelayUs*`: Measures the latency from the start
   of the event until the spinner appears on the screen (is 0 if the spinner
   never appears).
 - `wbBrowseToWatchDurVideoSpinnerDelayUs*`: Measures the delay after the video
   starts for the spinner to disappear from the screen (is 0 if the spinner
   never appears).

In each case above, the `*` symbol can be one of either `Mean`, `Pct25`,
`Pct50`, `Pct75` or `Pct95`.  For example, `wbStartupDurBlankToBrowseUsMean` or
`wbStartupDurBlankToBrowseUsPct95` are both valid measurements. The webdriver
benchmarks runs its tests many times in order to obtain multiple samples, so you
can drill into the data by exploring either the mean, or the various
percentiles.

### Interesting Count-Related Benchmarks
Some particularly interesting count-related benchmark results are:

#### Startup
 - `wbStartupCntDomHtmlElements*`: Lists the number of HTML elements in
   existence after startup completes. This includes HTML elements that are no
   longer in the DOM but have not been garbage collected yet.
 - `wbStartupCntDocumentDomHtmlElements*`: Lists the number of HTML
   elements within the DOM tree after startup completes.
 - `wbStartupCntLayoutBoxes*`: Lists the number of layout boxes within
   the layout tree after startup completes.
 - `wbStartupCntRenderTrees*`: Lists the number of render trees that were
   generated during startup.
 - `wbStartupCntLoadedImages*`: Lists the number of images that were
   loaded during startup.
 - `wbStartupCntLoadedFonts*`: Lists the number of web fonts that were
   loaded during startup.

#### Browse Horizontal Scroll Events (Tile-to-Tile)
 - `wbBrowseHorizontalCntDomHtmlElements*`: Lists the number of HTML elements in
   existence after the event. This includes HTML elements that are no longer in
   the DOM but have not been garbage collected yet.
 - `wbBrowseHorizontalCntDocumentDomHtmlElements*`: Lists the number of HTML
   elements within the DOM tree after the event.
 - `wbBrowseHorizontalCntLayoutBoxes*`: Lists the number of layout boxes within
   the layout tree after the event.
 - `wbBrowseHorizontalCntLayoutBoxesCreated*`: Lists the number of new layout
   boxes that were created during the event.
 - `wbBrowseHorizontalCntRenderTrees*`: Lists the number of render trees that
   were generated by the event.
 - `wbBrowseHorizontalCntLoadedImages*`: Lists the number of images that were
   loaded as a result of the event.

#### Browse Vertical Scroll Events (Shelf-to-Shelf)
 - `wbBrowseVerticalCntDomHtmlElements*`: Lists the number of HTML elements in
   existence after the event. This includes HTML elements that are no longer in
   the DOM but have not been garbage collected yet.
 - `wbBrowseVerticalCntDocumentDomHtmlElements*`: Lists the number of HTML
   elements within the DOM tree after the event.
 - `wbBrowseVerticalCntLayoutBoxes*`: Lists the number of layout boxes within
   the layout tree after the event.
 - `wbBrowseVerticalCntLayoutBoxesCreated*`: Lists the number of new layout
   boxes that were created during the event.
 - `wbBrowseVerticalCntRenderTrees*`: Lists the number of render trees that
   were generated by the event.
 - `wbBrowseVerticalCntLoadedImages*`: Lists the number of images that were
   loaded as a result of the event.

#### Browse-to-Watch Events
 - `wbBrowseToWatchCntLoadedImages*`: Lists the number of images that were
   loaded as a result of the event.

In each case above,  the `*` symbol can be one of either `Max`, `Median`, or
`Mean`. For example, `wbBrowseVerticalCntDomHtmlElementsMax` or
`wbBrowseVerticalCntDomHtmlElementsMedian` are both valid measurements. The
webdriver benchmarks runs its tests many times in order to obtain multiple
samples, so you can drill into the data by exploring either the max, median, or
mean.

### Interesting Memory-Related Benchmarks
Some particularly interesting memory-related benchmark results are:

#### Startup
 - `wbStartupMemLoadedImages*`: Lists the image cache memory in bytes taken up
   by all images loaded during startup.
 - `wbStartupMemLoadedFonts*`: Lists the font cache memory in bytes taken up by
   all web fonts loaded during startup.

#### Browse Horizontal Scroll Events (Tile-to-Tile)
 - `wbBrowseHorizontalMemLoadedImages*`: Lists the image cache memory in bytes
   taken up by all images loaded as a result of the event.

#### Browse Vertical Scroll Events (Shelf-to-Shelf)
 - `wbBrowseVerticalMemLoadedImages*`: Lists the image cache memory in bytes
   taken up by all images loaded as a result of the event.

#### Browse-to-Watch Events
 - `wbBrowseToWatchMemLoadedImages*`: Lists the image cache memory in bytes
   taken up by all images loaded as a result of the event.

In each case above, the `*` symbol can be one of either `Mean`, `Pct25`,
`Pct50`, `Pct75` or `Pct95`.  For example, `wbBrowseToWatchMemLoadedImagesMean`
or `wbBrowseToWatchMemLoadedImagesPct95` are both valid measurements. The
webdriver benchmarks runs its tests many times in order to obtain multiple
samples, so you can drill into the data by exploring either the mean, or the
various percentiles.

### Filtering results

The webdriver benchmarks output many metrics, but you may only be interested
in a few.  You will have to manually filter only the metrics that you are
interested in.  You can do so with `grep`, for example:

```
python performance.py -p raspi-2 -c qa -d $RASPI_ADDR > results.txt
echo "" > filtered_results.txt
printf "=================================TIMING-RELATED=================================\n" >> filtered_results.txt
printf "\nSTARTUP\n" >> filtered_results.txt
grep -o "wbStartupDurLaunchToBrowseUs.*$" results.txt >> filtered_results.txt
grep -o "wbStartupDurLaunchToUsableUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupDurNavigateToBrowseUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupDurNavigateToUsableUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE HORIZONTAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalDurTotalUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalDurAnimationsStartDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalDurAnimationsEndDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalDurFinalRenderTreeDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalDurRasterizeAnimationsUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE VERTICAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseVerticalDurTotalUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalDurAnimationsStartDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalDurAnimationsEndDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalDurFinalRenderTreeDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalDurRasterizeAnimationsUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE TO WATCH\n" >> filtered_results.txt
grep -o "wbBrowseToWatchDurVideoStartDelay.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseToWatchDurInitialRenderTreeDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseToWatchDurSpinnerStartDelayUs.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseToWatchDurVideoSpinnerDelayUs.*$" results.txt >> filtered_results.txt
printf "\n\n=================================COUNT-RELATED==================================\n" >> filtered_results.txt
printf "\nSTARTUP\n" >> filtered_results.txt
grep -o "wbStartupCntDomHtmlElements.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupCntDomDocumentHtmlElements.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupCntLayoutBoxes.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupCntRenderTrees.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupCntLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupCntLoadedFonts.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE HORIZONTAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntDomHtmlElementsM.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntDomDocumentHtmlElements.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntLayoutBoxesM.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntLayoutBoxesCreated.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntRenderTrees.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalCntLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE VERTICAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntDomHtmlElementsM.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntDomDocumentHtmlElements.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntLayoutBoxesM.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntLayoutBoxesCreated.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntRenderTrees.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbBrowseVerticalCntLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE TO WATCH\n" >> filtered_results.txt
grep -o "wbBrowseToWatchCntLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n\n=================================MEMORY-RELATED=================================\n" >> filtered_results.txt
printf "\nSTARTUP\n" >> filtered_results.txt
grep -o "wbStartupMemLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
grep -o "wbStartupMemLoadedFonts.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE HORIZONTAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseHorizontalMemLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE VERTICAL SCROLL EVENTS\n" >> filtered_results.txt
grep -o "wbBrowseVerticalMemLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
printf "\nBROWSE TO WATCH\n" >> filtered_results.txt
grep -o "wbBrowseToWatchMemLoadedImages.*$" results.txt >> filtered_results.txt
printf "\n" >> filtered_results.txt
cat filtered_results.txt
```
