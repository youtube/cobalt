#!/usr/bin/env python3
import sys
import re
import subprocess

def parse_sf_dump(content):
    layers = []
    # Split by "+" at the start of a line to get layer blocks (approximate)
    blocks = re.split(r'\n(?=\+ )', content)
    
    current_cobalt_layers = []
    
    for block in blocks:
        if "dev.cobalt.coat" not in block:
            continue
            
        # Extract layer name
        name_match = re.search(r'\+ \w+Layer \(([^)]+)\)', block)
        if not name_match:
            continue
        name = name_match.group(1)
        
        # Extract z-order
        z_match = re.search(r'z=\s*(-?\d+)', block)
        z = int(z_match.group(1)) if z_match else None
        
        # Extract zOrderRelativeOf
        rel_match = re.search(r'zOrderRelativeOf=([^\s\n]+)', block)
        rel_of = rel_match.group(1) if rel_match else "none"
        
        # Extract format (try to guess if UI or Video)
        fmt_match = re.search(r'defaultPixelFormat=([^\s,]+)', block)
        fmt = fmt_match.group(1) if fmt_match else "Unknown"
        
        # Extract size/pos
        pos_match = re.search(r'pos=\(([^)]+)\)', block)
        size_match = re.search(r'size=\(([^)]+)\)', block)
        pos = pos_match.group(1) if pos_match else "unknown"
        size = size_match.group(1) if size_match else "unknown"
        
        is_video = "YUV" in block or "0x11" in block or "req fmt:17" in block or fmt == "Unknown" # Video format is often 0x11 (NV21)
        # Web UI is usually RGBA_8888
        is_ui = fmt == "RGBA_8888"
        
        layer_info = {
            "name": name,
            "z": z,
            "rel_of": rel_of,
            "format": fmt,
            "pos": pos,
            "size": size,
            "is_video": is_video,
            "is_ui": is_ui,
            "raw": block
        }
        current_cobalt_layers.append(layer_info)
        
    return current_cobalt_layers

def analyze_layers(layers):
    print(f"Found {len(layers)} Cobalt related layers.")
    
    # Group by rel_of
    groups = {}
    for l in layers:
        rel = l["rel_of"]
        if rel not in groups:
            groups[rel] = []
        groups[rel].append(l)
        
    for rel, group in groups.items():
        if rel == "none":
            continue
        print(f"\n--- Layers relative to: {rel} ---")
        # Sort by Z-order (ascending)
        group.sort(key=lambda x: x["z"] if x["z"] is not None else -999)
        
        for l in group:
            role = "UI" if l["is_ui"] else ("Video" if l["is_video"] else "Unknown")
            print(f"  Z={l['z']:3d} | Name: {l['name']} | Role: {role:5s} | Format: {l['format']} | Pos: {l['pos']} | Size: {l['size']}")
            
        # Check for occlusion
        # We look for any UI layer that has a higher Z than a Video layer in the same relative group.
        ui_layers = [x for x in group if x["is_ui"]]
        video_layers = [x for x in group if x["is_video"]]
        
        for ui in ui_layers:
            for vid in video_layers:
                if ui["z"] > vid["z"]:
                    print(f"\n[WARNING] Occlusion Detected!")
                    print(f"  UI Layer '{ui['name']}' (Z={ui['z']}) is ON TOP OF Video Layer '{vid['name']}' (Z={vid['z']})")
                    # Check if UI is likely an orphan (we have duplicates)
                    ui_siblings = [x for x in ui_layers if x != ui]
                    if ui_siblings:
                        print(f"  Note: There are multiple UI layers. '{ui['name']}' might be an ORPHAN.")
                    vid_siblings = [x for x in video_layers if x != vid]
                    if vid_siblings:
                        print(f"  Note: There are multiple Video layers. '{vid['name']}' might be an ORPHAN.")

def main():
    content = ""
    if len(sys.argv) > 1:
        filepath = sys.argv[1]
        print(f"Reading from file: {filepath}")
        with open(filepath, 'r') as f:
            content = f.read()
    else:
        # Run adb command
        device = "localhost:38473"
        print(f"Running adb -s {device} shell dumpsys SurfaceFlinger ...")
        try:
            cmd = ["adb", "-s", device, "shell", "dumpsys", "SurfaceFlinger"]
            result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, check=True)
            content = result.stdout
        except subprocess.CalledProcessError as e:
            print(f"Error running adb: {e.stderr}")
            sys.exit(1)
            
    layers = parse_sf_dump(content)
    analyze_layers(layers)

if __name__ == "__main__":
    main()
