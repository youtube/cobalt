#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Processes an MP4 into separate VP9 and Opus WebM files for splash.html."""

import argparse
import os
import shutil
import subprocess
import sys


def process_splash_media(input_path: str,
                         output_base: str,
                         video_bitrate: str = '2M',
                         audio_bitrate: str = '128k',
                         overwrite: bool = True) -> int:
  """Converts an MP4 file into separate video-only and audio-only WebM files.

  Args:
    input_path: Path to the source MP4 file.
    output_base: Base output filename/path.
    video_bitrate: Target video bitrate for VP9 encoding.
    audio_bitrate: Target audio bitrate for Opus encoding.
    overwrite: Whether to overwrite existing destination files.

  Returns:
    0 on success, non-zero error code on failure.
  """
  if not shutil.which('ffmpeg'):
    print('Error: ffmpeg binary not found in PATH.', file=sys.stderr)
    return 1

  if not os.path.isfile(input_path):
    print(f'Error: Input file does not exist: {input_path}', file=sys.stderr)
    return 1

  # Strip .webm extension if caller passed something like "splash.webm"
  base = output_base[:-5] if output_base.endswith('.webm') else output_base
  video_output = f'{base}_video.webm'
  audio_output = f'{base}_audio.webm'

  # Ensure destination directory exists
  out_dir = os.path.dirname(base)
  if out_dir:
    os.makedirs(out_dir, exist_ok=True)

  overwrite_flag = ['-y'] if overwrite else ['-n']

  print(f'Processing: {input_path}')
  print(f'  -> Video output: {video_output}')
  print(f'  -> Audio output: {audio_output}')

  # 1. Generate VP9 Video-only WebM
  video_cmd = [
      'ffmpeg',
      *overwrite_flag,
      '-i',
      input_path,
      '-an',
      '-c:v',
      'libvpx-vp9',
      '-b:v',
      video_bitrate,
      '-pix_fmt',
      'yuv420p',
      video_output,
  ]
  cmd_str = ' '.join(video_cmd)
  print(f'\nRunning video encoding command:\n  {cmd_str}')
  video_result = subprocess.run(video_cmd, check=False)
  if video_result.returncode != 0:
    print(
        f'Error: Video encoding failed (code: {video_result.returncode})',
        file=sys.stderr)
    return video_result.returncode

  # 2. Generate Opus Audio-only WebM
  audio_cmd = [
      'ffmpeg',
      *overwrite_flag,
      '-i',
      input_path,
      '-vn',
      '-c:a',
      'libopus',
      '-b:a',
      audio_bitrate,
      audio_output,
  ]
  cmd_str = ' '.join(audio_cmd)
  print(f'\nRunning audio encoding command:\n  {cmd_str}')
  audio_result = subprocess.run(audio_cmd, check=False)
  if audio_result.returncode != 0:
    print(
        f'Error: Audio encoding failed (code: {audio_result.returncode})',
        file=sys.stderr)
    return audio_result.returncode

  # Report results
  video_size = os.path.getsize(video_output)
  audio_size = os.path.getsize(audio_output)
  print('\nEncoding completed successfully!')
  print(f'  Video file: {video_output} ({video_size:,} bytes)')
  print(f'  Audio file: {audio_output} ({audio_size:,} bytes)')
  return 0


def main() -> int:
  parser = argparse.ArgumentParser(
      description=(
          'Convert MP4 into separate VP9 video and Opus audio WebM files.'))
  parser.add_argument('input_mp4', help='Path to the input MP4 file')
  parser.add_argument(
      'output_filename',
      help=('Base output filename (produces <output>_video.webm and'
            ' <output>_audio.webm)'))
  parser.add_argument(
      '--video-bitrate',
      default='2M',
      help='Target video bitrate (default: 2M)')
  parser.add_argument(
      '--audio-bitrate',
      default='128k',
      help='Target audio bitrate (default: 128k)')
  parser.add_argument(
      '--no-overwrite',
      action='store_true',
      help='Do not overwrite existing destination files')

  args = parser.parse_args()
  return process_splash_media(
      input_path=args.input_mp4,
      output_base=args.output_filename,
      video_bitrate=args.video_bitrate,
      audio_bitrate=args.audio_bitrate,
      overwrite=not args.no_overwrite,
  )


if __name__ == '__main__':
  sys.exit(main())
