# Cobalt Performance Test Tooling

This enables performance monitoring via ADB commands and dumpsys meminfo
and plots the results using matplotlib.

## Android Monitoring Script

### Setup

This script requires a few dependencies - pandas, matplotlib, numpy, and
pylint.

To install them, run the following command:

```
sudo apt install python3-pandas python3-numpy python3-matplotlib 
```

### Usage

The monitoring script has various args that you can use to customize a few different parameters:

  * package  - Specify the Cobalt package name to test against
  * activity - Specify the Cobalt activity name to test against
  * url      - Specify a Youtube video URL to test against
  * output   - Specify an output format for the monitoring data. Can
               be one of ['csv', 'plot', 'both'].
  * interval - Specify the polling interval frequency for collecting data
  * outdir   - Specify the output directory to place both the output
               data and the resulting graphs.
  * flags    - Specify the command line & experiment flags to provide to Cobalt
  * skip-restart - Do not restart the active Cobalt process
  * help     - Display a list of command options and the script's usage

It defaults to a test YouTube video and sampling rate. So, you will
need to specify those if you want something specific.

Below is a simple usage command:

```
python3 $HOME/chromium/src/cobalt/tools/performance/android_monitor.py
```

**Tip:** To avoid the script from placing the resulting plots and csv files into
you local checkout, utilize the `--outdir` flag to specify somewhere outside of
your checkout.

### Testing

In order to execute Python unit tests, the command below will use the python unittest
module to run any tests named after a specific pattern:

```
python3 -m unittest discover -s </path/to/test/directory> -p '*TEST PATTERN*.py'
```


### Pylint

Like all other python scripts in open source, they should follow the
guidance set by https://google.github.io/styleguide/pyguide. A nice way
to ensure conformance to this is to run the pylinter provided by
`depot_tools/pylint_main.py`.

**Note:** Use of the pylint script is done via `python3`.

#### Usage

```
python3 $HOME/chromium/tools/depot_tools/pylint_main.py $HOME/chromium/src/cobalt/tools/performance/android_monitor.py
```

### Troubleshooting

#### Module pylint not found

Installations of python3 and vpython3 may not have pylint installed:

```
sudo apt install pylint
```

#### Original error was: No module named 'numpy.core._multiarray_umath'

Run `pip install numpy --upgrade` to resolve the error

## ADB Commands to load test sites

  * To load test sites

    ```
    adb shell am start --esa \
      commandLineArgs '--remote-allow-origins=*,--url="<NAME OF TEST SITE>"' \
      dev.cobalt.coat
    ```

  * To kill Cobalt

    ```
    adb shell am force-stop dev.cobalt.coat
    ```
