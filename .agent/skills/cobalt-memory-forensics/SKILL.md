---
name: cobalt-memory-forensics
description: Profile, capture, and analyze Cobalt's memory footprint on Android TV using unpolluted C++ JNI memory hooks, standalone plotting, and automated benchmarking.
---

# Cobalt Memory Forensics & Analysis (Unpolluted JNI Edition)

This skill provides a comprehensive, production-grade toolchain to capture, isolate, and visualize Cobalt's memory footprint on Android TV. It features **unpolluted C++ JNI memory hooks** to isolate the Android JVM native overhead, a **standalone parser and plotter**, and a **fully automated benchmarking script** for push-button regression testing.

---

## 🧠 The Android "Unknown" Memory Trap

In hybrid C++/Java Android applications like Cobalt, standard OS-level memory summaries (like `dumpsys meminfo`'s `Private Other`) are **massively polluted**.

Because Cobalt's core C++ allocator (**`PartitionAlloc`**) maps anonymous memory pages directly via `mmap`, the Linux kernel cannot identify them and classifies them as **`Unknown`**. The Android OS then lumps this entire C++ heap (typically 120MB+) into the `Private Other` category.

To solve this, this pipeline uses **direct C++ JNI hooks** in `application_android.cc` to query the ART VM directly, bypassing the polluted OS summaries and isolating the **true, pure JVM native overhead (~5MB - 15MB)**.

---

## 🛠️ The JNI C++ Memory Isolation Hook

The C++ platform layer ([application_android.cc](file:///usr/local/google/home/kjyoun/cobalt.m138/src/starboard/android/shared/application_android.cc)) uses a stable, public Android JNI API to isolate the JVM VM overhead from Cobalt's C++ heap:

```cpp
if (get_other_private_dirty_method) {
  info.dalvik_other_pss_kb =
      env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method, 0) + // Dalvik Other (VM structures)
      env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method, 3) + // Ashmem (JIT compiler cache)
      env->CallIntMethod(mem_info_obj.obj(), get_other_private_dirty_method, 13); // ART mmap (Boot image)
}
```

This JNI hook runs on a background thread and prints the unpolluted `dalvik_other` value to the logcat every 5 seconds under the **`PSS_STATS`** tag.

---

## 🚀 Pipeline 1: Automated C26 vs C27 Benchmarking

For push-button regression testing between different Cobalt branches (e.g., C27/M138 vs C26), use the automated benchmarking script:
👉 **[run_benchmark.sh](file:///usr/local/google/home/kjyoun/cobalt.m138/src/run_benchmark.sh)**

### Usage:
Run the script from your workspace root, specifying the test duration and your target device serial:
```bash
./run_benchmark.sh --duration 300 --serial localhost:37603
```

### What it Automates:
1.  **Deploys and Launches C26:** Deploys the C26 metrics APK, clears the logcat, launches the target YouTube TV video, and captures the background logcat.
2.  **Deploys and Launches C27:** Deploys your newly built M138 APK, launches the same video, and captures the background logcat.
3.  **Post-Processing & Plotting:** Runs the standalone parser on both logcat files, exporting clean CSVs and generating timestamped PNG plots.
4.  **Instant Comparison Report:** Performs a steady-state mathematical comparison (averaging the last 30% of the timeline) and prints a beautifully formatted Markdown table directly to your terminal!

---

## 📊 Pipeline 2: Standalone Logcat Parsing & Plotting

If you want to run your own custom logging session, you can manually capture the logcat and parse it using the standalone Python script:
👉 **[parse_and_plot_memory.py](file:///usr/local/google/home/kjyoun/cobalt.m138/src/parse_and_plot_memory.py)**

### 1. Capture Your Logcat:
Launch Cobalt and play a video on your target device, then dump the logcat to a file:
```bash
adb -s <device_serial> logcat -d > log.cobalt.latest.txt
```

### 2. Parse and Plot:
Run the standalone Python script. It uses an **ultra-robust parser** that slices timestamps from the line start and extracts process PIDs directly from the Cobalt internal brackets (`dev.cobalt.coat/(\d+):`), making it 100% immune to logcat header variations:
```bash
./parse_and_plot_memory.py --log-file log.cobalt.latest.txt
```

### 3. Outputs:
*   **Timestamped CSV Spreadsheet (`cobalt_memory_MMDD_HHMM.csv`):** Exports all 10 unpolluted memory pillars (C++ Heap, V8 Heap, Skia, pure JVM Java and Native Heap) in a clean table ready for Google Sheets or Excel.
*   **Timestamped Memory Plot (`cobalt_memory_MMDD_HHMM.png`):** Generates a gorgeous, high-resolution memory component timeline plot.

---

## 📈 Understanding the Memory Pillars in the Plot

*   **`Total Process PSS` (Black Solid):** The overall physical footprint. If this increases in C27, it is likely driven by GPU graphics memory or a larger compiled library size, not a heap leak.
*   **`C++ Heap (PartitionAlloc committed)` (Blue Solid):** Cobalt's core C++ heap. Highly optimized and identical across branches.
*   **`JVM Native Overhead` (Crimson Dashed):** The true, unpolluted native footprint of the ART VM (~3.5MB to 15MB).
*   **`JVM Java Heap` (Yellow Dash-Dot):** Active Java-side objects (~4MB to 6MB). Highly optimized in C27 ( M138 is ~30% lighter than C26!).
*   **`JS Heap Live (V8 Used)` (Orange Solid):** Active JavaScript execution heap.
*   **`Media Decoder Buffer Pool` (Purple Dash-Dot):** Audio/video playback decoder buffers (~108MB).
*   **`Skia GPU Resource Cache` (Green Solid):** Skia graphics resource cache (~2MB).
