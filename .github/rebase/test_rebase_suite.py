#!/usr/bin/env python3
# pylint: disable=duplicate-code
"""Comprehensive test suite for Cobalt Chromium rebase automation tools."""

import ast
import os
import tempfile
import unittest

from autoninja_loop import apply_patch_or_replacement, parse_compiler_errors
from resolve_conflicts import (
    detect_language,
    execute_local_tool,
    extract_conflict_blocks,
    resolve_file_conflicts,
    TokenUsage,
)

SAMPLE_DEPS_CONFLICT = """git_dependencies = "SYNC"

vars = {
  "build_with_chromium": True,
  "checkout_cobalt_internal": False,
<<<<<<< HEAD
  "skia_revision": "aaaa111122223333444455556666777788889999",
=======
  "skia_revision": "bbbb111122223333444455556666777788889999",
>>>>>>> Update to 139.7244.
  "checkout_copybara": False,
}
"""

SAMPLE_CPP_CONFLICT = """#include "base/logging.h"

void InitializeMedia() {
<<<<<<< HEAD
#if BUILDFLAG(USE_STARBOARD_MEDIA)
  InitStarboardMediaPipeline();
#endif
=======
  InitChromiumDefaultMediaPipeline(true);
>>>>>>> Update to 139.7244.
}
"""

SAMPLE_COMPILER_OUTPUT = (
    "[1420/5400] CXX obj/content/browser/keep_alive_url_loader_service.o\n"
    "../../content/browser/loader/keep_alive_url_loader_service.cc:123:45: "
    "error: no matching function for call to \"NavigationThrottle\"\n"
    "  NavigationThrottle throttle(registry);\n"
    "                     ^~~~~~~~\n"
    "../../content/public/browser/navigation_throttle.h:45:3: note: candidate "
    "function not viable\n"
    "  NavigationThrottle(NavigationThrottleRegistry& registry);\n"
    "  ^\n"
    "[1421/5400] CXX obj/media/audio/audio/audio_manager_android.o\n")


class TestRebaseAutomationSuite(unittest.TestCase):
  """Comprehensive unit test suite for Cobalt rebase automation."""

  def test_language_detection(self):
    """Tests language identification from file extension."""
    self.assertEqual(detect_language("DEPS"), "Python (Chromium DEPS)")
    self.assertEqual(detect_language("cobalt/browser/main.cc"), "C++")
    self.assertEqual(detect_language("cobalt/BUILD.gn"), "GN Build File")
    self.assertEqual(detect_language("cobalt/App.java"), "Java")

  def test_conflict_extraction(self):
    """Tests extraction of conflict blocks with context."""
    blocks = extract_conflict_blocks(SAMPLE_DEPS_CONFLICT)
    self.assertEqual(len(blocks), 1)
    self.assertEqual(blocks[0].index, 1)
    self.assertIn("aaaa1111", blocks[0].ours_content)
    self.assertIn("bbbb1111", blocks[0].theirs_content)

  def test_local_tool_read_file(self):
    """Tests execution of local file inspection tool."""
    with tempfile.NamedTemporaryFile("w+", delete=False) as tmp:
      tmp.write("line 1\nline 2\nline 3\nline 4\nline 5\n")
      tmp_path = tmp.name

    try:
      res = execute_local_tool(
          f"TOOL_READ_FILE: {tmp_path} 2-4",
          repo_path=os.path.dirname(tmp_path),
      )
      self.assertIn("line 2", res)
      self.assertIn("line 4", res)
      self.assertNotIn("line 1", res)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_deps_mock_resolution_and_ast(self):
    """Tests mock DEPS resolution and syntax verification."""
    with tempfile.NamedTemporaryFile("w+", suffix="DEPS", delete=False) as tmp:
      tmp.write(SAMPLE_DEPS_CONFLICT)
      tmp_path = tmp.name

    try:
      tracker = TokenUsage()
      escalations = []
      ok, _ = resolve_file_conflicts(
          file_path=tmp_path,
          repo_path=os.path.dirname(tmp_path),
          git_context="",
          skills_dir=None,
          api_key=None,
          model="gemini-flash-latest",
          timeout_seconds=60,
          max_retries=5,
          mock_mode=True,
          token_tracker=tracker,
          escalations=escalations,
      )
      self.assertTrue(ok)
      self.assertEqual(len(escalations), 0)
      with open(tmp_path, "r", encoding="utf-8") as f:
        resolved = f.read()

      self.assertNotIn("<<<<<<<", resolved)
      self.assertNotIn(">>>>>>>", resolved)
      self.assertIn("bbbb1111", resolved)
      self.assertIn("checkout_copybara", resolved)

      # Validate AST syntax
      tree = ast.parse(resolved)
      self.assertIsNotNone(tree)
      self.assertEqual(tracker.calls, 1)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_source_mock_resolution(self):
    """Tests mock C++ conflict resolution."""
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write(SAMPLE_CPP_CONFLICT)
      tmp_path = tmp.name

    try:
      tracker = TokenUsage()
      escalations = []
      ok, _ = resolve_file_conflicts(
          file_path=tmp_path,
          repo_path=os.path.dirname(tmp_path),
          git_context="",
          skills_dir=None,
          api_key=None,
          model="gemini-flash-latest",
          timeout_seconds=60,
          max_retries=5,
          mock_mode=True,
          token_tracker=tracker,
          escalations=escalations,
      )
      self.assertTrue(ok)
      self.assertEqual(len(escalations), 0)
      with open(tmp_path, "r", encoding="utf-8") as f:
        resolved = f.read()

      self.assertNotIn("<<<<<<<", resolved)
      self.assertNotIn(">>>>>>>", resolved)
      self.assertIn("InitChromiumDefaultMediaPipeline", resolved)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_compiler_error_parsing(self):
    """Tests parsing of Clang error logs."""
    diags = parse_compiler_errors(SAMPLE_COMPILER_OUTPUT, repo_path="/repo")
    self.assertEqual(len(diags), 1)
    self.assertEqual(diags[0].line_number, 123)
    self.assertIn("NavigationThrottle", diags[0].error_message)

  def test_gn_search_replace_with_slash_prefix(self):
    """Tests search and replace patch application on GN files."""
    with tempfile.NamedTemporaryFile(
        "w+", suffix="BUILD.gn", delete=False) as tmp:
      tmp.write("deps = [\n  \"//third_party/mesa_headers\",\n"
                "  \"//third_party/re2\",\n]\n")
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      ai_patch = f"""FILE: //{rel}
<<<<<<< SEARCH
deps = [
  "//third_party/mesa_headers",
  "//third_party/re2",
]
=======
deps = [
  "//third_party/re2",
]
>>>>>>> REPLACE"""
      modified = apply_patch_or_replacement(ai_patch, repo_path=parent)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("mesa_headers", content)
      self.assertIn("third_party/re2", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)


if __name__ == "__main__":
  unittest.main()
