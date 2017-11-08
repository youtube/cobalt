#
# Copyright 2017 Google Inc. All Rights Reserved.
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
"""Class for configuring Webdriver Benchmarks."""

PERFORMANCE_TEST = 'performance'
PRELOAD_TEST = 'preload'
SANITY_TEST = 'sanity'

# WEBDRIVER SCRIPT CONFIGURATION PARAMETERS
MINIMAL_SIZE = 'minimal'
REDUCED_SIZE = 'reduced'
STANDARD_SIZE = 'standard'

DISABLE_VIDEOS = '--disable_videos'

SAMPLE_SIZES = [MINIMAL_SIZE, REDUCED_SIZE, STANDARD_SIZE]

# COBALT COMMAND LINE PARAMETERS
DISABLE_SPLASH_SCREEN_ON_RELOADS = '--disable_splash_screen_on_reloads'
