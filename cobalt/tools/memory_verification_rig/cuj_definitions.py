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
"""Common Critical User Journey (CUJ) specifications for Cobalt."""

CUJS = {
    "browse": {
        "name": "BROWSE CUJ (Randomized UI Navigation)",
        "url": "https://www.youtube.com/tv",
        "is_random_nav": True,
    },
    "watch": {
        "name": "WATCH CUJ (Direct Video Playback)",
        "url": "https://www.youtube.com/tv?v=nxcKFjP5QrE",
        "is_random_nav": False,
    },
    "baseline": {
        "name": "BASELINE CUJ (Idle about:blank)",
        "url": "about:blank",
        "is_random_nav": False,
    },
    "combined": {
        "name": "COMBINED CUJ (Watch Playback + UI Browse)",
        "url": "https://www.youtube.com/tv?v=nxcKFjP5QrE",
        "is_random_nav": True,
    },
    "multi_watch": {
        "name": "MULTI-WATCH CUJ (Dynamic Video Hopping)",
        "url": "https://www.youtube.com/tv?v=nxcKFjP5QrE",
        "is_random_nav": False,
        "is_playlist": True,
    },
}
