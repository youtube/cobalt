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
  logging.info(metadata_file_path)

  with open(metadata_file_path, 'r', encoding='utf-8') as f:
    textproto_content = f.read()
    text_format.Parse(textproto_content, metadata)

  if not metadata.name:
    log.warning('%s: `name` field should be present', metadata_file_path)
  if not metadata.description:
    log.warning('%s: `description` field should be present', metadata_file_path)
  if not metadata.HasField('third_party'):
    raise RuntimeError('`third_party` field must be present')

  third_party = metadata.third_party
  if not third_party.license_type:
    log.warning('%s: third_party.licence_type is missing', metadata_file_path)
  if not third_party.version:
    log.warning('%s: third_party.version field should be present',
                metadata_file_path)
  # if len(third_party.url) > 0:
  #  log.warning('"url" field is deprecated, please use "identifier" instead')


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
  validate_files = sys.argv[1:]
  # pregen()
  res = [validate_file(file) for file in validate_files]
