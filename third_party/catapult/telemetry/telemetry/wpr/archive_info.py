# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import absolute_import
import json
import logging
import os
import shutil
import tempfile
import time
import six

from py_utils import cloud_storage  # pylint: disable=import-error


_DEFAULT_PLATFORM = 'DEFAULT'
_ALL_PLATFORMS = ['mac', 'linux', 'android', 'win', _DEFAULT_PLATFORM]


def AssertValidCloudStorageBucket(bucket):
  is_valid = bucket in (None,
                        cloud_storage.PUBLIC_BUCKET,
                        cloud_storage.PARTNER_BUCKET,
                        cloud_storage.INTERNAL_BUCKET)
  if not is_valid:
    raise ValueError("Cloud storage privacy bucket %s is invalid" % bucket)


class WprArchiveInfo():
  def __init__(self, file_path, data, bucket):
    AssertValidCloudStorageBucket(bucket)
    self._file_path = file_path
    self._base_dir = os.path.dirname(file_path)
    self._data = data
    self._bucket = bucket
    self.temp_target_wpr_file_path = None
    # Ensure directory exists.
    if not os.path.exists(self._base_dir):
      os.makedirs(self._base_dir)

    assert data.get('platform_specific', False), (
        'Detected old version of archive info json file. Please update to new '
        'version.')

    self._story_name_to_wpr_file = data['archives']

  @property
  def data(self):
    return self._data

  @classmethod
  def FromFile(cls, file_path, bucket):
    """ Generates an archive_info instance with the given json file. """
    if os.path.exists(file_path):
      with open(file_path, 'r') as f:
        data = json.load(f)
        return cls(file_path, data, bucket)
    return cls(file_path, {'archives': {}, 'platform_specific': True}, bucket)

  def DownloadArchivesIfNeeded(self, target_platforms=None, story_names=None):
    """Downloads archives iff the Archive has a bucket parameter and the user
    has permission to access the bucket.

    Raises cloud storage Permissions or Credentials error when there is no
    local copy of the archive and the user doesn't have permission to access
    the archive's bucket.

    Warns when a bucket is not specified or when the user doesn't have
    permission to access the archive's bucket but a local copy of the archive
    exists.

    Args:
      target_platform: only downloads archives for these platforms
      story_names: only downloads archives for these story names
    """
    logging.info('Downloading WPR archives. This can take a long time.')
    start_time = time.time()
    # If no target platform is set, download all platforms.
    if target_platforms is None:
      target_platforms = _ALL_PLATFORMS
    else:
      assert isinstance(target_platforms, list), 'Must pass platforms as a list'
      target_platforms = target_platforms + [_DEFAULT_PLATFORM]
    # Download all .wprgo files.
    if not self._bucket:
      logging.warning('Story set in %s has no bucket specified, and '
                      'cannot be downloaded from cloud_storage.', )
      return
    assert 'archives' in self._data, ("Invalid data format in %s. 'archives' "
                                      "field is needed" % self._file_path)

    def download_if_needed(path):
      try:
        cloud_storage.GetIfChanged(path, self._bucket)
      except (cloud_storage.CredentialsError,
              cloud_storage.CloudStoragePermissionError):
        if os.path.exists(path):
          # If the archive exists, assume the user recorded their own and warn
          # them that they do not have the proper credentials to download.
          logging.warning('Need credentials to update WPR archive: %s', path)
        else:
          logging.error("You either aren't authenticated or don't have "
                        "permission to use the archives for this page set."
                        "\nYou may need to run gsutil config."
                        "\nYou can find instructions for gsutil config at: "
                        "http://www.chromium.org/developers/telemetry/"
                        "upload_to_cloud_storage")
          raise

    try:
      story_archives = self._data['archives']
      download_names = set(six.iterkeys(story_archives))
      if story_names is not None:
        download_names.intersection_update(story_names)
      for story_name in download_names:
        for target_platform in target_platforms:
          if story_archives[story_name].get(target_platform):
            archive_path = self._WprFileNameToPath(
                story_archives[story_name][target_platform])
            download_if_needed(archive_path)
    finally:
      logging.info('All WPR archives are downloaded, took %s seconds.',
                   time.time() - start_time)

  def WprFilePathForStory(self, story, target_platform=_DEFAULT_PLATFORM):
    if self.temp_target_wpr_file_path:
      return self.temp_target_wpr_file_path

    wpr_file = self._story_name_to_wpr_file.get(story.name, None)
    if wpr_file:
      if target_platform in wpr_file:
        return self._WprFileNameToPath(wpr_file[target_platform])
      return self._WprFileNameToPath(wpr_file[_DEFAULT_PLATFORM])
    return None

  def AddNewTemporaryRecording(self, temp_wpr_file_path=None):
    if temp_wpr_file_path is None:
      temp_wpr_file_handle, temp_wpr_file_path = tempfile.mkstemp()
      os.close(temp_wpr_file_handle)
    self.temp_target_wpr_file_path = temp_wpr_file_path

  def AddRecordedStories(self, stories, upload_to_cloud_storage=False,
                         target_platform=_DEFAULT_PLATFORM):
    if not stories:
      os.remove(self.temp_target_wpr_file_path)
      return

    target_wpr_file_hash = cloud_storage.CalculateHash(
        self.temp_target_wpr_file_path)
    (target_wpr_file, target_wpr_file_path) = self._NextWprFileName(
        target_wpr_file_hash)
    for story in stories:
      # Check to see if the platform has been manually overrided.
      if not story.platform_specific:
        current_target_platform = _DEFAULT_PLATFORM
      else:
        current_target_platform = target_platform
      self._SetWprFileForStory(
          story.name, target_wpr_file, current_target_platform)
    shutil.move(self.temp_target_wpr_file_path, target_wpr_file_path)

    # Update the hash file.
    with open(target_wpr_file_path + '.sha1', 'wb') as f:
      f.write(target_wpr_file_hash.encode('utf-8'))
      f.flush()

    self._WriteToFile()

    # Upload to cloud storage
    if upload_to_cloud_storage:
      if not self._bucket:
        logging.warning('StorySet must have bucket specified to upload '
                        'stories to cloud storage.')
        return
      try:
        cloud_storage.Insert(self._bucket, target_wpr_file_hash,
                             target_wpr_file_path)
      except cloud_storage.CloudStorageError as e:
        logging.warning('Failed to upload wpr file %s to cloud storage. '
                        'Error:%s' % target_wpr_file_path, e)

  def RemoveStory(self, story):
    story_archives = self._data['archives']
    if story not in story_archives:
      logging.error("Story does not exist in archive!")
      return

    del story_archives[story]
    self._WriteToFile()

  def _WriteToFile(self):
    """Writes the metadata into the file passed as constructor parameter."""
    metadata = dict()
    metadata['description'] = (
        'Describes the Web Page Replay archives for a story set. '
        'Don\'t edit by hand! Use record_wpr for updating.')
    metadata['archives'] = self._story_name_to_wpr_file.copy()
    metadata['platform_specific'] = True

    with open(self._file_path, 'w') as f:
      json.dump(metadata, f, indent=4, sort_keys=True, separators=(',', ': '))
      f.flush()

  def _WprFileNameToPath(self, wpr_file):
    return os.path.abspath(os.path.join(self._base_dir, wpr_file))

  def _NextWprFileName(self, file_hash):
    """Creates a new file name for a wpr archive file."""
    base = os.path.splitext(os.path.basename(self._file_path))[0]
    new_filename = '%s_%s.%s' % (base, file_hash[:10], 'wprgo')
    return new_filename, self._WprFileNameToPath(new_filename)

  def _SetWprFileForStory(self, story_name, wpr_file, target_platform):
    """For modifying the metadata when we're going to record a new archive."""
    if story_name not in self._data['archives']:
      # If there is no other recording we want the first to be the default
      # until a new default is recorded.
      self._data['archives'][story_name] = {_DEFAULT_PLATFORM: wpr_file}
    self._data['archives'][story_name][target_platform] = wpr_file
