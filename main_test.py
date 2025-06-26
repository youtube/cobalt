import unittest
from unittest.mock import MagicMock
from main import are_summaries_similar, find_deepest_common_dir

class TestMain(unittest.TestCase):

    def test_are_summaries_similar(self):
        """Test the commit summary similarity function."""
        
        # Exact match
        self.assertTrue(are_summaries_similar(
            "feat: Add new feature",
            "feat: Add new feature"
        ))
        
        # Match with PR number
        self.assertTrue(are_summaries_similar(
            "feat: Add new feature (#123)",
            "feat: Add new feature"
        ))
        
        # Match with different PR numbers
        self.assertTrue(are_summaries_similar(
            "feat: Add new feature (#123)",
            "feat: Add new feature (#456)"
        ))
        
        # Case-insensitive match
        self.assertTrue(are_summaries_similar(
            "Feat: Add new feature",
            "feat: add new feature"
        ))
        
        # Substring match (e.g., rebase artifacts) - This is not supported by the function
        self.assertFalse(are_summaries_similar(
            "Revert \"feat: Add new feature\"",
            "feat: Add new feature"
        ))
        
        # Should not match
        self.assertFalse(are_summaries_similar(
            "feat: Add a completely different feature",
            "fix: A bug fix"
        ))
        
        # Should not match with short strings
        self.assertFalse(are_summaries_similar(
            "short",
            "a short"
        ))

    def test_find_deepest_common_dir(self):
        """Test the find_deepest_common_dir function."""
        
        # Simple case
        self.assertEqual(
            find_deepest_common_dir(["a/b/c/d.txt", "a/b/e/f.txt"]),
            "a/b"
        )
        
        # Deeper common path
        self.assertEqual(
            find_deepest_common_dir(["media/filters/a.cc", "media/filters/b.h"]),
            "media/filters"
        )
        
        # No common path
        self.assertEqual(
            find_deepest_common_dir(["a/b/c.txt", "d/e/f.txt"]),
            ""
        )
        
        # Test with absolute paths
        self.assertEqual(
            find_deepest_common_dir(["/a/b.txt", "/c/d.txt"]),
            "/"
        )
        
        # Single file
        self.assertEqual(
            find_deepest_common_dir(["a/b/c.txt"]),
            "a/b"
        )
        
        # Empty list
        self.assertEqual(
            find_deepest_common_dir([]),
            "/"
        )

if __name__ == '__main__':
    unittest.main()
