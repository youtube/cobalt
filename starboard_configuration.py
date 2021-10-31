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
# limitations under the License."""
"""Definition of interface used by host apps to configure Starboard"""

from cobalt.build import cobalt_configuration

# List of paths to directories in which Starboard ports are located. Each path
# should be represented as a list of directory names whose locations are
# relative to the location of this file.
PORT_ROOTS = [
    ["starboard"],
    ["starboard", "contrib"],
    ["starboard", "port"],
    ["third_party", "starboard"]
]

# A Mapping of application names to cross-platform ApplicationConfiguration
# subclasses (or factory functions).
#
# For more information, see starboard/doc/building.md.
APPLICATIONS = {
    cobalt_configuration.APPLICATION_NAME:
        cobalt_configuration.CobaltConfiguration,
}
