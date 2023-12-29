"""
    Validates Cobalt METADATA files to schema, and applies Cobalt specific
    rules.
"""
import subprocess
import logging
from google.protobuf import text_format

import os
import sys

log = logging.getLogger(__name__)


def validate_file(metadata_file_path):
  # pylint: disable=import-outside-toplevel
  from tools.metadata.gen import metadata_file_pb2
  metadata = metadata_file_pb2.Metadata()

  with open(metadata_file_path, 'r', encoding='utf-8') as f:
    textproto_content = f.read()
    text_format.Parse(textproto_content, metadata)

  if not metadata.name:
    log.warning('`name` field is missing')
  if not metadata.description:
    log.warning('`description` field is missing')
  if not metadata.HasField('third_party'):
    raise RuntimeError('`third_party` field must be present')

  third_party = metadata.third_party
  if not third_party.license_type:
    log.warning('third_party.licence_type is missing')
  if not third_party.version:
    log.warning('third_party.version field must be present')
  if len(third_party.url) > 0:
    log.warning('"url" field is deprecated, please use "identifier" instead')


def pregen():
  cur_dir = os.path.dirname(os.path.abspath(__file__))
  # Generate proto on the fly to ease development
  subprocess.run([
      'protoc', f'--proto_path={cur_dir}/', f'--python_out={cur_dir}/gen/',
      f'{cur_dir}/metadata_file.proto'
  ],
                 text=True,
                 check=False)


if __name__ == '__main__':
  pregen()
  validate_file(sys.argv[1])
