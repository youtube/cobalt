# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates FlagExpectations/surface-embed from wpt.fyi.

This script is used to generate the
third_party/blink/web_tests/FlagExpectations/surface-embed file, which contains
a list of tests that fail on the default Chrome build (as found on wpt.fyi).
This is used for filtering pre-existing failures on the
linux-surface-embed-rel test bot."""

import urllib.request
import json
import gzip
import datetime


def main():
    print("Fetching run details from wpt.fyi...")
    runs_url = "https://wpt.fyi/api/runs?product=chrome&label=master&max-count=1"
    req = urllib.request.Request(runs_url,
                                 headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req) as response:
        runs_data = json.loads(response.read().decode('utf-8'))

    if not runs_data:
        print("No runs found")
        return

    run = runs_data[0]
    results_url = run["results_url"]
    print(f"Latest Chrome run results URL: {results_url}")

    print("Downloading results summary...")
    req_summary = urllib.request.Request(results_url,
                                         headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req_summary) as response_summary:
        body = response_summary.read()
        if body.startswith(b'\x1f\x8b'):
            raw_data = gzip.decompress(body)
        else:
            raw_data = body
        summary = json.loads(raw_data.decode('utf-8'))

    print(f"Total test files in summary: {len(summary)}")

    failures = []
    for test_path, result in summary.items():
        status = result.get("s")
        if status in ("F", "C", "T", "TIMEOUT", "FAIL", "CRASH", "ERROR"):
            normalized_path = f"external/wpt{test_path}"
            failures.append((normalized_path, status))
        elif "c" in result:
            passes, total = result["c"]
            if passes < total:
                normalized_path = f"external/wpt{test_path}"
                failures.append((normalized_path, "FAIL"))

    print(f"Found {len(failures)} failing test files in default Chrome.")

    output_path = "third_party/blink/web_tests/FlagExpectations/surface-embed"
    with open(output_path, "w") as f:
        f.write("# tags: [ Linux ]\n")
        f.write("# tags: [ Release Debug ]\n")
        f.write("# results: [ Timeout Crash Pass Failure Slow Skip ]\n")
        f.write("\n")
        f.write(
            "# Automatically generated expectations of failures on default Chrome from wpt.fyi\n"
        )
        f.write(
            "# Used for filtering pre-existing failures on linux-surface-embed-rel\n"
        )
        f.write("#\n")
        f.write(
            f"# Last update: {datetime.datetime.now().strftime('%Y-%m-%d')}\n")
        f.write("#\n")
        f.write("# To update, run:\n")
        f.write(
            "#   vpython3 third_party/blink/web_tests/FlagExpectations/generate_surface_embed_expectations.py\n\n"
        )
        for path, status in sorted(failures):
            exp = "Failure"
            if status in ("C", "CRASH"):
                exp = "Crash"
            elif status in ("T", "TIMEOUT"):
                exp = "Timeout"

            # Add "Pass" expectations so that flaky tests won't show up as
            # "pass unexpectedly" in the test results.
            f.write(f"{path} [ Pass {exp} ]\n")

    print(f"Written expectations to {output_path}")


if __name__ == "__main__":
    main()
