# Copyright 2021 The TensorFlow Authors. All Rights Reserved.
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
"""Test util for tensorflow lite support."""

import os
from absl import flags

FLAGS = flags.FLAGS


def test_srcdir():
  """Returns the path where to look for test data files."""
  if "test_srcdir" in flags.FLAGS:
    return flags.FLAGS["test_srcdir"].value
  elif "TEST_SRCDIR" in os.environ:
    return os.environ["TEST_SRCDIR"]
  else:
    raise RuntimeError("Missing TEST_SRCDIR environment.")


def get_test_data_path(file_or_dirname: str) -> str:
  """Return full test data path."""
  for (directory, subdirs, files) in os.walk(test_srcdir()):
    for f in subdirs + files:
      if f.endswith(file_or_dirname):
        return os.path.join(directory, f)
  raise ValueError("No %s in test directory" % file_or_dirname)
