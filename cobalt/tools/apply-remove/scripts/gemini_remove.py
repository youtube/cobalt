#!/usr/bin/env python3
# Copyright 2024 The Cobalt Authors. All Rights Reserved.

"""Gemini-powered surgical component removal context gatherer."""

import argparse
import os
import subprocess
import json

def run_command(cmd):
    try:
        return subprocess.check_output(cmd, stderr=subprocess.STDOUT, shell=False).decode('utf-8')
    except:
        return ""

def get_build_gn_path(target):
    base_target = target.split('(')[0]
    path_part = base_target.split(':')[0].replace('//', '')
    return os.path.join(path_part, 'BUILD.gn')

def main():
    parser = argparse.ArgumentParser(description="Gather context for Gemini-powered removal.")
    parser.add_argument("--pivot", required=True, help="Target where removal happens")
    parser.add_argument("--target", required=True, help="Target to subtract")
    parser.add_argument("--header", help="Header pattern to guard")
    
    args = parser.parse_args()

    header_pattern = args.header or args.target.split('(')[0].replace('//', '').split(':')[0]
    pivot_gn = get_build_gn_path(args.pivot)
    pivot_dir = os.path.dirname(pivot_gn)
    
    cpp_files = []
    if os.path.exists(pivot_dir):
        cmd = ["git", "grep", "-l", f'#include "{header_pattern}', pivot_dir]
        output = run_command(cmd)
        cpp_files = output.splitlines()

    # Output JSON for the agent to process
    context = {
        "action": "remove_dependency",
        "pivot": args.pivot,
        "target": args.target,
        "header_pattern": header_pattern,
        "build_gn": pivot_gn,
        "cpp_files": cpp_files
    }
    
    print(json.dumps(context, indent=2))

if __name__ == "__main__":
    main()
