"""Extracts GitHub Actions run IDs for jobs ending in '_tests'."""
import sys
import re

run_ids = set()
for line in sys.stdin:
  # Check if the line contains a job name ending in '_tests' followed by
  # a tab and the 'fail' status
  if re.search(r'\s+(Failed|Cancelled|fail|cancel)\s+', line, re.IGNORECASE):
    try:
      # The URL is the last element on the line
      url = line.strip().split()[-1]
      # Extract the number from the URL like .../runs/1234f/...
      match = re.search(r'/runs/(\d+)', url)
      if match:
        run_ids.add(match.group(1))
    except IndexError:
      # Ignore empty or malformed lines
      continue

if run_ids:
  print(' '.join(sorted(list(run_ids))))
