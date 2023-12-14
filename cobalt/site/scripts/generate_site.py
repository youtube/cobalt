#!/usr/bin/env python3
# Copyright 2023 The Cobalt Authors. All Rights Reserved.
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
"""Generate the Cobalt Developer site markdown.

This script generates all documentation derived from source, placing
the precipitate markdown files in the specified output directory.
"""

import sys

import cobalt_configuration_public
import cobalt_documentation
import cobalt_gn_configuration
import cobalt_module_reference
import environment


def main(argv):
  environment.setup_logging()
  arguments = environment.parse_arguments(__doc__, argv)
  environment.set_log_level(arguments.log_delta)

  result = cobalt_configuration_public.main(arguments.source, arguments.out)
  if result:
    return result

  result = cobalt_gn_configuration.main(arguments.source, arguments.out)
  if result:
    return result

  result = cobalt_module_reference.generate(arguments.source, arguments.out)
  if result:
    return result

  cobalt_documentation.copy_doc_locations(arguments.source, arguments.out)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
