# Cobalt Memory Analysis Scripts

This directory contains scripts to automate the process of running a Cobalt evergreen build and measuring its Resident Set Size (RSS) memory usage over time. This provides a programmatic way to collect the same type of memory data you might observe manually using a tool like `htop`.

The scripts can generate a comparative plot from two different data sets, making it easy to analyze the memory impact of code changes.

## How to Use

### 1. Run Your Benchmark Test

First, run a baseline test to establish your benchmark performance. The script will automatically run the Cobalt process 10 times for 60 seconds each and log the memory usage.

```bash
./log_rss.sh /path/to/your/cobalt/directory
mv rss_log.csv benchmark.csv
```

### 2. Run Your Experiment Test

After making your code changes, run the test again to gather data on your experiment.

```bash
./log_rss.sh /path/to/your/cobalt/directory
mv rss_log.csv experiment.csv
```

### 3. Generate the Comparison Plot

Use the `plot_rss.py` script to generate a visual comparison of your benchmark and experiment data.

```bash
python3 plot_rss.py benchmark.csv experiment.csv
```

This will create an image file named `rss_comparison_plot.png` in your current directory. The plot will display two lines: a solid blue line for the benchmark's average RSS and a dashed red line for the experiment's average RSS, allowing for a clear comparison of memory performance.
