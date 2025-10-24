# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
""" Starts an av_sync_record process to record the video from the camera. """

import logging
import os.path
import subprocess

from dataclasses import dataclass
from typing import Optional


@dataclass
class Parameters:
    fps: Optional[int] = None
    output_path: str = '/tmp'
    file: str = 'video'
    duration_sec: Optional[int] = None
    max_frames: Optional[int] = None
    serial_number: Optional[str] = None

    @property
    def video_file(self):
        assert self.output_path
        assert self.file
        return os.path.join(self.output_path, f'{self.file}.mkv')

    @property
    def info_file(self):
        assert self.output_path
        assert self.file
        return os.path.join(self.output_path, f'{self.file}.info.csv')


def start(parameters: Parameters) -> None:
    """Starts an av_sync_record process to record the video from the camera.
    Executing of this function shouldn't be terminated as it would create a
    bad constructed video file."""
    assert parameters.output_path
    assert parameters.file
    BINARY = '/usr/local/cipd/av_sync_record/av_sync_record'
    assert os.path.isfile(BINARY)
    args = [
        BINARY, '--gid=', '--uid=',
        f'--camera_info_path={parameters.info_file}',
        f'--video_output={parameters.video_file}'
    ]
    if parameters.fps:
        args.append(f'--camera_fps={parameters.fps}')
    if parameters.duration_sec:
        args.append(f'--duration={parameters.duration_sec}s')
    if parameters.max_frames:
        args.append(f'--max_frames={parameters.max_frames}')
    if parameters.serial_number:
        args.append(f'--serial_number={parameters.serial_number}')
    logging.warning('Camera starts recording to %s', parameters.output_path)
    subprocess.run(args, check=True)
    logging.warning('Camera finishes recording.')
