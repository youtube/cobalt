# Gnuplot script: plot_performance_multi.gp
# Plots CPU, Memory, and specified log metrics over time from a CSV file using multiplot.
# Assumes CSV has headers matching metric names (e.g., 'FrameTime_ms', 'CacheMisses').
# Accepts input/output filenames, system context, and log column names via -e flag.
# Example:
# gnuplot -e "datafile='in.csv'; outfile='out.png'; cores=8; mem_total=4096; \
#             logmetric1_col='FrameTime_ms'; logmetric2_col='CacheMisses'" plot_performance_multi.gp

# --- Input File & Variables ---
# Define defaults if not provided via the -e flag
if (!exists("datafile")) datafile = 'cobalt_performance_data.csv'
if (!exists("outfile")) outfile = 'cobalt_performance_multiplot.png' # Default output name
if (!exists("cores")) cores = 0
if (!exists("mem_total")) mem_total = 0
# --- !!! Define the EXACT column header names for your log metrics !!! ---
# Pass these via -e flag like: logmetric1_col='YourHeaderName1'
if (!exists("logmetric1_col")) logmetric1_col = 'FrameTime_ms'  # Default for plot 2
if (!exists("logmetric2_col")) logmetric2_col = 'CacheMisses' # Default for plot 3

# --- Output ---
set terminal pngcairo size 1024,900 enhanced font 'Verdana,10' # Taller for 3 plots
set output outfile

# --- Data Format ---
set datafile separator ","
set datafile columnheaders # <<< USE FIRST ROW AS HEADERS >>>

# --- General Settings ---
set grid # Enable grid for all plots by default
set key top left box opaque # Default key position

# Check if input file exists before proceeding
stats datafile nooutput name "DF_"
if (GPVAL_ERRNO != 0) {
    print sprintf("Error: Cannot read input datafile '%s'. Exiting.", datafile)
    exit gnuplot
}

# --- Multiplot Layout ---
# 3 rows, 1 column layout. Adjust margins for titles and labels.
set multiplot layout 3,1 title "Cobalt Performance Over Time" font ",14"
set lmargin screen 0.12 # Left margin
set rmargin screen 0.90 # Right margin (adjust if y2 labels overlap)
set tmargin screen 0.92 # Top margin (space for main title)
set bmargin screen 0.10 # Bottom margin (space for x-label)

# --- Common X-Axis Settings ---
# Get xrange from data first to ensure alignment across plots
stats datafile using "Timestamp (s)" nooutput name "X_" # Use header name
set xrange [X_min:X_max]
# We will set the xlabel only on the bottom plot

# ==================================================
# --- Plot 1: CPU (%) & Memory (MB) ---
# ==================================================
set title "CPU & Memory" font ",10" # Subplot title
set ylabel "CPU Usage (% of 1 Core)" textcolor rgb "blue"
set y2label "Memory Usage (MB - PSS)" textcolor rgb "red" # Add y2 label
set ytics textcolor rgb "blue"              # Color primary y tics
set y2tics textcolor rgb "red" nomirror     # Color secondary y tics, make independent
set link y2                             # Link y2 axis for this plot
set autoscale y                         # Autoscale primary y axis
set autoscale y2                        # Autoscale secondary y axis
set xtics format ""                     # Hide x-axis tic labels for top/middle plots

# Optional context lines
if (cores > 0) { set arrow 1 from graph 0, first (cores*100) to graph 1, first (cores*100) nohead dt 2 lc rgb "dark-blue" back }
# if (mem_total > 0) { set arrow 2 from graph 0, second mem_total to graph 1, second mem_total nohead dt 3 lc rgb "dark-red" back }

# Plot using column header names
plot datafile using "Timestamp (s)":"CPU (%)" axes x1y1 with linespoints pt 7 lc rgb "blue" title "CPU (%)", \
     '' using "Timestamp (s)":"Memory (MB)" axes x1y2 with linespoints pt 5 lc rgb "red" title "Memory (MB)"

# Cleanup for next plot
unset link y2
unset y2label
unset y2tics
if (exists("arrow 1")) unset arrow 1
if (exists("arrow 2")) unset arrow 2

# ==================================================
# --- Plot 2: Log Metric 1 (e.g., FrameTime_ms) ---
# ==================================================
set title logmetric1_col font ",10" # Use variable for title
set ylabel sprintf("%s", logmetric1_col) textcolor rgb "dark-green" # Use variable for label
set ytics textcolor rgb "dark-green"
set autoscale y # Autoscale this y-axis
set xtics format "" # Hide x-axis tic labels

# Check if the specified column exists before plotting
stats datafile using logmetric1_col nooutput name "LM1_" # Check column by header name
if (GPVAL_ERRNO == 0) {
    # Plot using column header name, explicitly use column() function for safety with names
    plot datafile using "Timestamp (s)":(column(logmetric1_col)) axes x1y1 with linespoints pt 9 lc rgb "dark-green" title logmetric1_col
} else {
    print sprintf("Warning: Column '%s' not found or invalid in datafile '%s'. Skipping plot 2.", logmetric1_col, datafile)
    plot NaN title sprintf("Column '%s' not found", logmetric1_col) # Plot dummy to keep layout
}

# ==================================================
# --- Plot 3: Log Metric 2 (e.g., CacheMisses) ---
# ==================================================
set title logmetric2_col font ",10"
set ylabel sprintf("%s", logmetric2_col) textcolor rgb "purple"
set ytics textcolor rgb "purple"
set autoscale y
set xtics format "%.0f" # SHOW x-axis tic labels on the bottom plot
set xlabel "Time (s)"    # SHOW x-axis label on the bottom plot

# Check if the specified column exists before plotting
stats datafile using logmetric2_col nooutput name "LM2_"
if (GPVAL_ERRNO == 0) {
    plot datafile using "Timestamp (s)":(column(logmetric2_col)) axes x1y1 with linespoints pt 11 lc rgb "purple" title logmetric2_col
} else {
    print sprintf("Warning: Column '%s' not found or invalid in datafile '%s'. Skipping plot 3.", logmetric2_col, datafile)
    plot NaN title sprintf("Column '%s' not found", logmetric2_col) # Plot dummy
}

# ==================================================

# --- Finish Multiplot ---
unset multiplot # IMPORTANT: End multiplot mode
set output # Close output file

# Use the 'outfile' variable in the confirmation message
print sprintf("Multiplot graph saved to '%s'", outfile)

# Optional pause for interactive terminals
# pause -1 "Press Enter to exit"
