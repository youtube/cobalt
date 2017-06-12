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
    [browse_horizontal.py](performance/non_video/browse_horizontal.py).

 2. Add the file name to the tests added within
    [performance.py](performance.py), causing it run when
    [performance](performance.py) is run.

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

### Interesting benchmarks

#### Timing-related
Some particularly interesting timing-related benchmark results are:

 - `wbStartupDurLaunchToBlankUs*`: Measures the time it takes to load
   'about:blank', which provides Cobalt's basic initialization time.
 - `wbStartupDurBlankToBrowseUs*`: Measures the time it takes Cobalt to navigate
   from 'about:blank' to the desired URL. The measurement ends when all images
   finish loading and the final startup render tree is produced. Adding
   `wbStartupDurLaunchToBlankUs*` to this time provides the full launch to
   browse startup time.
 - `wbStartupDurBlankToUsableUs*`: Measures the time it takes Cobalt to become
   fully usable when navigating from 'about:blank' to the desired URL. This
   measurement ends when all scripts finish being run, meaning that this
   includes loading the Player code JS on the main page. Adding
   `wbStartupDurLaunchToBlankUs*` to this time provides the full launch to
   usable browse startup time.
 - `wbBrowseToWatchDurVideoStartDelay*`: Measures the browse-to-watch time.
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
 - `wbBrowseHorizontalDurTotalUs*`: Same as `wbBrowseVerticalDurTotalUs*` except
   for horizontal scroll events.
 - `wbBrowseHorizontalDurAnimationsStartDelayUs*`: Same as
   `wbBrowseVerticalDurAnimationsStartDelayUs*` except for horizontal scroll
   events.
 - `wbBrowseHorizontalDurAnimationsEndDelayUs*`: Same as
   `wbBrowseVerticalDurAnimationsEndDelayUs*` except for horizontal scroll
   events.
 - `wbBrowseHorizontalDurFinalRenderTreeDelayUs*`: Same as
   `wbBrowseVerticalDurFinalRenderTreeDelayUs*` except for horizontal scroll
   events.
 - `wbBrowseHorizontalDurRasterizeAnimationsUs*`: Same as
   `wbBrowseVerticalDurRasterizeAnimationsUs*` except for horizontal scroll
   events.

In each case above, the `*` symbol can be one of either `Mean`, `Pct25`,
`Pct50`, `Pct75` or `Pct95`.  For example, `wbStartupDurBlankToBrowseUsMean` or
`wbStartupDurBlankToBrowseUsPct95` are both valid measurements. The webdriver
benchmarks runs its tests many times in order to obtain multiple samples, so you
can drill into the data by exploring either the mean, or the various
percentiles.

#### Object count-related
Some particularly interesting count-related benchmark results are:

 - `wbStartupCntDomHtmlElements*`: Lists the number of HTML elements in
   existence after startup completes. This includes HTML elements that are no
   longer in the DOM but have not been garbage collected yet.
 - `wbStartupCntDocumentDomHtmlElements*`: Lists the number of HTML
   elements within the DOM tree after startup completes.
 - `wbStartupCntLayoutBoxes*`: Lists the number of layout boxes within
   the layout tree after startup completes.
 - `wbStartupCntRenderTrees*`: Lists the number of render trees that
   were generated during startup.
 - `wbStartupCntRequestedImages*`: Lists the number of images that were
   requested during startup.
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
 - `wbBrowseVerticalCntRequestedImages*`: Lists the number of images that were
   requested as a result of the event.
 - `wbBrowseHorizontalCntDomHtmlElements*`: Same as
   `wbBrowseVerticalCntDomHtmlElements*` except for horizontal scroll events.
 - `wbBrowseHorizontalCntDocumentDomHtmlElements*`: Same as
   `wbBrowseVerticalCntDocumentDomHtmlElements*` except for horizontal scroll
   events.
 - `wbBrowseHorizontalCntLayoutBoxes*`: Same as
   `wbBrowseVerticalCntLayoutBoxes*` except for horizontal scroll events.
 - `wbBrowseHorizontalCntLayoutBoxesCreated*`: Same as
   `wbBrowseVerticalCntLayoutBoxesCreated*` except for horizontal scroll events.
 - `wbBrowseHorizontalCntRenderTrees*`: Same as
   `wbBrowseVerticalCntRenderTrees*` except for horizontal scroll events.
 - `wbBrowseHorizontalCntRequestedImages*`: Same as
   `wbBrowseVerticalCntRequestedImages*` except for horizontal scroll events.

In each case above,  the `*` symbol can be one of either `Max`, `Median`, or
`Mean`. For example, `wbBrowseVerticalCntDomHtmlElementsMax` or
`wbBrowseVerticalCntDomHtmlElementsMedian` are both valid measurements. The
webdriver benchmarks runs its tests many times in order to obtain multiple
samples, so you can drill into the data by exploring either the max, median, or
mean.

### Filtering results

The webdriver benchmarks output many metrics, but you may only be interested
in a few.  You will have to manually filter only the metrics that you are
interested in.  You can do so with `grep`, for example:

```
python performance.py -p raspi-2 -c qa -d $RASPI_ADDR > results.txt
echo "" > filtered_results.txt
grep -o "wbStartupDurLaunchToBlankUs.*$" results.txt >> filtered_results.txt
grep -o "wbStartupDurBlankToBrowseUs.*$" results.txt >> filtered_results.txt
grep -o "wbStartupDurBlankToUsableUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseToWatchDurVideoStartDelay.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalDurAnimationsStartDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalDurAnimationsEndDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalDurFinalRenderTreeDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalDurRasterizeAnimationsUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalDurAnimationsStartDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalDurAnimationsEndDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalDurFinalRenderTreeDelayUs.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalDurRasterizeAnimationsUs.*$" results.txt >> filtered_results.txt
grep -o "wbStartupCntDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbStartupCntDocumentDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbStartupCntLayoutBoxes.*$" results.txt >> filtered_results.txt
grep -o "wbStartupCntRenderTrees.*$" results.txt >> filtered_results.txt
grep -o "wbStartupCntRequestedImages.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalCntDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalCntDocumentDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalCntLayoutBoxes.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalCntRenderTrees.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseVerticalCntRequestedImages.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalCntDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalCntDocumentDomHtmlElements.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalCntLayoutBoxes.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalCntRenderTrees.*$" results.txt >> filtered_results.txt
grep -o "wbBrowseHorizontalCntRequestedImages.*$" results.txt >> filtered_results.txt
cat filtered_results.txt
```

