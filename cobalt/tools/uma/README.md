# UMA Histogram Puller via CDP

This directory contains a Python script for interacting with Cobalt's UMA histograms via the Chrome DevTools Protocol (CDP).

## Features

*   **Connect to a running Cobalt instance:** The script can connect to a Cobalt instance with remote debugging enabled.
*   **Automatic Cobalt management:** The script can automatically launch and stop the Cobalt application on a connected Android device.
*   **Custom histogram sets:** The script can query for a default set of histograms, or a custom set provided via a text file.
*   **Flexible configuration:** The script can be configured with command-line arguments to control its behavior.

## Prerequisites

The script is designed to be run with `vpython3`, which manages the Python environment and dependencies. Ensure that `vpython3` is available in your shell.

## Usage

To run the script, navigate to the `cobalt/tools/uma/` directory and execute `pull_uma_histogram_set_via_cdp.py` with `vpython3`.

```bash
vpython3 pull_uma_histogram_set_via_cdp.py [OPTIONS]
```

### Command-line Arguments

*   `--histogram-file` (type: `str`)
    Path to a text file containing a list of histograms to query, one per line.
*   `--no-manage-cobalt` (action: `store_true`)
    If set, the script will not attempt to stop the Cobalt process on exit.
    The script will always attempt to start Cobalt if it is not running.
*   `--package-name` (type: `str`, default: `dev.cobalt.coat`)
    The package name of the Cobalt application.
*   `--poll-interval-s` (type: `float`, default: `30.0`)
    The polling frequency in seconds.
*   `--output-file` (type: `str`)
    Path to a file to log the histogram data to. The data will be saved in a
    CSV-like format with the timestamp, histogram name, and JSON data.

### Examples

1.  **Run with default settings (managing the default Cobalt package):**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py
    ```

2.  **Query for a custom set of histograms from a file:**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py --histogram-file my_histograms.txt
    ```

3.  **Connect to a running Cobalt instance without managing its lifecycle:**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py --no-manage-cobalt
    ```

4.  **Specify a different Cobalt package name:**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py --package-name com.my.custom.cobalt
    ```

5.  **Set the polling interval to 10.5 seconds:**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py --poll-interval-s 10.5
    ```

6.  **Direct all output to a file:**
    ```bash
    vpython3 pull_uma_histogram_set_via_cdp.py --output-file output.log
    ```

## Testing

Unit tests are provided to ensure the functionality of the script. To run the tests, execute the following command from the project root:

```bash
vpython3 cobalt/tools/uma/pull_uma_histogram_set_via_cdp_test.py
```
