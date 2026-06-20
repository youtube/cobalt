#!/usr/bin/env python3
"""
Cobalt Memory Benchmark Comparison Report Generator (Control vs Treatment)

This script reads two parsed memory metrics CSV files (Control and Treatment),
calculates the steady-state averages (last 30% of the run by default),
and generates a beautifully formatted hierarchical Markdown comparison table.

Usage:
  python3 compare_runs.py --control path/to/control.csv --treatment path/to/treatment.csv

Example:
  python3 compare_runs.py \
    --control kjyoun.gemini/memory-pressure/test-run-0613_1131/cobalt_c26_0613_1131.csv \
    --treatment kjyoun.gemini/memory-pressure/test-run-0613_1131/cobalt_c27_0613_1131.csv
"""

import argparse
import csv
import os
import sys
import numpy as np

def load_metrics(csv_path):
    """Loads memory metrics from CSV file into a dictionary of lists."""
    metrics = {
        'Total_PSS': [], 'Native_PSS': [], 'CPP_Heap': [],
        'JVM_Native': [], 'JVM_Java': [], 'JS_Heap': [],
        'JS_Phys': [], 'JS_Exec': [], 'JS_Ext': [],
        'Decoder_Buffer': [], 'Skia_Cache': [],
        'Code_PSS': [], 'Other_PSS': []
    }
    
    if not os.path.exists(csv_path):
        print(f"Error: File not found at '{csv_path}'", file=sys.stderr)
        sys.exit(1)
        
    with open(csv_path, 'r', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        
        headers = reader.fieldnames
        if not headers:
            print(f"Error: Empty CSV file at '{csv_path}'", file=sys.stderr)
            sys.exit(1)
            
        for row in reader:
            try:
                metrics['Total_PSS'].append(float(row['Total_PSS_MiB']))
                metrics['Native_PSS'].append(float(row['Total_Native_PSS_MiB']))
                metrics['CPP_Heap'].append(float(row['CPP_Heap_PA_Committed_MiB']))
                metrics['JVM_Native'].append(float(row['JVM_Native_Overhead_dalvik_other_MiB']))
                metrics['JVM_Java'].append(float(row['JVM_Java_Heap_dalvik_MiB']))
                metrics['JS_Heap'].append(float(row['JS_Heap_Live_V8_Used_MiB']))
                metrics['Decoder_Buffer'].append(float(row['Media_Decoder_Buffer_Pool_MiB']))
                metrics['Skia_Cache'].append(float(row['Skia_GPU_Cache_MiB']))
                
                # Handle optional columns with fallback for backward compatibility
                metrics['Code_PSS'].append(float(row.get('Code_PSS_MiB', 0.0)))
                metrics['Other_PSS'].append(float(row.get('Other_PSS_MiB', 0.0)))
                metrics['JS_Phys'].append(float(row.get('JS_Heap_Physical_V8_MiB', 0.0)))
                metrics['JS_Exec'].append(float(row.get('JS_Heap_Executable_V8_MiB', 0.0)))
                metrics['JS_Ext'].append(float(row.get('JS_Heap_External_V8_MiB', 0.0)))
            except ValueError as e:
                print(f"Warning: Skipping malformed row in {csv_path}: {e}", file=sys.stderr)
                
    return metrics

def get_steady_state_avg(data_list, window_pct):
    """Calculates mean and std dev of the last window_pct portion of the data."""
    if not data_list:
        return 0.0, 0.0
    n = len(data_list)
    start_idx = int(n * (1.0 - window_pct))
    steady_data = data_list[start_idx:]
    return np.mean(steady_data), np.std(steady_data)

def main():
    parser = argparse.ArgumentParser(description="Generate hierarchical memory comparison report.")
    parser.add_argument("-c", "--control", type=str, required=True, help="Path to Control (baseline) CSV file")
    parser.add_argument("-t", "--treatment", type=str, required=True, help="Path to Treatment (comparison) CSV file")
    parser.add_argument("-w", "--window", type=float, default=0.3, help="Steady-state window percentage (default: 0.3)")
    args = parser.parse_args()

    control = load_metrics(args.control)
    treatment = load_metrics(args.treatment)

    # Define the hierarchical pillars: (Label, CSV_Key, Indent_Level)
    # Indent levels: 0 = root, 1 = first level child (├──), 2 = second level child (      ├──)
    pillars = [
        ('Total Process PSS', 'Total_PSS', 0),
        ('├── Total Native PSS', 'Native_PSS', 1),
        ('├── JVM Java Heap (Dalvik)', 'JVM_Java', 1),
        ('└── Other PSS (mmap/system/stack)', 'Other_PSS', 1),
        ('&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;├── Of which: Code PSS (Binary/Dex)', 'Code_PSS', 2),
        ('&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;├── Of which: JVM Native Overhead (JNI)', 'JVM_Native', 2),
        ('C++ Heap (PartitionAlloc) *', 'CPP_Heap', 0),
        ('Media Decoder Buffer Pool *', 'Decoder_Buffer', 0),
        ('JS Heap Live (V8 Used)', 'JS_Heap', 0),
        ('├── V8 Physical Committed Heap', 'JS_Phys', 1),
        ('├── V8 Executable JIT Code', 'JS_Exec', 1),
        ('└── V8 External (ArrayBuffers)', 'JS_Ext', 1),
        ('Skia GPU Resource Cache', 'Skia_Cache', 0)
    ]

    print('| Memory Pillar (MiB) | Control (Steady State) | Treatment (Steady State) | Delta (MiB) | % Change |')
    print('| :--- | :---: | :---: | :---: | :---: |')

    for label, key, indent in pillars:
        control_mean, control_std = get_steady_state_avg(control[key], args.window)
        treatment_mean, treatment_std = get_steady_state_avg(treatment[key], args.window)
        
        # If the metric was not present (all zeros), show as 0.00
        if control_mean == 0.0 and treatment_mean == 0.0:
            print(f'| {label} | 0.00 (±0.00) | 0.00 (±0.00) | 0.00 | 0.0% |')
            continue
            
        delta = treatment_mean - control_mean
        pct_change = (delta / control_mean) * 100 if control_mean > 0 else 0.0
        
        sign = '+' if delta > 0 else ''
        pct_sign = '+' if pct_change > 0 else ''
        
        # Bold only the root level metrics for clear hierarchy
        display_label = f"**{label}**" if indent == 0 else label
        
        print(f'| {display_label} | {control_mean:.2f} (±{control_std:.2f}) | {treatment_mean:.2f} (±{treatment_std:.2f}) | {sign}{delta:.2f} | {pct_sign}{pct_change:.1f}% |')

if __name__ == "__main__":
    main()
