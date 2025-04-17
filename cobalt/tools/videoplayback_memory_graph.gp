# Gnuplot script: plot_performance.gp
# Plots CPU (%) and Memory (MB) usage over time from a CSV file.
# Accepts input/output filenames and system context via -e flag, e.g.:
# gnuplot -e "datafile='in.csv'; outfile='out.png'; cores=8; mem_total=4096" plot_performance.gp

# --- Input File & Variables ---
# Define default filenames and context values if they are not provided via the -e flag
if (!exists("datafile")) datafile = 'cobalt_performance_data.csv'
if (!exists("outfile")) outfile = 'cobalt_performance_graph.png'
if (!exists("cores")) cores = 0 # Default to 0 if not specified
if (!exists("mem_total")) mem_total = 0 # Default to 0 if not specified

# --- Output ---
# Configure the output terminal and filename using the 'outfile' variable
set terminal pngcairo size 1024,700 enhanced font 'Verdana,10'
set output outfile

# --- Data Format ---
# Use comma as the separator for CSV
set datafile separator ","

# --- Plot Appearance ---
# Use variables in title if available
title_str = "Cobalt CPU and Memory Usage Over Time"
if (cores > 0 && mem_total > 0) {
    # Condition 1: Both cores and mem_total are known and positive
    title_str = sprintf("%s\n(%d Cores, %.0f MB Total RAM)", title_str, cores, mem_total)
} else {
    # Condition 2: At least one is unknown or zero, check if only cores is known
    if (cores > 0) {
        # Condition 2a: Only cores is known and positive
        title_str = sprintf("%s (%d Cores)", title_str, cores)
    }
    # Condition 2b: If only mem_total is known, or neither is known,
    # the default title_str remains appropriate, so no further 'else' needed here.
}
set title title_str font ',14'

set xlabel "Time (s)"
set grid

# --- Handling Dual Y-Axes ---
set ylabel "CPU Usage (% of 1 Core)" textcolor rgb "blue"
set y2label "Memory Usage (MB - PSS)" textcolor rgb "red"
set ytics textcolor rgb "blue"
set y2tics textcolor rgb "red"
set link y2
set autoscale y
set autoscale y2

# --- Optional Context Lines (using variables) ---
# Add a horizontal line showing theoretical max CPU (cores * 100%) if cores known
if (cores > 0) {
  # Use 'set style line' for better management if adding more lines
  set style line 100 lc rgb "dark-blue" dt 2 # Dashed type 2
  set arrow from graph 0, first (cores*100) to graph 1, first (cores*100) nohead \
            linestyle 100 back
  # Add a label for the max CPU line
  set label 1 sprintf("%d00%% CPU Max", cores) at graph 0.02, first (cores*100) offset 0,0.5 \
            textcolor rgb "dark-blue" font ',8'
}
# Example: Add a line for total system memory on y2 axis if known
# if (mem_total > 0) {
#   set style line 101 lc rgb "dark-red" dt 3 # Dotted type 3
#   set arrow from graph 0, second mem_total to graph 1, second mem_total nohead \
#             linestyle 101 back
#   set label 2 sprintf("Total RAM %.0f MB", mem_total) at graph 0.02, second mem_total offset 0,0.5 \
#             textcolor rgb "dark-red" font ',8'
# }

# --- Key / Legend ---
set key top left box opaque

# --- Plot Command ---
# Use the 'datafile' variable
# Check if datafile exists before attempting to plot
stats datafile nooutput name "DF_" # Check file, prefix stats vars with DF_
if (GPVAL_ERRNO == 0) { # If no error reading file stats
    plot datafile using 1:2 axes x1y1 with linespoints pt 7 lc rgb "blue" title sprintf("CPU (%% of 1 Core)"), \
         '' using 1:3 axes x1y2 with linespoints pt 5 lc rgb "red" title sprintf("Memory (MB PSS)") # Simplified titles
} else {
    print sprintf("Error: Cannot read input datafile '%s'. Please check the path.", datafile)
    exit gnuplot # Exit if file not found/readable
}

# --- Cleanup ---
set output # Close the output file
unset link y2
# Unset any arrows and labels we might have added
if (exists("arrow")) unset arrow
if (exists("label")) unset label
if (exists("style line 100")) unset style line 100
if (exists("style line 101")) unset style line 101


# Use the 'outfile' variable in the confirmation message
print sprintf("Graph saved to '%s'", outfile)

# Optional pause for interactive terminals
# pause -1 "Press Enter to exit"
