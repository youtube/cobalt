#!/usr/bin/python
# Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
"""Initialization for the starboard package."""

# Provide a holder for the config package to insert an adapter for platforms
# that refer to the deprecated config.starboard.PlatformConfigStarboard.
PlatformConfigStarboard = None  # pylint: disable=invalid-name
