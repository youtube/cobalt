import unittest
from unittest.mock import patch, MagicMock
import os
import tempfile
import shutil
from rebase import find_existing_patch, get_conflicted_files

class TestRebase(unittest.TestCase):

    def setUp(self):
        self.test_dir = tempfile.mkdtemp()
        self.patch_dir = os.path.join(self.test_dir, "patch")
        os.makedirs(self.patch_dir)

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_find_existing_patch_simple(self):
        """Test finding a patch with the simple name format."""
        file_path = "path/to/some_file.cc"
        patch_name = "some_file_resolution.patch"
        patch_path = os.path.join(self.patch_dir, patch_name)
        with open(patch_path, "w") as f:
            f.write("patch content")

        found_patch = find_existing_patch(self.patch_dir, file_path)
        self.assertEqual(found_patch, patch_path)

    def test_find_existing_patch_timestamped(self):
        """Test finding a patch with the old timestamped format."""
        file_path = "path/to/another_file.py"
        patch_name = "another_file_resolution_20250101120000.patch"
        patch_path = os.path.join(self.patch_dir, patch_name)
        with open(patch_path, "w") as f:
            f.write("patch content")

        found_patch = find_existing_patch(self.patch_dir, file_path)
        self.assertEqual(found_patch, patch_path)

    def test_find_existing_patch_prefers_simple(self):
        """Test that the simple format is preferred when both exist."""
        file_path = "path/to/mixed_file.txt"
        simple_patch_name = "mixed_file_resolution.patch"
        simple_patch_path = os.path.join(self.patch_dir, simple_patch_name)
        with open(simple_patch_path, "w") as f:
            f.write("simple patch")

        timestamped_patch_name = "mixed_file_resolution_20250101120000.patch"
        timestamped_patch_path = os.path.join(self.patch_dir, timestamped_patch_name)
        with open(timestamped_patch_path, "w") as f:
            f.write("timestamped patch")

        found_patch = find_existing_patch(self.patch_dir, file_path)
        self.assertEqual(found_patch, simple_patch_path)

    def test_find_existing_patch_not_found(self):
        """Test that None is returned when no patch is found."""
        file_path = "path/to/non_existent.file"
        found_patch = find_existing_patch(self.patch_dir, file_path)
        self.assertIsNone(found_patch)

    @patch('git.Repo')
    def test_get_conflicted_files(self, mock_repo):
        """Test that conflicted files are correctly identified."""
        # Mock the output of 'git status --porcelain'
        mock_repo.git.status.return_value = (
            "UU path/to/file1.cc\n"
            "AA path/to/file2.py\n"
            "DD path/to/file3.txt\n"
            "UD path/to/file4.h\n"
            "DU path/to/file5.gn\n"
            " M path/to/modified.file\n"
            " A path/to/added.file\n"
            "?? path/to/untracked.file"
        )
        
        conflicted = get_conflicted_files(mock_repo)
        
        expected_files = [
            "path/to/file1.cc",
            "path/to/file2.py",
            "path/to/file3.txt",
            "path/to/file4.h",
            "path/to/file5.gn"
        ]
        
        self.assertEqual(sorted(conflicted), sorted(expected_files))

    @patch('git.Repo')
    def test_get_conflicted_files_no_conflicts(self, mock_repo):
        """Test that an empty list is returned when there are no conflicts."""
        mock_repo.git.status.return_value = (
            " M path/to/modified.file\n"
            " A path/to/added.file\n"
            "?? path/to/untracked.file"
        )
        
        conflicted = get_conflicted_files(mock_repo)
        self.assertEqual(conflicted, [])

    @patch('git.Repo')
    def test_get_conflicted_files_empty_status(self, mock_repo):
        """Test that an empty list is returned for an empty status output."""
        mock_repo.git.status.return_value = ""
        conflicted = get_conflicted_files(mock_repo)
        self.assertEqual(conflicted, [])

if __name__ == '__main__':
    unittest.main()
