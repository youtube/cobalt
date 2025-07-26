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
"""Bounding box protobuf."""

import dataclasses
from typing import Any

from tensorflow_lite_support.cc.task.processor.proto import bounding_box_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls


@dataclasses.dataclass
class BoundingBox:
  """An integer bounding box, axis aligned.

  Attributes:
    origin_x: The X coordinate of the top-left corner, in pixels.
    origin_y: The Y coordinate of the top-left corner, in pixels.
    width: The width of the bounding box, in pixels.
    height: The height of the bounding box, in pixels.
  """

  origin_x: int
  origin_y: int
  width: int
  height: int

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> bounding_box_pb2.BoundingBox:
    """Generates a protobuf object to pass to the C++ layer."""
    return bounding_box_pb2.BoundingBox(
        origin_x=self.origin_x,
        origin_y=self.origin_y,
        width=self.width,
        height=self.height,
    )

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(cls,
                      pb2_obj: bounding_box_pb2.BoundingBox) -> "BoundingBox":
    """Creates a `BoundingBox` object from the given protobuf object."""
    return BoundingBox(
        origin_x=pb2_obj.origin_x,
        origin_y=pb2_obj.origin_y,
        width=pb2_obj.width,
        height=pb2_obj.height)

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, BoundingBox):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
