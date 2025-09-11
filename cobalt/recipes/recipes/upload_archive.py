"""Testing if we can use recipes
"""
from google.protobuf import json_format
from PB.recipe_modules.build.archive.properties import ArchiveData

DEPS = [
    'recipe_engine/step', 'recipe_engine/file', 'recipe_engine/json',
    'recipe_engine/path', 'recipe_engine/platform', 'recipe_engine/properties',
    'build/archive'
]


def RunSteps(api):
  api.step('Print testing archive API', ['echo', 'just', 'a', 'test'])
  archive_spec_path = 'infra/archive_config/android-archive-rel.json'
  abs_archive = api.path.abspath(archive_spec_path)
  checkout_dir = api.path.abs_to_path(api.path.abspath('.'))
  source_dir = api.path.abs_to_path(api.path.abspath('.'))
  build_dir = api.path.abs_to_path(api.path.abspath('out/Android'))
  print('checkout_dir:', checkout_dir)
  print('source_dir:', source_dir)
  print('build_dir:', build_dir)
  update_properties = {}

  archive_spec = api.file.read_json(
      'read archive spec (%s)' % api.path.basename(abs_archive),
      abs_archive,
      test_data={})
  source_side_config = json_format.ParseDict(
      archive_spec, ArchiveData(), ignore_unknown_fields=True)

  api.archive.gcs_archive(
      checkout_dir=checkout_dir,
      source_dir=source_dir,
      build_dir=build_dir,
      update_properties=update_properties,
      archive_data=source_side_config)


def GenTests(api):
  return ()
