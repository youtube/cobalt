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
"""
This script calculates the display cadence for a video given its frame rate
and the display's refresh rate. The cadence is the pattern of how many times
each video frame should be displayed on screen to ensure smooth playback.

This script is a Python port of the logic found in the
VideoFrameCadencePatternGenerator::UpdateFrameRate function from
video_frame_cadence_pattern_generator.cc.
"""

import argparse
from fractions import Fraction


class VideoFrameCadencePatternGenerator:
  """
    Generates a cadence pattern based on video frame rate and display refresh
    rate, mimicking the logic from the Starboard C++ implementation.
    """

  def __init__(self, frame_rate: float, refresh_rate: float):
    """
        Initializes the generator with the given rates.

        Args:
            frame_rate: The frame rate of the video source in frames per second.
            refresh_rate: The refresh rate of the display in Hz.
        """
    if frame_rate <= 0 or refresh_rate <= 0:
      raise ValueError("Frame rate and refresh rate must be positive.")
    self.refresh_rate = float(refresh_rate)
    self.frame_rate = float(frame_rate)
    self.frame_index = 0

  def get_number_of_times_current_frame_displays(self) -> int:
    """
        Calculates how many display refreshes the current video frame should be
        shown for, by simulating the display's refresh ticks.

        This method directly ports the logic from the C++ implementation.
        """
    current_frame_display_times = 0

    current_frame_time = self.frame_index / self.frame_rate
    next_frame_time = (self.frame_index + 1) / self.frame_rate

    refresh_ticks = int(
        (current_frame_time - 1 / self.refresh_rate) * self.refresh_rate)

    max_iterations = 120
    for _ in range(max_iterations):
      refresh_time = refresh_ticks / self.refresh_rate
      if refresh_time >= next_frame_time:
        break
      if refresh_time >= current_frame_time:
        current_frame_display_times += 1
      refresh_ticks += 1

    return current_frame_display_times

  def advance_to_next_frame(self):
    """Advances the internal counter to the next video frame."""
    self.frame_index += 1


def main():
  """Main function to parse arguments and print the cadence."""
  parser = argparse.ArgumentParser(
      description="Calculate the video frame release cadence.",
      formatter_class=argparse.RawTextHelpFormatter,
      epilog="""
Example Usage:
  # Classic 3:2 pulldown for film on NTSC displays
  python video_frame_cadence_pattern_generator.py --frame-rate 24 --refresh-rate 60
  # Output: 3 2

  # Smooth playback for 25fps content on a 60Hz display
  python video_frame_cadence_pattern_generator.py --frame-rate 25 --refresh-rate 60
  # Output: 3 2 3 2 2

  # Standard NTSC drop-frame video on a true 60Hz display
  python video_frame_cadence_pattern_generator.py --frame-rate 29.97 --refresh-rate 60
  # Output: A long pattern of 2s with an occasional 3.
""")
  parser.add_argument(
      "--frame-rate",
      type=float,
      required=True,
      help="The frame rate of the video (e.g., 23.976, 24, 30).")
  parser.add_argument(
      "--refresh-rate",
      type=float,
      required=True,
      help="The refresh rate of the display (e.g., 59.94, 60, 120).")
  args = parser.parse_args()

  try:
    generator = VideoFrameCadencePatternGenerator(args.frame_rate,
                                                  args.refresh_rate)

    # The cadence pattern repeats after a certain number of frames. This
    # number is the denominator of the simplified fraction of
    # (refresh_rate / frame_rate).
    simplified_fraction = Fraction(args.refresh_rate /
                                   args.frame_rate).limit_denominator()
    pattern_length = simplified_fraction.denominator

    print(f"Video Frame Rate:   {args.frame_rate} fps")
    print(f"Display Refresh Rate: {args.refresh_rate} Hz")
    print("-" * 40)
    print(f"Cadence pattern repeats every {pattern_length} video frames:")

    cadence = []
    # Ensure we don't get stuck in a huge loop for complex fractions
    for _ in range(min(pattern_length, 1000)):
      cadence.append(
          str(generator.get_number_of_times_current_frame_displays()))
      generator.advance_to_next_frame()

    print(" ".join(cadence))

  except (ValueError, ZeroDivisionError) as e:
    print(f"Error: {e}")


if __name__ == "__main__":
  main()
