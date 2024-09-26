# Copyright 2022 The TensorFlow Authors. All Rights Reserved.
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
"""Segmentation options protobuf."""

import dataclasses
import enum
from typing import Any, Optional

from tensorflow_lite_support.cc.task.processor.proto import segmentation_options_pb2
from tensorflow_lite_support.python.task.core.optional_dependencies import doc_controls

_SegmentationOptionsProto = segmentation_options_pb2.SegmentationOptions


class OutputType(enum.Enum):
  UNSPECIFIED = 0
  CATEGORY_MASK = 1
  CONFIDENCE_MASK = 2


@dataclasses.dataclass
class SegmentationOptions:
  """Options for segmentation processor.

  Attributes:
    display_names_locale: The locale to use for display names specified through
      the TFLite Model Metadata.
    output_type: The output mask type allows specifying the type of
      post-processing to perform on the raw model results.
  """

  display_names_locale: Optional[str] = None
  output_type: Optional[OutputType] = OutputType.CATEGORY_MASK

  @doc_controls.do_not_generate_docs
  def to_pb2(self) -> _SegmentationOptionsProto:
    """Generates a protobuf object to pass to the C++ layer."""
    return _SegmentationOptionsProto(
        display_names_locale=self.display_names_locale,
        output_type=self.output_type.value)

  @classmethod
  @doc_controls.do_not_generate_docs
  def create_from_pb2(
      cls, pb2_obj: _SegmentationOptionsProto) -> "SegmentationOptions":
    """Creates a `SegmentationOptions` object from the given protobuf object."""
    return SegmentationOptions(
        display_names_locale=pb2_obj.display_names_locale,
        output_type=OutputType(pb2_obj.output_type))

  def __eq__(self, other: Any) -> bool:
    """Checks if this object is equal to the given object.

    Args:
      other: The object to be compared with.

    Returns:
      True if the objects are equal.
    """
    if not isinstance(other, SegmentationOptions):
      return False

    return self.to_pb2().__eq__(other.to_pb2())
