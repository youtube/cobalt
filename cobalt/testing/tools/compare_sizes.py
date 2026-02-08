"""Compares the current size of libcobalt.so with a reference size."""

import json
import sys
import argparse


def get_stripped_size(perf_results_path):
  with open(perf_results_path, 'r', encoding='utf-8') as f:
    data = json.load(f)
  for item in data:
    if item.get('name') == 'libcobalt.so':
      return item['sampleValues'][0]
  return None


def get_reference_size(reference_path, platform, config):
  with open(reference_path, 'r', encoding='utf-8') as f:
    data = json.load(f)
  build_key = f"{platform}_{config}"
  if build_key in data and 'libcobalt.so' in data[build_key]:
    return data[build_key]['libcobalt.so']['size']
  return None


def main():
  parser = argparse.ArgumentParser(description='Compare binary sizes.')
  parser.add_argument(
      '--current', required=True, help='Path to the current perf_results.json')
  parser.add_argument(
      '--reference', required=True, help='Path to the reference_metrics.json')
  parser.add_argument(
      '--platform',
      required=True,
      help='Platform (e.g., evergreen-arm-hardfp-rdk)')
  parser.add_argument('--config', required=True, help='Config (e.g., gold)')
  args = parser.parse_args()

  current_size = get_stripped_size(args.current)
  if current_size is None:
    print(
        f"Error: Could not find libcobalt.so size in {args.current}",
        file=sys.stderr)
    sys.exit(1)

  reference_size = get_reference_size(args.reference, args.platform,
                                      args.config)
  if reference_size is None:
    print(
        f'Error: Could not find reference size for {args.platform}_'
        f'{args.config} in {args.reference}',
        file=sys.stderr)
    sys.exit(1)

  print(f'Current stripped size: {current_size} bytes ('
        f'{current_size / 1000000:.2f} MB)')
  print(f'Reference stripped size: {reference_size} bytes ('
        f'{reference_size / 1000000:.2f} MB)')

  if current_size > reference_size:
    print(
        f'Error: Current size ({current_size}) exceeds reference size '
        f'({reference_size}) by {current_size - reference_size} bytes',
        file=sys.stderr)
    sys.exit(1)
  else:
    print('Success: Current size is within the reference size.')
    sys.exit(0)


if __name__ == '__main__':
  main()
