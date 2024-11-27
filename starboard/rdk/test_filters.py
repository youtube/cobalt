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
"""Starboard RDK Platform Test Filters."""

from starboard.tools.testing import test_filter

# pylint: disable=line-too-long
_FILTERED_TESTS = {
    'base_unittests': [
        # TODO: (b/302394149) These cases need to be executed on RDK
        'PersistentHistogramStorageTest.HistogramWriteTest',
        'FileEnumerator.EmptyFolder',
        'FileEnumerator.SingleFileInFolderForFileSearch',
        'FileEnumerator.SingleFileInFolderForDirSearch',
        'FileEnumerator.SingleFolderInFolderForFileSearch',
        'FileEnumerator.SingleFolderInFolderForDirSearch',
        'FileEnumerator.FolderAndFileInFolder',
        'FileEnumerator.FilesInParentFolderAlwaysFirst',
        'FileEnumerator.FileInSubfolder',
        'FileProxyTest.CreateOrOpen_Create',
        'FileProxyTest.CreateOrOpen_Open',
        'FileProxyTest.CreateOrOpen_OpenNonExistent',
        'FileProxyTest.CreateOrOpen_AbandonedCreate',
        'FileProxyTest.Close',
        'FileProxyTest.CreateTemporary',
        'FileProxyTest.SetAndTake',
        'FileProxyTest.GetInfo',
        'FileProxyTest.Read',
        'FileProxyTest.WriteAndFlush',
        'FileProxyTest.SetLength_Shrink',
        'FileProxyTest.SetLength_Expand',
        'FileTest.Create',
        'FileTest.Async',
        'FileTest.ReadWrite',
        'FileTest.Append',
        'FileTest.Length',
        'FileTest.ReadAtCurrentPosition',
        'FileTest.WriteAtCurrentPosition',
        'FileTest.Seek',
        'FileTest.WriteDataToLargeOffset',
        'FileUtilTest.FileAndDirectorySize',
        'FileUtilTest.DeleteNonExistent',
        'FileUtilTest.DeleteNonExistentWithNonExistentParent',
        'FileUtilTest.DeleteFile',
        'FileUtilTest.DeleteDirNonRecursive',
        'FileUtilTest.DeleteDirRecursive',
        'FileUtilTest.DeleteDirRecursiveWithOpenFile',
        'FileUtilTest.CopyFile',
        'FileUtilTest.CreateTemporaryFileTest',
        'FileUtilTest.GetHomeDirTest',
        'FileUtilTest.CreateDirectoryTest',
        'FileUtilTest.DetectDirectoryTest',
        'FileUtilTest.FileEnumeratorTest',
        'FileUtilTest.AppendToFile',
        'FileUtilTest.ReadFile',
        'FileUtilTest.ReadFileToString',
        'FileUtilTest.ReadFileToStringWithUnknownFileSize',
        'FileUtilTest.ReadFileToStringWithLargeFile',
        'FileUtilTest.TouchFile',
        'FileUtilTest.IsDirectoryEmpty',
        'ImportantFileWriterTest.Basic',
        'ImportantFileWriterTest.WriteWithObserver',
        'ImportantFileWriterTest.CallbackRunsOnWriterThread',
        'ImportantFileWriterTest.ScheduleWrite',
        'ImportantFileWriterTest.DoScheduledWrite',
        'ImportantFileWriterTest.BatchingWrites',
        'ImportantFileWriterTest.ScheduleWrite_FailToSerialize',
        'ImportantFileWriterTest.ScheduleWrite_WriteNow',
        'ImportantFileWriterTest.DoScheduledWrite_FailToSerialize',
        'ScopedTempDir.FullPath',
        'ScopedTempDir.TempDir',
        'ScopedTempDir.UniqueTempDirUnderPath',
        'ScopedTempDir.MultipleInvocations',
        'JSONValueDeserializerTest.ReadProperJSONFromFile',
        'JSONValueDeserializerTest.ReadJSONWithCommasFromFile',
        'JSONFileValueSerializerTest.Roundtrip',
        'JSONFileValueSerializerTest.RoundtripNested',
        'JSONFileValueSerializerTest.NoWhitespace',
        'PathServiceTest.Get',
        'PathServiceTest.Override',
        'PathServiceTest.OverrideMultiple',
        'PathServiceTest.RemoveOverride',
        'SysInfoTest.AmountOfFreeDiskSpace',
        'SysInfoTest.AmountOfTotalDiskSpace',
    ],
}


class TestFilters(object):
  """Starboard RDK platform test filters."""

  def GetTestFilters(self):
    filters = []
    for target, tests in _FILTERED_TESTS.items():
      filters.extend(test_filter.TestFilter(target, test) for test in tests)
    return filters


def CreateTestFilters():
  return TestFilters()
