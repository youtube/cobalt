#!/usr/bin/env python3
"""
Segment-Aware Zstd Compression Script for Cobalt/Evergreen

This script optimizes Zstandard (Zstd) compression for the Cobalt ELF loader by 
aligning Zstd frame boundaries with ELF Program Segments (LOAD segments).

Why this is needed:
1.  **Eliminate Redundant Decompression:** Standard Zstd chunking is "blind" to ELF 
    structure. If a 4KB ELF header sits inside a 1MB frame, the loader must 
    decompress the whole 1MB just to check the header.
2.  **Structural Alignment:** By forcing Zstd "Cut Points" at ELF segment boundaries, 
    we ensure that each loader request maps to a clean set of Zstd frames.
3.  **Zero-Copy Support:** This alignment allows the C++ Decompression Orchestrator 
    to expand frames directly into their final memory-mapped destination without 
    intermediate buffering.

Implementation:
- Uses `readelf` to identify LOAD segments.
- Defines frame boundaries at segment start/end points.
- Limits maximum frame size to ~1MB to ensure high multi-core parallelization.
- Guarantees "Content Size" metadata in every frame header for random access.
"""

import subprocess
import sys
import os
import tempfile
import re

def get_segments(elf_path):
    """Parses readelf -l to find LOAD segment offsets and sizes from the ELF header."""
    # Support multiple toolchains by trying different readelf binaries.
    readelf_cmds = ["aarch64-linux-gnu-readelf", "arm-linux-gnueabi-readelf", "readelf"]
    
    output = None
    for cmd in readelf_cmds:
        try:
            output = subprocess.check_output([cmd, "-l", elf_path]).decode('utf-8')
            break
        except (subprocess.CalledProcessError, FileNotFoundError):
            continue
            
    if output is None:
        raise RuntimeError("Could not find a valid readelf command.")
    
    segments = []
    # State machine to handle multi-line "LOAD" entries typical of 64-bit readelf output.
    lines = output.splitlines()
    for i in range(len(lines)):
        if "LOAD" in lines[i]:
            # Combine current and next line to capture all hex fields (Offset, VirtAddr, FileSiz, etc.)
            combined = lines[i] + " " + (lines[i+1] if i+1 < len(lines) else "")
            hex_values = re.findall(r'0x[0-9a-fA-F]+', combined)
            
            if len(hex_values) >= 4:
                # 64-bit format: Offset is index 0, FileSiz is index 3.
                offset = int(hex_values[0], 16)
                filesz = int(hex_values[3], 16)
                if filesz > 0:
                    segments.append((offset, filesz))
            elif len(hex_values) >= 2:
                # Fallback for 32-bit single-line format where Offset is index 1 and FileSiz is index 4.
                parts = lines[i].split()
                if "LOAD" in parts[0]:
                    offset = int(parts[1], 16)
                    filesz = int(parts[4], 16)
                    if filesz > 0:
                        segments.append((offset, filesz))
                        
    return sorted(list(set(segments)))

def create_segment_aware_zstd(out_dir, level=3, max_frame_size=1024*1024):
    """Creates a multi-frame Zstd file where frames are aligned to ELF segments."""
    input_so = os.path.join(out_dir, "libcobalt.so")
    output_zst = os.path.join(out_dir, f"libcobalt_smart_L{level}.zst")
    
    if not os.path.exists(input_so):
        print(f"Error: {input_so} not found.")
        return

    print(f"Analyzing {input_so}...")
    segments = get_segments(input_so)
    file_size = os.path.getsize(input_so)
    
    # 1. Calculate "Cut Points" based on ELF structural boundaries.
    # We always include the 4KB mark to ensure the ELF header gets its own tiny frame.
    cut_points = {0, 4096} 
    for offset, size in segments:
        cut_points.add(offset)
        cut_points.add(offset + size)
    cut_points.add(file_size)
    sorted_points = sorted([p for p in cut_points if p <= file_size])
    
    # 2. Sub-divide large segments into chunks (default 1MB) to allow parallel decompression.
    final_ranges = []
    for i in range(len(sorted_points) - 1):
        start = sorted_points[i]
        end = sorted_points[i+1]
        while (end - start) > max_frame_size:
            final_ranges.append((start, start + max_frame_size))
            start += max_frame_size
        if end > start:
            final_ranges.append((start, end))

    print(f"Created {len(final_ranges)} smart frame definitions.")

    # Load the whole file into RAM for faster splitting.
    with open(input_so, 'rb') as in_f:
        full_data = in_f.read()

    # 3. Compress each range as a separate Zstd frame.
    # We use temporary files to ensure `zstd` can calculate and embed the Content Size 
    # metadata in each frame header.
    with open(output_zst, 'wb') as out_f:
        with tempfile.TemporaryDirectory() as tmpdir:
            for i, (start, end) in enumerate(final_ranges):
                chunk_data = full_data[start:end]
                chunk_file = os.path.join(tmpdir, f"chunk_{i}.raw")
                compressed_file = os.path.join(tmpdir, f"chunk_{i}.zst")
                
                with open(chunk_file, 'wb') as cf:
                    cf.write(chunk_data)
                
                # --content-size is critical for the C++ loader to know the output buffer size.
                subprocess.run(["zstd", f"-{level}", "--quiet", "--content-size", chunk_file, "-o", compressed_file], check=True)
                
                # Append the individual frame to the final Zstd bitstream.
                with open(compressed_file, 'rb') as zf:
                    out_f.write(zf.read())
                
                if i % 20 == 0:
                    print(f"  Processed {i}/{len(final_ranges)} frames...")

    print(f"Success! Created {output_zst}")

if __name__ == "__main__":
    # Usage: ./make_segment_aware_zstd_frames.py [out_dir] [level]
    # Defaults to ARMv7 Gold build at Compression Level 3.
    target_dir = sys.argv[1] if len(sys.argv) > 1 else "out/evergreen-arm-hardfp-rdk_gold"
    lvl = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    create_segment_aware_zstd(target_dir, level=lvl)
