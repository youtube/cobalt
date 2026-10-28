#!/usr/bin/env python3
"""Comprehensive test suite for Cobalt Chromium rebase automation tools."""

import ast
import os
import subprocess
import tempfile
import unittest
from unittest import mock

from autoninja import (
    AutoninjaResolver,
    CompilerDiagnostic,
    find_build_file_for_object,
    parse_compiler_errors,
)
from base_resolver import (
    EXPERT_MAX_ROUNDS,
    BaseResolver,
    extract_meaningful_error_summary,
)
from change_record import (
    NO_RATIONALE,
    AgentChangeRecord,
    format_change_history,
    guidance_summary,
)
from diagnostics import Diagnostic
from patching import (
    NEEDS_HUMAN_PREFIX,
    apply_parsed_patch,
    apply_search_replace,
    exec_script_changes,
    parse_patch,
    patch_file_changes,
    patch_explanation,
)
from repo_guards import (
    _COBALT_GIT_HISTORY_CACHE,
    _find_unique_tracked_file,
    has_cobalt_git_history,
    is_unmodified_third_party,
    is_within_repo,
    patch_target_rejection,
    resolve_repo_file_path,
)
from run_rebase_pipeline import (format_change_log, get_chromium_milestone,
                                 restore_files_outside_build,
                                 write_rebase_report)
from tools import (
    execute_local_tool,
    extract_line_anchored_tool_commands,
    extract_tool_commands,
)
from conflicts import (
    ConflictResolver,
    detect_language,
    extract_conflict_blocks,
    resolve_file_conflicts,
    sort_conflict_priority,
)
from engine_client import ReasoningEngineClient
from fix_commits import (
    FixCommitter,
    commit_files,
    conflict_commit_message,
    conflict_rationales,
    fix_commit_message,
    fix_rationales,
    parse_judgement,
)
from gclient_sync import GClientSyncDiagnostic
import upstream_cl
from gn_gen import GNDiagnostic, GNGenResolver, extract_gn_target_files
from reasoning_engine import CobaltReasoningEngine
from token_usage import TokenUsage

# ReasoningEngineClient has no default models; tests pass placeholders.
_TEST_MODELS = {"flash_model": "flash-test", "expert_model": "expert-test"}


def _apply(patch_text, repo_path, default_file=None):
  """Parses and applies a model patch, as the resolver does."""
  return apply_parsed_patch(
      parse_patch(patch_text, repo_path, default_file), repo_path)


SAMPLE_DEPS_CONFLICT = """git_dependencies = "SYNC"

vars = {
  "build_with_chromium": True,
  "checkout_cobalt_internal": False,
<<<<<<< HEAD
  "skia_revision": "aaaa111122223333444455556666777788889999",
=======
  "skia_revision": "bbbb111122223333444455556666777788889999",
>>>>>>> origin/main
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
  InitChromiumDefaultMediaPipeline();
>>>>>>> origin/main
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
      escalations = []
      ok = resolve_file_conflicts(
          file_path=tmp_path,
          repo_path=os.path.dirname(tmp_path),
          git_context="",
          mock_mode=True,
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
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_source_mock_resolution(self):
    """Tests mock C++ conflict resolution."""
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write(SAMPLE_CPP_CONFLICT)
      tmp_path = tmp.name

    try:
      escalations = []
      ok = resolve_file_conflicts(
          file_path=tmp_path,
          repo_path=os.path.dirname(tmp_path),
          git_context="",
          mock_mode=True,
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

  def test_unmodified_third_party_fast_path(self):
    """Tests fast-path resolution for pure third-party files."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      tp_dir = os.path.join(tmp_dir, "third_party", "xnnpack")
      os.makedirs(tp_dir, exist_ok=True)
      tp_file = os.path.join(tp_dir, "BUILD.gn")
      with open(tp_file, "w", encoding="utf-8") as f:
        f.write("source_set(\"xnnpack\") {\n"
                "<<<<<<< HEAD\n"
                "  sources = [ \"new.c\" ]\n"
                "=======\n"
                "  sources = [ \"old.c\" ]\n"
                ">>>>>>> parent of 65ea0fa (CONFLICTED Chromium Cherry pick: "
                "Revert Cobalt.)\n"
                "}\n")

      escalations = []
      ok = resolve_file_conflicts(
          file_path=tp_file,
          repo_path=tmp_dir,
          git_context="",
          mock_mode=False,  # Should not invoke API at all
          escalations=escalations,
      )
      # With no engine, any model call would fail the resolution.
      self.assertTrue(ok)
      with open(tp_file, "r", encoding="utf-8") as f:
        resolved = f.read()
      self.assertNotIn("<<<<<<<", resolved)
      self.assertNotIn(">>>>>>>", resolved)
      self.assertIn("sources = [ \"new.c\" ]", resolved)

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
>>>>>>> REPLACE
"""
      modified = _apply(ai_patch, repo_path=parent)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("mesa_headers", content)
      self.assertIn("third_party/re2", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_search_replace_sanitizes_rogue_markers(self):
    """Tests that apply_search_replace strips rogue ======= markers."""
    with tempfile.NamedTemporaryFile("w+", suffix=".gn", delete=False) as tmp:
      tmp.write("enable_rust_png = false\n")
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      # Simulates model repeating ======= in replacement
      ai_patch = f"""FILE: //{rel}
<<<<<<< SEARCH
enable_rust_png = false
=======
=======
# enable_rust_png = false
>>>>>>> REPLACE
"""
      modified = _apply(ai_patch, repo_path=parent)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("=======", content)
      self.assertIn("# enable_rust_png = false", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_search_replace_token_normalized_matching(self):
    """Tests matching auto-generated files with quirky internal whitespace."""
    file_content = (
        "DAWN_NO_SANITIZE(\"cfi-icall\")\n"
        "__attribute__((weak)) WGPUStatus  wgpuGetInstanceCapabilities("
        "WGPUInstanceCapabilities * capabilities) {\n"
        "return     procs.getInstanceCapabilities(capabilities);\n"
        "}\n")
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write(file_content)
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      ai_patch = (f"FILE: //{rel}\n"
                  "<<<<<<< SEARCH\n"
                  "55: DAWN_NO_SANITIZE(\"cfi-icall\")\n"
                  "56: __attribute__((weak)) WGPUStatus "
                  "wgpuGetInstanceCapabilities("
                  "WGPUInstanceCapabilities * capabilities) {\n"
                  "57:   return procs.getInstanceCapabilities(capabilities);\n"
                  "58: }\n"
                  "=======\n"
                  "DAWN_NO_SANITIZE(\"cfi-icall\")\n"
                  "__attribute__((weak)) WGPUStatus "
                  "wgpuGetInstanceCapabilities(void* capabilities) {\n"
                  "  return 0;\n"
                  "}\n"
                  ">>>>>>> REPLACE\n")
      modified = _apply(ai_patch, repo_path=parent)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertIn("void* capabilities", content)
      self.assertNotIn("WGPUInstanceCapabilities", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_apply_explicit_delete_block(self):
    """Tests that <<<<<<< DELETE ... >>>>>>> DELETE removes code cleanly."""
    file_content = ("void KeepBefore() {}\n"
                    "void ObsoleteFunc() {\n"
                    "  // To be removed\n"
                    "}\n"
                    "void KeepAfter() {}\n")
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write(file_content)
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      del_patch = f"""FILE: //{rel}
<<<<<<< DELETE
void ObsoleteFunc() {{
  // To be removed
}}
>>>>>>> DELETE
"""
      modified = _apply(del_patch, repo_path=parent)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("ObsoleteFunc", content)
      self.assertIn("KeepBefore", content)
      self.assertIn("KeepAfter", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_reject_empty_search_replace_block(self):
    """Tests that empty REPLACE in SEARCH/REPLACE is rejected as a glitch."""
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write("void Foo() {}\n")
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      glitch_patch = f"""FILE: //{rel}
<<<<<<< SEARCH
void Foo() {{}}
=======
>>>>>>> REPLACE
"""
      modified = _apply(glitch_patch, repo_path=parent)
      self.assertEqual(modified, [])
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertIn("void Foo() {}", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_autoninja_sends_full_file_context_on_repeated_errors(self):
    """Tests sending full source files when error count >= 3."""
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      # Write 100 lines of dummy C++ code
      tmp.write("\n".join(f"// Line {i}" for i in range(1, 101)) + "\n")
      tmp_path = tmp.name

    try:
      repo_dir = os.path.dirname(tmp_path)
      resolver = AutoninjaResolver(
          repo_path=repo_dir,
          out_dir="out/dummy",
          target="cobalt_apk",
      )
      diag = CompilerDiagnostic(
          file_path=tmp_path,
          line_number=50,
          column=1,
          error_message="sample error",
          raw_snippet="snippet",
          notes=[],
      )

      # 1st attempt: should send 60-line window
      called_contexts = []

      class MockEngine:
        flash_model = "gemini-2.5-flash"

        def heal_compiler_error(self, **kwargs):
          called_contexts.append(kwargs.get("file_context", ""))
          return {"status": "SUCCESS", "patch": "", "model_used": "flash"}

      resolver.reasoning_engine = MockEngine()
      resolver.file_error_counts[tmp_path] = 1
      resolver.resolve_diagnostic(diag)
      self.assertTrue(len(called_contexts[0].splitlines()) < 80)

      # 3rd error: should send full 100-line file
      resolver.file_error_counts[tmp_path] = 3
      resolver.resolve_diagnostic(diag)
      self.assertEqual(len(called_contexts[1].splitlines()), 100)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_autoninja_resolves_raw_string_diagnostic(self):
    """Tests that AutoninjaResolver handles unstructured string error traces."""
    resolver = AutoninjaResolver(
        repo_path="/tmp", out_dir="out/test", target="cobalt_apk")
    called_traces = []

    class MockEngine:
      flash_model = "gemini-2.5-flash"

      def heal_compiler_error(self, **kwargs):
        called_traces.append(kwargs.get("error_trace", ""))
        return {"patch": "PATCH", "model_used": "gemini-2.5-flash"}

    resolver.reasoning_engine = MockEngine()
    raw_error = (
        "FAILED: obj/cobalt/apk/cobalt_apk.jar\njavac: package not found")
    patch, model_used, rel_target = resolver.resolve_diagnostic(
        Diagnostic(error_message=raw_error))
    self.assertEqual(patch, "PATCH")
    self.assertEqual(model_used, "gemini-2.5-flash")
    self.assertEqual(rel_target, "cobalt_apk")
    self.assertIn("FAILED: obj/cobalt/apk/cobalt_apk.jar", called_traces[0])

  def test_parse_linker_errors_and_map_build_file(self):
    """Tests that linker errors map to the offending component's BUILD.gn."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      ft_dir = os.path.join(tmp_dir, "third_party", "freetype")
      os.makedirs(ft_dir, exist_ok=True)
      gn_file = os.path.join(ft_dir, "BUILD.gn")
      with open(gn_file, "w", encoding="utf-8") as f:
        f.write("component(\"freetype\") {}\n")

      linker_output = (
          "build step: solink \"./libchrobalt.so\"\n"
          "ld.lld: error: obj/third_party/freetype/libfreetype.a(autofit.o) "
          "is incompatible with armelf_linux_eabi\n"
          "clang++: error: linker command failed with exit code 1")

      diags = parse_compiler_errors(linker_output, tmp_dir)
      self.assertEqual(len(diags), 1)
      self.assertEqual(diags[0].file_path, os.path.abspath(gn_file))
      self.assertIn("incompatible with armelf_linux_eabi",
                    diags[0].error_message)
      self.assertIn("ld.lld: error:", diags[0].raw_snippet)

      # Test direct helper
      found_gn = find_build_file_for_object(
          "obj/third_party/freetype/libfreetype.a(autofit.o)", tmp_dir)
      self.assertEqual(found_gn, os.path.abspath(gn_file))

  def test_gn_stray_conflict_marker_auto_removal(self):
    """Tests that GNGenResolver automatically removes stray conflict markers."""
    with tempfile.NamedTemporaryFile("w+", suffix=".gn", delete=False) as tmp:
      tmp.write("is_cobalt = true\n=======\nenable_vulkan = false\n")
      tmp_path = tmp.name

    try:
      repo_dir = os.path.dirname(tmp_path)
      rel = os.path.basename(tmp_path)
      resolver = GNGenResolver(
          repo_path=repo_dir, platform="android", build_type="devel")
      diag = GNDiagnostic(
          error_message="Unexpected token '=='",
          raw_output=f"ERROR at //{rel}:2:1: Unexpected token '=='",
          target_files={tmp_path: 2},
          is_structural_break=True,
      )
      patch, model_used, rel_target = resolver.resolve_diagnostic(diag)
      self.assertEqual(model_used, "auto-stray-marker-cleaner")
      self.assertEqual(rel_target, rel)
      modified = _apply(patch, repo_path=repo_dir)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("=======", content)
      self.assertIn("is_cobalt = true", content)
      self.assertIn("enable_vulkan = false", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_compiler_stray_conflict_marker_auto_removal(self):
    """Tests that AutoninjaResolver removes stray conflict markers."""
    with tempfile.NamedTemporaryFile("w+", suffix=".cc", delete=False) as tmp:
      tmp.write("#include <iostream>\n=======\nvoid Init() {}\n")
      tmp_path = tmp.name

    try:
      repo_dir = os.path.dirname(tmp_path)
      rel = os.path.basename(tmp_path)
      resolver = AutoninjaResolver(
          repo_path=repo_dir, out_dir="out", target="cobalt")
      diag = CompilerDiagnostic(
          file_path=tmp_path,
          line_number=2,
          column=1,
          error_message="expected unqualified-id",
          raw_snippet="=======",
          notes=[],
      )
      patch, model_used, rel_target = resolver.resolve_diagnostic(diag)
      self.assertEqual(model_used, "auto-stray-marker-cleaner")
      self.assertEqual(rel_target, rel)
      modified = _apply(patch, repo_path=repo_dir)
      self.assertTrue(modified)
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertNotIn("=======", content)
      self.assertIn("#include <iostream>", content)
      self.assertIn("void Init() {}", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_execute_local_tool_multi_arg_grep_and_list_dir(self):
    """Tests execute_local_tool with path filter and list_dir."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      sub_dir = os.path.join(tmp_dir, "content", "browser")
      os.makedirs(sub_dir, exist_ok=True)
      gn_file = os.path.join(sub_dir, "BUILD.gn")
      with open(gn_file, "w", encoding="utf-8") as f:
        f.write('sources = [ "fenced_frame/observer.cc" ]\n')

      # Test TOOL_LIST_DIR
      list_res = execute_local_tool("TOOL_LIST_DIR: content/browser", tmp_dir)
      self.assertIn("BUILD.gn", list_res)

      # Test TOOL_READ_FILE with line numbers
      read_res = execute_local_tool(
          "TOOL_READ_FILE: content/browser/BUILD.gn 1-1", tmp_dir)
      self.assertIn('1: sources = [ "fenced_frame/observer.cc" ]', read_res)

  def test_upstream_diff_skips_conflicted_cherry_pick(self):
    """TOOL_UPSTREAM_DIFF must select the pure upstream roll commit.

    An autoroll lands as three commits: "Revert Cobalt", then
    "Update to <milestone>" (pure upstream), then a "CONFLICTED Cherry
    pick ...: Update to <milestone>" that re-applies Cobalt. Only the
    middle one is pure upstream. Regression test: the original filter
    checked line.startswith("CONFLICTED") against a "%H %s" line that
    always begins with a SHA, so it never excluded anything and the tool
    returned Cobalt's own conflicted merge labelled as upstream.
    """
    with tempfile.TemporaryDirectory() as tmp_dir:

      def git(*args):
        subprocess.run(
            ["git"] + list(args),
            cwd=tmp_dir,
            capture_output=True,
            text=True,
            check=True,
        )

      def commit(content, message):
        with open(
            os.path.join(tmp_dir, "BUILD.gn"), "w", encoding="utf-8") as f:
          f.write(content)
        git("add", "BUILD.gn")
        git("commit", "-m", message)

      git("init")
      git("config", "user.email", "test@example.com")
      git("config", "user.name", "Test")

      commit("mojom(\"mojom\") {\n}\n", "Revert Cobalt.")
      commit(
          "mojom(\"mojom\") {\n}\nUPSTREAM_ONLY_MARKER\n",
          "Update to 143.7471.",
      )
      commit(
          "mojom(\"mojom\") {\n}\nUPSTREAM_ONLY_MARKER\nCOBALT_ONLY_MARKER\n",
          "CONFLICTED Cherry pick commit abc123: Update to 143.7471.",
      )

      res = execute_local_tool("TOOL_UPSTREAM_DIFF: BUILD.gn", tmp_dir)

      # The pure upstream commit introduced UPSTREAM_ONLY_MARKER.
      self.assertIn("UPSTREAM_ONLY_MARKER", res)
      # The CONFLICTED cherry-pick introduced COBALT_ONLY_MARKER. Its
      # presence means the wrong commit was selected.
      self.assertNotIn("COBALT_ONLY_MARKER", res)

      # TOOL_COBALT_DIFF must select the complementary commit: the Cobalt
      # cherry-pick, which is the one that adds COBALT_ONLY_MARKER.
      cobalt_res = execute_local_tool("TOOL_COBALT_DIFF: BUILD.gn", tmp_dir)
      self.assertIn("COBALT_ONLY_MARKER", cobalt_res)
      # UPSTREAM_ONLY_MARKER was already present in the cherry-pick's parent,
      # so it must not appear as an added line in the Cobalt delta.
      self.assertNotIn("+UPSTREAM_ONLY_MARKER", cobalt_res)

  def test_upstream_diff_not_hardcoded_to_milestone_14x(self):
    """Milestone matching must not be hardcoded to 'Update to 14'."""
    with tempfile.TemporaryDirectory() as tmp_dir:

      def git(*args):
        subprocess.run(
            ["git"] + list(args),
            cwd=tmp_dir,
            capture_output=True,
            text=True,
            check=True,
        )

      git("init")
      git("config", "user.email", "test@example.com")
      git("config", "user.name", "Test")
      with open(os.path.join(tmp_dir, "BUILD.gn"), "w", encoding="utf-8") as f:
        f.write("base\n")
      git("add", "BUILD.gn")
      git("commit", "-m", "Revert Cobalt.")
      with open(os.path.join(tmp_dir, "BUILD.gn"), "w", encoding="utf-8") as f:
        f.write("base\nM152_MARKER\n")
      git("add", "BUILD.gn")
      git("commit", "-m", "Update to 152.8000.")

      res = execute_local_tool("TOOL_UPSTREAM_DIFF: BUILD.gn", tmp_dir)
      self.assertIn("M152_MARKER", res)
      self.assertNotIn("Could not find upstream roll commit", res)

  def test_extract_tool_commands_multi_tool_and_think_tags(self):
    """Tests that extract_tool_commands parses tools and strips think tags."""
    glm_output = (
        "<think>\n"
        "I need to read BUILD.gn around line 1060 and check filter_exclude.\n"
        "</think>\n"
        "TOOL_READ_FILE: content/browser/BUILD.gn 1060-1075\n"
        "TOOL_READ_FILE: content/browser/BUILD.gn 3720-3750\n"
        "TOOL_GREP: fenced_frame content/browser/BUILD.gn\n")
    cmds = extract_tool_commands(glm_output)
    self.assertEqual(len(cmds), 3)
    self.assertEqual(cmds[0],
                     "TOOL_READ_FILE: content/browser/BUILD.gn 1060-1075")
    self.assertEqual(cmds[1],
                     "TOOL_READ_FILE: content/browser/BUILD.gn 3720-3750")
    self.assertEqual(cmds[2],
                     "TOOL_GREP: fenced_frame content/browser/BUILD.gn")

    # Test inline tag leak
    inline_leak = ("TOOL_READ_FILE: content/browser/BUILD.gn 1060-1075</think>"
                   "The results confirm\n")
    cmds2 = extract_tool_commands(inline_leak)
    self.assertEqual(len(cmds2), 1)
    self.assertEqual(cmds2[0],
                     "TOOL_READ_FILE: content/browser/BUILD.gn 1060-1075")

    # Test conversational preambles, markdown backticks, and bullet points
    conversational_output = (
        "Let's investigate how `character_data` action works.\n\n"
        "Tool Call: `TOOL_FIND_FILE: icudtl.dat`\n"
        "Tool Call: `TOOL_READ_FILE: "
        "third_party/blink/renderer/platform/text/"
        "character_property_data_generator.cc 50-105`\n"
        "- `TOOL_GREP: character_data "
        "third_party/blink/renderer/platform/BUILD.gn`\n"
        "* Tool: TOOL_LIST_DIR: third_party/icu/common\n")
    # Default cap truncates a batch to _MAX_TOOL_CMDS_PER_TURN directives.
    cmds3 = extract_tool_commands(conversational_output)
    self.assertEqual(len(cmds3), 3)
    # Raising the cap parses every directive in the batch.
    cmds3_all = extract_tool_commands(conversational_output, max_commands=10)
    self.assertEqual(len(cmds3_all), 4)
    self.assertEqual(cmds3_all[0], "TOOL_FIND_FILE: icudtl.dat")
    self.assertEqual(
        cmds3_all[1],
        "TOOL_READ_FILE: third_party/blink/renderer/platform/text/"
        "character_property_data_generator.cc 50-105",
    )
    self.assertEqual(
        cmds3_all[2],
        "TOOL_GREP: character_data "
        "third_party/blink/renderer/platform/BUILD.gn",
    )
    self.assertEqual(cmds3_all[3], "TOOL_LIST_DIR: third_party/icu/common")

    # Test that valid SEARCH/REPLACE block is not mistaken for a tool command
    patch_output = ("Here is the patch to fix the file:\n"
                    "<<<<<<< SEARCH\n"
                    "void InitializeIcu();\n"
                    "=======\n"
                    "void InitializeIcu(const char* exec_path);\n"
                    ">>>>>>> REPLACE\n")
    self.assertEqual(extract_tool_commands(patch_output), [])

  def test_extract_tool_commands_run_on_directives(self):
    """Tests recovery of directives concatenated without separators.

    Regression for the Phase 4 circuit-breaker stall on
    media/mojo/mojom/audio_decoder_config_mojom_traits.cc, where the model
    emitted several TOOL_ directives run together on one line with a trailing
    FILE: patch header glued onto the last one.
    """
    run_on = (
        "TOOL_GREP: struct AudioDecoderConfig media/mojo/mojom/media_types."
        "mojom"
        "TOOL_READ_FILE: media/mojo/mojom/media_types.mojom 240 320"
        "TOOL_READ_FILE: media/mojo/mojom/audio_decoder_config_mojom_traits.h"
        " 1 50")
    cmds = extract_tool_commands(run_on)
    self.assertEqual(len(cmds), 3)
    self.assertEqual(
        cmds[0], "TOOL_GREP: struct AudioDecoderConfig media/mojo/mojom/"
        "media_types.mojom")
    self.assertEqual(
        cmds[1], "TOOL_READ_FILE: media/mojo/mojom/media_types.mojom "
        "240 320")

    # A FILE: patch header glued to the last directive must terminate it.
    run_on_with_header = (
        "TOOL_GREP: mime_type media/mojo/mojom/media_types.mojom"
        "FILE: media/mojo/mojom/media_types.mojom\n"
        "some trailing text")
    cmds_hdr = extract_tool_commands(run_on_with_header)
    self.assertEqual(
        cmds_hdr, ["TOOL_GREP: mime_type media/mojo/mojom/media_types.mojom"])

    # Directives mixed with a patch block yield no tool commands: the patch
    # wins so the investigation loop terminates instead of cycling.
    mixed = (
        run_on + "FILE: media/mojo/mojom/media_types.mojom\n"
        "<<<<<<< SEARCH\n"
        "  bool should_discard_decoder_delay;\n"
        "=======\n"
        "  bool should_discard_decoder_delay;\n"
        "  string mime_type;\n"
        ">>>>>>> REPLACE\n")
    self.assertEqual(extract_tool_commands(mixed), [])

    # DELETE blocks are likewise treated as terminal patches.
    delete_block = ("TOOL_GREP: foo bar\n"
                    "FILE: a.cc\n"
                    "<<<<<<< DELETE\n"
                    "int x = 1;\n"
                    ">>>>>>> DELETE\n")
    self.assertEqual(extract_tool_commands(delete_block), [])

  def test_parse_linker_undefined_symbol_ignores_command_line_noise(self):
    """Tests that linker parser does not match noise in command line."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      content_dir = os.path.join(tmp_dir, "content", "browser")
      os.makedirs(content_dir, exist_ok=True)
      gn_file = os.path.join(content_dir, "BUILD.gn")
      with open(gn_file, "w", encoding="utf-8") as f:
        f.write('source_set("browser") {}\n')

      noisy_output = (
          "FAILED: ./libchrobalt.so\n"
          "clang++ -o ./libchrobalt.so "
          "obj/third_party/rust/cxx/v1/lib/libcxx.rlib\n"
          "ld.lld: error: undefined symbol: "
          "content::FencedFrameViewportObserver::"
          "FencedFrameViewportObserver()\n"
          ">>> referenced by web_contents_impl.cc:1405\n"
          ">>>               obj/content/browser/browser/web_contents_impl.o\n")
      diags = parse_compiler_errors(noisy_output, tmp_dir)
      self.assertEqual(len(diags), 1)
      self.assertEqual(diags[0].file_path, os.path.abspath(gn_file))
      self.assertIn("undefined symbol", diags[0].error_message)

  def test_parse_compiler_errors_universal_catch_all(self):
    """Tests that arbitrary non-standard errors trigger raw trace fallback."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      java_output = (
          "FAILED: obj/cobalt/android/cobalt_apk.javac.jar\n"
          "Traceback (most recent call last):\n"
          "  File '../../build/android/gyp/javac.py', line 45, in <module>\n"
          "    sys.exit(main())\n"
          "Java compilation failed with 2 errors in CobaltActivity.java\n")
      resolver = AutoninjaResolver(
          repo_path=tmp_dir, out_dir="out/test", target="cobalt_apk")
      diags = resolver.extract_diagnostics(
          build_output="", siso_output=java_output)
      self.assertEqual(len(diags), 1)
      self.assertIs(type(diags[0]), Diagnostic)
      self.assertIn("Java compilation failed", diags[0].error_message)

  def test_token_usage_multi_model_tracking(self):
    """Tests that TokenUsage tracks Flash and Pro models separately."""
    tracker = TokenUsage()
    tracker.add(prompt=100, completion=20, total=120, model="gemini-2.5-flash")
    tracker.add(prompt=150, completion=30, total=180, model="gemini-2.5-flash")
    tracker.add(prompt=500, completion=100, total=600, model="gemini-2.5-pro")

    self.assertEqual(tracker.calls, 3)
    self.assertEqual(tracker.prompt_tokens, 750)
    self.assertEqual(tracker.completion_tokens, 150)
    self.assertEqual(tracker.total_tokens, 900)

    self.assertIn("gemini-2.5-flash", tracker.by_model)
    self.assertIn("gemini-2.5-pro", tracker.by_model)
    self.assertEqual(tracker.by_model["gemini-2.5-flash"].calls, 2)
    self.assertEqual(tracker.by_model["gemini-2.5-flash"].total_tokens, 300)
    self.assertEqual(tracker.by_model["gemini-2.5-pro"].calls, 1)
    self.assertEqual(tracker.by_model["gemini-2.5-pro"].total_tokens, 600)

    table = tracker.format_summary_table()
    self.assertIn("`gemini-2.5-flash`", table)
    self.assertIn("`gemini-2.5-pro`", table)
    self.assertIn("**Total**", table)

  def test_get_chromium_milestone(self):
    """Tests reading Chromium major milestone from VERSION file."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      chrome_dir = os.path.join(tmp_dir, "chrome")
      os.makedirs(chrome_dir, exist_ok=True)
      version_file = os.path.join(chrome_dir, "VERSION")
      with open(version_file, "w", encoding="utf-8") as f:
        f.write("MAJOR=138\nMINOR=0\nBUILD=7204\nPATCH=311\n")

      milestone = get_chromium_milestone(tmp_dir)
      self.assertEqual(milestone, "M138")

  def test_extract_gn_target_files_universal(self):
    """Tests that GN file extraction captures all referenced .gn/.gni files."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      buildconfig = os.path.join(tmp_dir, "build/config/BUILDCONFIG.gn")
      rtc_gn = os.path.join(tmp_dir, "third_party/webrtc/rtc_tools/BUILD.gn")
      os.makedirs(os.path.dirname(buildconfig), exist_ok=True)
      os.makedirs(os.path.dirname(rtc_gn), exist_ok=True)
      with open(buildconfig, "w", encoding="utf-8") as f:
        f.write("# buildconfig")
      with open(rtc_gn, "w", encoding="utf-8") as f:
        f.write("# rtc_gn")

      gn_trace = ("ERROR at //build/config/BUILDCONFIG.gn:703:5: "
                  "Source file not found.\n"
                  "See //third_party/webrtc/rtc_tools/BUILD.gn:167:3: for "
                  "'rtp_generator'\n")
      res = extract_gn_target_files(gn_trace, tmp_dir)
      self.assertIn(buildconfig, res)
      self.assertEqual(res[buildconfig], 703)
      self.assertIn(rtc_gn, res)
      self.assertEqual(res[rtc_gn], 167)

  def test_gn_duplicate_object_file_extraction_and_summary(self):
    """Tests 'The target //dir:name' without colon and unique GN summaries."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      buildconfig = os.path.join(tmp_dir, "build/config/BUILDCONFIG.gn")
      gfx_gn = os.path.join(tmp_dir, "ui/gfx/BUILD.gn")
      os.makedirs(os.path.dirname(buildconfig), exist_ok=True)
      os.makedirs(os.path.dirname(gfx_gn), exist_ok=True)
      with open(buildconfig, "w", encoding="utf-8") as f:
        f.write("# buildconfig")
      with open(gfx_gn, "w", encoding="utf-8") as f:
        f.write("# gfx")

      gn_out_1 = ("WARNING: Existing args.gn was overwritten.\n"
                  "Running gn gen out/android-arm_devel --check\n"
                  "ERROR at //build/config/BUILDCONFIG.gn:602:5: "
                  "Duplicate object file\n"
                  "    target(_target_type, target_name) {\n"
                  "    ^----------------------------------\n"
                  "The target //ui/gfx:gfx\n"
                  "generates two object files with the same name:\n"
                  "  clang_x64/obj/ui/gfx/gfx/achoreographer_compat.o\n")
      gn_out_2 = gn_out_1.replace("achoreographer_compat.o", "blit.o")

      # 1. Target files must prioritize ui/gfx/BUILD.gn first and defer
      # BUILDCONFIG.gn to the end.
      files = list(extract_gn_target_files(gn_out_1, tmp_dir).keys())
      self.assertEqual(files, [gfx_gn, buildconfig])

      # 2. Error summaries must include the specific object file so distinct
      # duplicate object files do not compare equal or increment stuck_count.
      resolver = GNGenResolver(tmp_dir, "android-arm", "devel")
      sum1 = resolver.extract_diagnostics(gn_out_1, "")[0].error_message
      sum2 = resolver.extract_diagnostics(gn_out_2, "")[0].error_message
      self.assertNotEqual(sum1, sum2)
      self.assertIn("//ui/gfx:gfx", sum1)
      self.assertIn("achoreographer_compat.o", sum1)
      self.assertIn("blit.o", sum2)

  def test_libcxx_plus_path_and_conflict_newline_and_siso_uuid(self):
    """Tests '+' path parsing/patching, conflict trailing newline, and UUID."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      rel_msg = "third_party/libc++/src/include/__locale_dir/messages.h"
      msg_path = os.path.join(tmp_dir, rel_msg)
      os.makedirs(os.path.dirname(msg_path), exist_ok=True)
      with open(msg_path, "w", encoding="utf-8") as f:
        f.write("#if defined(__unix__)\n"
                "<<<<<<< HEAD\n"
                "#  if !defined(__BIONIC__)\n"
                "=======\n"
                "#  if !defined(__BIONIC__) && !defined(STARBOARD)\n"
                ">>>>>>> upstream\n"
                "#    define _LIBCPP_HAS_CATOPEN 1\n"
                "#  endif\n"
                "#endif\n")

      class _FakeEngine:

        def generate_expert_guidance(self, **_kwargs):
          return {"guidance": ""}

        def resolve_conflict(self, **_kwargs):
          return {
              "replacement": ("```cpp\n#  if !defined(__BIONIC__) && "
                              "!defined(STARBOARD)\n```"),
              "model_used": "flash",
          }

      ok = resolve_file_conflicts(
          file_path=msg_path,
          repo_path=tmp_dir,
          git_context="",
          engine=_FakeEngine(),
          mock_mode=False,
      )
      self.assertTrue(ok)
      with open(msg_path, "r", encoding="utf-8") as f:
        resolved = f.read()
      self.assertIn(
          "#  if !defined(__BIONIC__) && !defined(STARBOARD)\n"
          "#    define _LIBCPP_HAS_CATOPEN 1\n",
          resolved,
      )

      # Parse compiler error on a path containing '+'
      err_out = (f"../../{rel_msg}:25:104: error: token is not a valid binary "
                 "operator in a preprocessor subexpression\n")
      diags = parse_compiler_errors(err_out, tmp_dir)
      self.assertEqual(len(diags), 1)
      self.assertEqual(diags[0].file_path, msg_path)
      self.assertEqual(diags[0].line_number, 25)

      # Apply FILE:-directed SEARCH/REPLACE patch to a path containing '+'
      patch = (f"FILE: {rel_msg}\n"
               "<<<<<<< SEARCH\n"
               "#    define _LIBCPP_HAS_CATOPEN 1\n"
               "=======\n"
               "#    define _LIBCPP_HAS_CATOPEN 0\n"
               ">>>>>>> REPLACE\n")
      modified = _apply(patch, tmp_dir, default_file="build/BUILD.gn")
      self.assertEqual(modified, [msg_path])

      # Strip Siso UUIDs in extract_meaningful_error_summary
      s1 = extract_meaningful_error_summary(
          "FAILED: 8b3a1832-a70f-430a-980d-9c5ac4c8b11c \"./foo.o\" CXX foo.o")
      s2 = extract_meaningful_error_summary(
          "FAILED: 11112222-3333-4444-5555-666677778888 \"./foo.o\" CXX foo.o")
      self.assertEqual(s1, s2)
      self.assertEqual(s1, 'FAILED: "./foo.o" CXX foo.o')

  def test_execute_investigation_tools_preserves_guidance_and_synthesizes(self):
    """Tests tool loop preserves guidance/history and synthesizes on break."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      calls = []

      class _ToolLoopResolver(BaseResolver):
        """Stub resolver for testing execute_investigation_tools."""

        @property
        def name(self):
          return "ToolLoopTest"

        def run_command(self, iteration):
          del iteration
          return True, "", ""

        def extract_diagnostics(self, build_output, siso_output):
          del build_output, siso_output
          return []

        def resolve_diagnostic(
            self,
            diagnostic,
            use_expert=False,
            expert_guidance="",
            investigation_log="",
        ):
          del diagnostic, use_expert
          calls.append({
              "investigation": investigation_log,
              "guidance": expert_guidance,
          })
          if "TOOL BUDGET EXHAUSTED" in investigation_log:
            return (
                "FILE: h_5_vcc.h\n<<<<<<< SEARCH\nold\n=======\nnew\n"
                ">>>>>>> REPLACE\n",
                "gemini-3.8-flash",
                "h_5_vcc.h",
            )
          return ("TOOL_FIND_FILE: *supplementable*", "gemini-3.8-flash",
                  "h_5_vcc.h")

      resolver = _ToolLoopResolver(tmp_dir)
      patch, _ = resolver.execute_investigation_tools(
          initial_patch="TOOL_FIND_FILE: *supplementable*",
          diagnostic="diag",
          expert_guidance="Migrate Supplement<LocalDOMWindow>",
      )
      self.assertIn(">>>>>>> REPLACE", patch)
      self.assertNotIn("TOOL_FIND_FILE", patch)
      self.assertTrue(
          all(c["guidance"] == "Migrate Supplement<LocalDOMWindow>"
              for c in calls))
      self.assertIn("Tool Call: `TOOL_FIND_FILE: *supplementable*`",
                    calls[-1]["investigation"])
      self.assertIn("TOOL BUDGET EXHAUSTED", calls[-1]["investigation"])

  def test_reject_nested_file_in_search_replace(self):
    """Tests that SEARCH/REPLACE blocks with nested FILE: are rejected."""
    with tempfile.NamedTemporaryFile("w+", suffix=".gn", delete=False) as tmp:
      tmp.write("var_a = true\n")
      tmp_path = tmp.name

    try:
      rel = os.path.basename(tmp_path)
      parent = os.path.dirname(tmp_path)
      bad_patch = f"""FILE: //{rel}
<<<<<<< SEARCH
var_a = true
=======
FILE: //other/BUILD.gn
target("foo") {{}}
>>>>>>> REPLACE
"""
      modified = _apply(bad_patch, repo_path=parent)
      self.assertEqual(modified, [])
      with open(tmp_path, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertIn("var_a = true", content)
    finally:
      if os.path.exists(tmp_path):
        os.remove(tmp_path)

  def test_autoninja_sends_full_context_for_build_files(self):
    """Tests that AutoninjaResolver sends full context for build files."""
    with tempfile.TemporaryDirectory() as tmp_dir:
      ft_dir = os.path.join(tmp_dir, "third_party", "freetype")
      os.makedirs(ft_dir, exist_ok=True)
      gn_file = os.path.join(ft_dir, "BUILD.gn")
      lines = [f"# Line {i}\n" for i in range(1, 150)]
      lines.append(
          "component(\"freetype\") { visibility = [ \"//public\" ] }\n")
      with open(gn_file, "w", encoding="utf-8") as f:
        f.writelines(lines)

      called_contexts = []

      class MockEngine:
        flash_model = "gemini-2.5-flash"

        def heal_compiler_error(self, **kwargs):
          called_contexts.append(kwargs.get("file_context", ""))
          return {"status": "SUCCESS", "patch": "PATCH", "model_used": "gemini"}

      resolver = AutoninjaResolver(
          repo_path=tmp_dir,
          out_dir="out",
          target="cobalt",
          engine=MockEngine(),
      )
      diag = CompilerDiagnostic(
          file_path=gn_file,
          line_number=1,
          column=1,
          error_message="incompatible with arm",
          raw_snippet="ld.lld: error",
          notes=[],
      )
      resolver.resolve_diagnostic(diag)
      self.assertEqual(len(called_contexts), 1)
      self.assertIn("component(\"freetype\")", called_contexts[0])
      self.assertIn("150: component(\"freetype\")", called_contexts[0])

  def test_reasoning_engine_client_dispatch(self):
    """Tests that ReasoningEngineClient routes calls to remote mock engine."""
    client = ReasoningEngineClient(
        **_TEST_MODELS,
        resource_id="projects/p/locations/l/reasoningEngines/123",
        project_id="test-p",
        location="us-central1",
    )
    mock_remote = mock.MagicMock()
    mock_remote.query.return_value = {
        "status": "SUCCESS",
        "patch": "TEST_PATCH",
        "model_used": "gemini-2.5-flash",
    }
    client._remote_engine = mock_remote  # pylint: disable=protected-access

    res = client.heal_compiler_error(
        target="cobalt",
        diagnostics="error: foo",
    )
    self.assertEqual(res["status"], "SUCCESS")
    self.assertEqual(res["patch"], "TEST_PATCH")
    mock_remote.query.assert_called_once_with(
        action="heal_compiler_error",
        target="cobalt",
        diagnostics="error: foo",
        source_contexts="",
        history="",
        investigation_history="",
        expert_guidance="",
        use_expert=False,
        **_TEST_MODELS,
    )

  def test_reasoning_engine_client_local_in_process(self):
    """Tests ReasoningEngineClient with local=True uses in-process engine."""
    client = ReasoningEngineClient(
        **_TEST_MODELS,
        project_id="test-p",
        location="us-central1",
        local=True,
    )
    self.assertTrue(client.local)
    mock_local = mock.MagicMock()
    mock_local.query.return_value = {
        "status": "SUCCESS",
        "patch": "LOCAL_PATCH",
        "model_used": "gemini-3.7-flash",
    }
    client._local_engine = mock_local  # pylint: disable=protected-access

    res = client.heal_compiler_error(
        target="cobalt",
        diagnostics="error: local_foo",
    )
    self.assertEqual(res["status"], "SUCCESS")
    self.assertEqual(res["patch"], "LOCAL_PATCH")
    mock_local.query.assert_called_once_with(
        action="heal_compiler_error",
        target="cobalt",
        diagnostics="error: local_foo",
        source_contexts="",
        history="",
        investigation_history="",
        expert_guidance="",
        use_expert=False,
        **_TEST_MODELS,
    )

  def test_token_usage_flows_from_engine_to_client(self):
    """Real model token counts reach client.token_usage, per model."""
    engine = CobaltReasoningEngine(project_id="test-proj", **_TEST_MODELS)
    fake_resp = mock.MagicMock(text="<<<<<<< SEARCH\na\n=======\nb\n"
                               ">>>>>>> REPLACE")
    fake_resp.usage_metadata = mock.MagicMock(
        prompt_token_count=1000,
        candidates_token_count=200,
        thoughts_token_count=50)
    genai_client = mock.MagicMock()
    genai_client.models.generate_content.return_value = fake_resp
    client = ReasoningEngineClient(**_TEST_MODELS, project_id="p", local=True)
    client._local_engine = engine  # pylint: disable=protected-access
    with mock.patch.object(engine, "_get_client", return_value=genai_client):
      res = client.heal_compiler_error(target="cobalt", diagnostics="error")
      client.heal_compiler_error(target="cobalt", diagnostics="error")

    self.assertNotIn("usage", res)
    model = _TEST_MODELS["flash_model"]
    self.assertEqual(client.token_usage.calls, 2)
    self.assertEqual(client.token_usage.by_model[model].prompt_tokens, 2000)
    self.assertEqual(client.token_usage.by_model[model].completion_tokens, 500)
    self.assertEqual(client.token_usage.total_tokens, 2500)

  def test_client_models_override_engine_models(self):
    """--model/--expert-model reach an engine deployed with other models."""
    engine = CobaltReasoningEngine(
        project_id="test-proj",
        flash_model="deployed-flash",
        expert_model="deployed-expert")
    engine._generate_content_with_retry = mock.MagicMock(  # pylint: disable=protected-access
        return_value=mock.MagicMock(text="patch"))
    client = ReasoningEngineClient(**_TEST_MODELS, project_id="p", local=True)
    client._local_engine = engine  # pylint: disable=protected-access

    res = client.heal_compiler_error(target="cobalt", diagnostics="error")
    self.assertEqual(res["model_used"], _TEST_MODELS["flash_model"])
    res = client.heal_compiler_error(
        target="cobalt", diagnostics="error", use_expert=True)
    self.assertEqual(res["model_used"], _TEST_MODELS["expert_model"])

  def test_engine_kwargs_safety_and_tolerance(self):
    """Guards kwargs safety when client sends unexpected arguments."""
    # pylint: disable=protected-access
    engine = CobaltReasoningEngine(project_id="test-proj", **_TEST_MODELS)
    engine._generate_content_with_retry = mock.MagicMock(
        return_value=mock.MagicMock(text="SEARCH / REPLACE"))
    engine._generate_expert_content = mock.MagicMock(
        return_value="Strategic guidance")

    # 1. generate_expert_guidance with unknown client kwargs and expert_model
    res1 = engine.query(
        action="generate_expert_guidance",
        target="cobalt/media/sandbox.cc",
        diagnostics="error: no member AddSample",
        expert_model="claude-sonnet-5",
        unexpected_future_flag="extra_value",
        arbitrary_metadata={"client_ver": "2.0"},
    )
    self.assertEqual(res1["status"], "SUCCESS")
    self.assertEqual(res1["model_used"], "claude-sonnet-5")

    # 2. resolve_conflict with extra kwargs
    res2 = engine.query(
        action="resolve_conflict",
        file_path="DEPS",
        language="Python",
        raw_conflict="conflict",
        expert_model="gemini-3.7-flash",
        use_expert=True,
        unknown_kwarg_1=True,
        unknown_kwarg_2=42,
    )
    self.assertEqual(res2["status"], "SUCCESS")

    # 3. heal_gn_error with extra kwargs
    res3 = engine.query(
        action="heal_gn_error",
        error_trace="gn error: undefined identifier",
        expert_model="gemini-3.8-flash",
        use_expert=True,
        extra_server_option="test",
    )
    self.assertEqual(res3["status"], "SUCCESS")

    # 4. heal_compiler_error with extra kwargs
    res4 = engine.query(
        action="heal_compiler_error",
        target="cobalt",
        diagnostics="error: undefined symbol",
        expert_model="claude-sonnet-5",
        use_expert=True,
        client_timestamp=12345678,
    )
    self.assertEqual(res4["status"], "SUCCESS")

  def test_anthropic_thinking_block_parsing(self):
    """Guards against AttributeError when Anthropic returns ThinkingBlock."""
    # pylint: disable=protected-access
    engine = CobaltReasoningEngine(project_id="test-proj", **_TEST_MODELS)

    class FakeThinkingBlock:
      type = "thinking"
      thinking = "Step 1: analyze root cause..."

    class FakeTextBlock:
      type = "text"
      text = ("## Concrete Refactoring Directive\n"
              "Replace AddSample with WriteSample.")

    fake_response = mock.MagicMock()
    fake_response.content = [FakeThinkingBlock(), FakeTextBlock()]

    mock_anthropic = mock.MagicMock()
    mock_anthropic.messages.create.return_value = fake_response
    engine._get_anthropic_client = mock.MagicMock(return_value=mock_anthropic)

    extracted_text = engine._generate_expert_content(
        contents="Diagnose error",
        system_instruction="You are Senior Architect",
        expert_model="claude-sonnet-5",
    )
    self.assertIn("## Concrete Refactoring Directive", extracted_text)
    self.assertIn("Replace AddSample with WriteSample.", extracted_text)

  def test_resolve_conflict_keeps_first_line_indentation(self):
    """Only surrounding newlines are stripped from a conflict resolution."""
    # pylint: disable=protected-access
    engine = CobaltReasoningEngine(project_id="test-proj", **_TEST_MODELS)
    engine._generate_content_with_retry = mock.MagicMock(
        return_value=mock.MagicMock(text="\n  if (is_android) {\n  }\n"))
    for use_expert in (False, True):
      res = engine.query(
          action="resolve_conflict",
          file_path="media/base/BUILD.gn",
          language="GN",
          raw_conflict="conflict",
          use_expert=use_expert)
      self.assertEqual(res["replacement"], "  if (is_android) {\n  }")

  def test_has_cobalt_git_history(self):
    """Verifies that has_cobalt_git_history accurately identifies Cobalt PRs."""
    _COBALT_GIT_HISTORY_CACHE.clear()

    # 1. Pure upstream Chromium commits should return False
    with mock.patch("subprocess.run") as mock_run:
      mock_run.return_value = mock.Mock(
          returncode=0,
          stdout=(
              "jlulejian@chromium.org\t[M138] Revert \"[Extensions] Log crash"
              " keys\"\n"
              "upstream@chromium.org\tRoll third_party/foo\n"),
      )
      self.assertFalse(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

    _COBALT_GIT_HISTORY_CACHE.clear()

    # 2. Automated rolling PRs should be skipped and return False
    with mock.patch("subprocess.run") as mock_run:
      mock_run.return_value = mock.Mock(
          returncode=0,
          stdout=(
              "cobalt-github-releaser-bot@google.com\tCONFLICTED Cherry pick"
              " commit f9d38917: Update to 139.7258. (#12003)\n"
              "95661244+cobalt-github-releaser-bot@users.noreply.github.com\t"
              "Autoroll from main to staging (#11194)\n"
              "jlulejian@chromium.org\tUpstream change\n"),
      )
      self.assertFalse(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

    _COBALT_GIT_HISTORY_CACHE.clear()

    # 3. Genuine Cobalt PR from Googler should return True
    with mock.patch("subprocess.run") as mock_run:
      mock_run.return_value = mock.Mock(
          returncode=0,
          stdout=(
              "cobalt-github-releaser-bot@google.com\tCONFLICTED Cherry pick"
              " commit f9d38917: Update to 139.7258. (#12003)\n"
              "andrewsavage@google.com\tAdd copied_base to crashpad build and"
              " compile with c++17 (#7599)\n"
              "jlulejian@chromium.org\tUpstream change\n"),
      )
      self.assertTrue(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

    _COBALT_GIT_HISTORY_CACHE.clear()

    # 4. Genuine Cobalt PR from Igalia contractor should return True
    with mock.patch("subprocess.run") as mock_run:
      mock_run.return_value = mock.Mock(
          returncode=0,
          stdout=("kubo@igalia.com\tFix staging build after #12126, #12123"
                  " (#12141)\n"),
      )
      self.assertTrue(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

    _COBALT_GIT_HISTORY_CACHE.clear()

    # 5. Genuine Cobalt PR from Collabora contractor should return True
    with mock.patch("subprocess.run") as mock_run:
      mock_run.return_value = mock.Mock(
          returncode=0,
          stdout=("denis.shimizu@collabora.com\tCherry pick PR #10222:"
                  " starboard/tests: wire opus_tests\n"),
      )
      self.assertTrue(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

    # 6. Cache verification: repeated call should not invoke subprocess.run
    with mock.patch("subprocess.run") as mock_run:
      self.assertTrue(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))
      mock_run.assert_not_called()

    _COBALT_GIT_HISTORY_CACHE.clear()

    # 7. Subprocess error should gracefully return False
    with mock.patch("subprocess.run") as mock_run:
      mock_run.side_effect = OSError("git not found")
      self.assertFalse(
          has_cobalt_git_history("third_party/foo/bar.h", "/mock/repo"))

  def test_is_unmodified_third_party(self):
    """Verifies third-party file classification guards against clobbering."""
    with tempfile.TemporaryDirectory() as tmpdir:
      # 1. Non-third-party files are never treated as unmodified third-party
      base_file = os.path.join(tmpdir, "base", "logging.cc")
      os.makedirs(os.path.dirname(base_file), exist_ok=True)
      with open(base_file, "w", encoding="utf-8") as f:
        f.write("// normal file")
      self.assertFalse(is_unmodified_third_party(base_file, tmpdir))

      # 2. Tooling and first-party Blink under third_party are patchable
      jni_file = os.path.join(tmpdir, "third_party", "jni_zero", "tool.py")
      os.makedirs(os.path.dirname(jni_file), exist_ok=True)
      with open(jni_file, "w", encoding="utf-8") as f:
        f.write("# tooling")
      self.assertFalse(is_unmodified_third_party(jni_file, tmpdir))

      blink_file = os.path.join(
          tmpdir,
          "third_party",
          "blink",
          "renderer",
          "platform",
          "loader",
          "fetch",
          "policy_container_utils.cc",
      )
      os.makedirs(os.path.dirname(blink_file), exist_ok=True)
      with open(blink_file, "w", encoding="utf-8") as f:
        f.write("// new upstream Blink file\n")
      self.assertFalse(is_unmodified_third_party(blink_file, tmpdir))

      # 3. Pure third-party file with no Cobalt git history or macros
      pure_tp = os.path.join(tmpdir, "third_party", "zlib", "zlib.h")
      os.makedirs(os.path.dirname(pure_tp), exist_ok=True)
      with open(pure_tp, "w", encoding="utf-8") as f:
        f.write("// upstream zlib header\n")
      with mock.patch("repo_guards.has_cobalt_git_history", return_value=False):
        self.assertTrue(is_unmodified_third_party(pure_tp, tmpdir))

      # 4. Third-party file with Cobalt git history is recognized as modified
      with mock.patch("repo_guards.has_cobalt_git_history", return_value=True):
        self.assertFalse(is_unmodified_third_party(pure_tp, tmpdir))

      # 5. Third-party file containing Cobalt macro is recognized as modified
      cobalt_tp = os.path.join(tmpdir, "third_party", "absl", "traits.h")
      os.makedirs(os.path.dirname(cobalt_tp), exist_ok=True)
      with open(cobalt_tp, "w", encoding="utf-8") as f:
        f.write("#if defined(ENABLE_BUILDFLAG_BUILD_BASE_WITH_CPP17)\n")
      with mock.patch("repo_guards.has_cobalt_git_history", return_value=False):
        self.assertFalse(is_unmodified_third_party(cobalt_tp, tmpdir))

  def test_forked_third_party_metadata_is_not_patchable(self):
    """Verifies the jni_zero exception unlocks code but not dir metadata."""
    with tempfile.TemporaryDirectory() as tmpdir:
      jni_dir = os.path.join(tmpdir, "third_party", "jni_zero")
      os.makedirs(jni_dir, exist_ok=True)

      # Source and build files in the forked dir stay patchable.
      for name in ("jni_generator.py", "BUILD.gn"):
        path = os.path.join(jni_dir, name)
        with open(path, "w", encoding="utf-8") as f:
          f.write("# upstream content\n")
        self.assertFalse(
            is_unmodified_third_party(path, tmpdir),
            f"{name} should remain patchable")

      # Repository metadata is blocked even inside the forked dir.
      for name in ("DEPS", "LICENSE", "OWNERS", "README.chromium"):
        path = os.path.join(jni_dir, name)
        with open(path, "w", encoding="utf-8") as f:
          f.write("upstream metadata\n")
        self.assertTrue(
            is_unmodified_third_party(path, tmpdir),
            f"{name} should be blocked")

      # The top-level DEPS file is outside third_party/ and unaffected, so
      # Phase 0/1 DEPS conflict resolution still works.
      top_deps = os.path.join(tmpdir, "DEPS")
      with open(top_deps, "w", encoding="utf-8") as f:
        f.write("deps = {}\n")
      self.assertFalse(is_unmodified_third_party(top_deps, tmpdir))

  def test_base_resolver_circuit_breaker_abort(self):
    """Verifies BaseResolver aborts loops when error repeats 8 times."""
    with tempfile.TemporaryDirectory() as tmpdir:
      iteration_count = 0

      class DummyBreakerResolver(BaseResolver):
        """Mock resolver for testing circuit breaker abort."""

        @property
        def name(self) -> str:
          return "DummyBreakerResolver"

        def run_command(self, iteration: int):
          del iteration  # Unused.
          nonlocal iteration_count
          iteration_count += 1
          return False, "persistent_syntax_error.cc:10: error: bad token", ""

        def extract_diagnostics(self, output: str, siso_out: str):
          del siso_out  # Unused.
          return [output]

        def resolve_diagnostic(
            self,
            diagnostic,
            use_expert=False,
            expert_guidance="",
        ):
          del diagnostic  # Unused.
          del use_expert, expert_guidance  # Unused.
          return "", "flash", "persistent_syntax_error.cc"

      resolver = DummyBreakerResolver(repo_path=tmpdir, max_iterations=20)
      res = resolver.run_resolution_loop()
      self.assertFalse(res)
      # Iteration 1: stuck_count = 0; Iteration 2: stuck_count = 1; ...
      # Iteration 9: stuck_count = 8 -> circuit breaker triggers and halts loop
      self.assertEqual(iteration_count, 9)

  def test_distinct_errors_on_same_file_do_not_trigger_anti_loop(self):
    """Distinct errors on one file must not trigger Anti-Loop or breaker."""
    with tempfile.TemporaryDirectory() as tmpdir:
      build_gn = os.path.join(tmpdir, "BUILD.gn")
      with open(build_gn, "w", encoding="utf-8") as f:
        f.write("# baseline\n")
      expert_flags = []

      class MultiErrorResolver(BaseResolver):
        """Simulates 5 distinct linker errors targeting the same BUILD.gn."""

        @property
        def name(self) -> str:
          return "MultiErrorResolver"

        def run_command(self, iteration: int):
          if iteration <= 5:
            msg = f"BUILD.gn:1: error: undefined symbol sym_{iteration}"
            return False, msg, ""
          return True, "build passed", ""

        def extract_diagnostics(self, output: str, siso_out: str):
          del siso_out
          return [
              CompilerDiagnostic(
                  file_path=build_gn,
                  line_number=1,
                  column=1,
                  error_message=output,
                  raw_snippet=output,
                  notes=[],
              )
          ]

        def resolve_diagnostic(
            self,
            diagnostic,
            use_expert=False,
            expert_guidance="",
        ):
          del diagnostic, expert_guidance
          expert_flags.append(use_expert)
          return "", "flash", build_gn

      resolver = MultiErrorResolver(repo_path=tmpdir, max_iterations=10)
      with mock.patch("subprocess.run") as mock_run:
        res = resolver.run_resolution_loop()
      self.assertTrue(res)
      # Anti-Loop git checkout must never be invoked when errors are distinct.
      for call in mock_run.call_args_list:
        args = call[0][0] if call[0] else []
        self.assertNotIn("checkout", args)
      # use_expert escalates once file_error_counts reaches 3 (iterations 3..5).
      self.assertEqual(expert_flags, [False, False, True, True, True])

  def test_parse_compiler_errors_full_include_stack(self):
    """Verifies parse_compiler_errors captures up to 30 lines of stack."""
    with tempfile.TemporaryDirectory() as tmpdir:
      leaf_file = os.path.join(tmpdir, "mojo", "public", "cpp", "bindings",
                               "pending_receiver.h")
      os.makedirs(os.path.dirname(leaf_file), exist_ok=True)
      with open(leaf_file, "w", encoding="utf-8") as f:
        f.write("// header\n")

      build_output = (
          "In file included from ../../third_party/blink/renderer/modules/"
          "cobalt_modules_stubs.cc:451:\n"
          "In file included from ../../third_party/blink/renderer/modules/xr/"
          "xr_webgl_layer.h:11:\n"
          "In file included from ../../third_party/blink/renderer/modules/xr/"
          "xr_webgl_rendering_context.h:11:\n"
          "In file included from ../../third_party/blink/renderer/platform/"
          "mojo/cross_variant_mojo_util.h:15:\n"
          "../../mojo/public/cpp/bindings/pending_receiver.h:25:3: error: "
          "no template named 'PendingReceiverConverter'; did you mean "
          "'::mojo::PendingReceiverConverter'?\n"
          "  PendingReceiverConverter<T>::Convert();\n"
          "  ^~~~~~~~~~~~~~~~~~~~~~~~\n")

      diags = parse_compiler_errors(build_output, tmpdir)
      self.assertEqual(len(diags), 1)
      diag = diags[0]
      self.assertEqual(diag.file_path, leaf_file)
      self.assertEqual(diag.line_number, 25)
      self.assertEqual(diag.column, 3)
      self.assertIn("no template named 'PendingReceiverConverter'",
                    diag.error_message)
      # Full 30-line window ensures the entire include stack is in raw_snippet
      self.assertIn("cobalt_modules_stubs.cc:451", diag.raw_snippet)
      self.assertIn("xr_webgl_layer.h:11", diag.raw_snippet)
      self.assertIn("cross_variant_mojo_util.h:15", diag.raw_snippet)

  def test_autoninja_resolve_diagnostic_passes_full_snippet_and_guard(self):
    """Verifies resolve_diagnostic passes raw_snippet and guard directly."""
    with tempfile.TemporaryDirectory() as tmpdir:
      source_file = os.path.join(tmpdir, "third_party", "blink", "util.h")
      os.makedirs(os.path.dirname(source_file), exist_ok=True)
      with open(source_file, "w", encoding="utf-8") as f:
        f.write("// third party header\n")

      resolver = AutoninjaResolver(
          repo_path=tmpdir, out_dir="out/test", target="cobalt_apk")
      snippet_with_stack = (
          "In file included from ../../cobalt/stubs.cc:451:\n"
          "../../third_party/blink/util.h:25:3: error: invalid type\n")
      diag = CompilerDiagnostic(
          file_path=source_file,
          line_number=25,
          column=3,
          error_message="invalid type",
          raw_snippet=snippet_with_stack,
          notes=[],
      )

      called_error_trace = []

      class MockEngine:
        flash_model = "gemini-2.5-flash"

        def heal_compiler_error(self, **kwargs):
          called_error_trace.append(kwargs.get("error_trace", ""))
          return {"status": "SUCCESS", "patch": "", "model_used": "flash"}

      resolver.reasoning_engine = MockEngine()
      with unittest.mock.patch(
          "autoninja.is_unmodified_third_party", return_value=True):
        resolver.resolve_diagnostic(diag)

      self.assertEqual(len(called_error_trace), 1)
      # Verifies snippet with stack was passed unadulterated to the LLM
      self.assertIn("cobalt/stubs.cc:451", called_error_trace[0])
      self.assertIn("invalid type", called_error_trace[0])
      # Verifies standard read-only guard is included
      self.assertIn("CRITICAL GUARD", called_error_trace[0])
      self.assertIn("strictly READ-ONLY", called_error_trace[0])

  def test_parse_compiler_errors_linker_build_file_mapping(self):
    """Verifies that linker errors map archives to their BUILD.gn."""
    with tempfile.TemporaryDirectory() as tmpdir:
      build_gn = os.path.join(tmpdir, "third_party", "blink", "renderer",
                              "bindings", "modules", "v8", "BUILD.gn")
      os.makedirs(os.path.dirname(build_gn), exist_ok=True)
      with open(build_gn, "w", encoding="utf-8") as f:
        f.write("# BUILD.gn\n")

      build_output = (
          "ld.lld: error: undefined symbol: "
          "blink::V8GPUTextureViewDimension::string_table_\n"
          ">>> referenced by xr_gpu_sub_image.h:0 "
          "(../../third_party/blink/renderer/modules/xr/xr_gpu_sub_image.h:0)\n"
          ">>>               v8/v8_xr_gpu_sub_image.o:"
          "(blink::XRGPUSubImage::getViewDescriptor() const) in archive "
          "obj/third_party/blink/renderer/bindings/modules/v8/libv8.a\n"
          "ld.lld: error: undefined symbol: "
          "blink::GPUTextureViewDescriptor::GPUTextureViewDescriptor()\n")

      diags = parse_compiler_errors(build_output, tmpdir)
      self.assertEqual(len(diags), 1)
      diag = diags[0]
      self.assertEqual(diag.file_path, build_gn)
      self.assertIn(
          "undefined symbol: blink::V8GPUTextureViewDimension::string_table_",
          diag.error_message)
      self.assertIn("v8_xr_gpu_sub_image.o", diag.raw_snippet)

  def test_apply_code_fence_and_bold_file(self):
    """Verifies parser handles code fences between FILE and SEARCH."""
    with tempfile.TemporaryDirectory() as tmpdir:
      test_file = os.path.join(tmpdir, "cobalt", "config.gni")
      os.makedirs(os.path.dirname(test_file), exist_ok=True)
      with open(test_file, "w", encoding="utf-8") as f:
        f.write('target_os = "linux"\nfoo_flag = false\n')

      patch_text = ("Let me analyze the build failure.\n"
                    "### **FILE**: `cobalt/config.gni`\n"
                    "```gn\n"
                    "<<<<<<< SEARCH\n"
                    "foo_flag = false\n"
                    "=======\n"
                    "foo_flag = true\n"
                    ">>>>>>> REPLACE\n"
                    "```\n")

      modified = _apply(patch_text, tmpdir)
      self.assertEqual(len(modified), 1)
      with open(test_file, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertIn("foo_flag = true", content)

  def test_apply_with_default_file(self):
    """Verifies patch application falls back to default_file."""
    with tempfile.TemporaryDirectory() as tmpdir:
      test_file = os.path.join(tmpdir, "cobalt", "settings.cc")
      os.makedirs(os.path.dirname(test_file), exist_ok=True)
      with open(test_file, "w", encoding="utf-8") as f:
        f.write("void Init() {\n  int val = 1;\n}\n")

      patch_text = ("Fixing settings:\n"
                    "<<<<<<< SEARCH\n"
                    "  int val = 1;\n"
                    "=======\n"
                    "  int val = 2;\n"
                    ">>>>>>> REPLACE\n")

      modified = _apply(patch_text, tmpdir, default_file="cobalt/settings.cc")
      self.assertEqual(len(modified), 1)
      with open(test_file, "r", encoding="utf-8") as f:
        content = f.read()
      self.assertIn("int val = 2;", content)

  def test_linker_diagnostic_file_context_inclusion(self):
    """Verifies file_context is sent for text files on linker diagnostics."""
    with tempfile.TemporaryDirectory() as tmpdir:
      gni_file = os.path.join(tmpdir, "cobalt", "build.gni")
      os.makedirs(os.path.dirname(gni_file), exist_ok=True)
      with open(gni_file, "w", encoding="utf-8") as f:
        f.write('cobalt_exclude = [\n  "pattern1",\n]\n')

      resolver = AutoninjaResolver(
          repo_path=tmpdir, out_dir="out/test", target="cobalt_apk")
      diag = CompilerDiagnostic(
          file_path=gni_file,
          line_number=2,
          column=1,
          error_message="Linker error: undefined symbol: foo",
          raw_snippet="ld.lld: error: undefined symbol: foo",
          notes=[],
      )

      captured_context = []

      class MockEngine:
        flash_model = "gemini-2.5-flash"

        def heal_compiler_error(self, **kwargs):
          captured_context.append(kwargs.get("file_context", ""))
          return {"status": "SUCCESS", "patch": "", "model_used": "flash"}

      resolver.reasoning_engine = MockEngine()
      resolver.resolve_diagnostic(diag)

      self.assertEqual(len(captured_context), 1)
      self.assertIn("cobalt_exclude = [", captured_context[0])

  def test_tier2_preflight_diagnostic_attribute_safety(self):
    """Verifies that diagnostics work without missing attribute errors."""
    sync_diag = GClientSyncDiagnostic(
        error_message="Hook 'python3 configure_siso.py' failed",
        raw_output="raw error",
        diagnostic_trace="diagnostic trace text",
    )
    self.assertTrue(hasattr(sync_diag, "file_path"))
    self.assertTrue(hasattr(sync_diag, "line_number"))

    gn_diag = GNDiagnostic(
        error_message="GN Error",
        raw_output="raw gn output",
        target_files={"BUILD.gn": 10},
        is_structural_break=False,
        file_path="BUILD.gn",
        line_number=10,
    )
    self.assertTrue(hasattr(gn_diag, "file_path"))
    self.assertEqual(gn_diag.file_path, "BUILD.gn")
    self.assertEqual(gn_diag.line_number, 10)

  def test_agent_change_record_data_structure(self):
    """Verifies AgentChangeRecord initialization, dict, and prompt string."""
    record = AgentChangeRecord(
        phase="autoninja",
        iteration=3,
        target_file="cobalt/browser/features.cc",
        file_changes={
            "cobalt/browser/features.cc":
                ("<<<<<<< SEARCH\nfoo();\n=======\nbar();\n>>>>>>> REPLACE")
        },
        error="features.cc:42: error: undefined symbol bar",
        command_output=("FAILED: obj/cobalt/browser/features.o\n"
                        "features.cc:42: error: undefined symbol bar"),
        applied_cleanly=True,
    )
    d = record.to_dict()
    self.assertEqual(d["phase"], "autoninja")
    self.assertEqual(d["iteration"], 3)
    self.assertEqual(d["target_file"], "cobalt/browser/features.cc")
    self.assertEqual(d["error"], "features.cc:42: error: undefined symbol bar")
    self.assertTrue(d["applied_cleanly"])
    self.assertEqual(d["modified_files"], ["cobalt/browser/features.cc"])

    p_str = record.to_prompt_str()
    self.assertIn(
        "#### [autoninja] Iteration 3 -> Target: `cobalt/browser/features.cc`",
        p_str)
    self.assertIn("Patch Applied Cleanly: True", p_str)
    self.assertIn("FAILED: features.cc:42: error: undefined symbol bar", p_str)
    self.assertIn("bar();", p_str)
    self.assertIn("features.cc:42: error: undefined symbol bar", p_str)

  def test_agent_change_record_multi_file_changes(self):
    """Verifies AgentChangeRecord with multi-file dictionary changes."""
    record = AgentChangeRecord(
        phase="autoninja",
        iteration=2,
        target_file="foo.py",
        file_changes={
            "goo.cc": ("<<<<<<< SEARCH\nvoid Old();\n=======\n"
                       "void New();\n>>>>>>> REPLACE"),
            "hoo.java": ("<<<<<<< SEARCH\nint a = 1;\n=======\n"
                         "int a = 2;\n>>>>>>> REPLACE"),
        },
        error="hoo.java:10: error: incompatible types",
        applied_cleanly=True,
    )
    self.assertEqual(record.modified_files, ["goo.cc", "hoo.java"])
    self.assertEqual(
        record.file_changes["goo.cc"],
        "<<<<<<< SEARCH\nvoid Old();\n=======\nvoid New();\n>>>>>>> REPLACE",
    )
    self.assertEqual(
        record.file_changes["hoo.java"],
        "<<<<<<< SEARCH\nint a = 1;\n=======\nint a = 2;\n>>>>>>> REPLACE",
    )
    d = record.to_dict()
    self.assertEqual(d["modified_files"], ["goo.cc", "hoo.java"])
    self.assertIn("goo.cc", d["file_changes"])
    self.assertIn("hoo.java", d["file_changes"])
    self.assertIn("FILE: goo.cc", record.changes)
    self.assertIn("FILE: hoo.java", record.changes)
    p_str = record.to_prompt_str()
    self.assertIn("Modified Files (2): `goo.cc`, `hoo.java`", p_str)

  def test_expert_agent_omniscience_and_model_defaults(self):
    """Verifies models are caller-supplied and omniscience guidance."""
    engine = CobaltReasoningEngine(
        project_id="test-proj",
        flash_model="gemini-3.7-flash",
        expert_model="gemini-3.8-flash")
    self.assertEqual(engine.flash_model, "gemini-3.7-flash")
    self.assertEqual(engine.expert_model, "gemini-3.8-flash")
    with self.assertRaises(TypeError):
      CobaltReasoningEngine(project_id="test-proj")  # pylint: disable=missing-kwoa

    client = ReasoningEngineClient(
        project_id="test-proj",
        local=True,
        flash_model="gemini-3.7-flash",
        expert_model="gemini-3.8-flash")
    self.assertEqual(client.flash_model, "gemini-3.7-flash")
    self.assertEqual(client.expert_model, "gemini-3.8-flash")
    # Models must come from the caller; there are no built-in defaults.
    with self.assertRaises(TypeError):
      ReasoningEngineClient(project_id="test-proj", local=True)  # pylint: disable=missing-kwoa
    with self.assertRaises(ValueError):
      ReasoningEngineClient(
          project_id="test-proj", local=True, flash_model="", expert_model="e")

    captured_prompts = []

    def mock_expert_content(contents, system_inst, expert_model=None, **kwargs):
      del kwargs
      captured_prompts.append((str(contents), system_inst, expert_model))
      return "Directives:\n1. Update header include\n2. Fix type signature"

    # pylint: disable=protected-access
    engine._generate_expert_content = mock_expert_content

    res = engine.generate_expert_guidance(
        target="cobalt",
        diagnostics="fatal error: 'v8.h' file not found",
        source_contexts="void Init() { ... }",
        trajectory_history="#### Iteration 1: Attempted #include <v8.h>",
        working_diff="diff --git a/file.cc b/file.cc\n+#include <v8.h>",
        raw_log="[1/100] CXX obj/test.o\nfatal error: 'v8.h' file not found",
        all_diagnostics="1. fatal error: 'v8.h' file not found",
        mode="compiler",
    )

    self.assertEqual(res["status"], "SUCCESS")
    self.assertIn("Directives:", res["guidance"])
    self.assertEqual(res["model_used"], "gemini-3.8-flash")
    self.assertEqual(len(captured_prompts), 1)

    prompt_text, sys_inst, model_called = captured_prompts[0]
    self.assertEqual(model_called, "gemini-3.8-flash")
    self.assertIn("OMNISCIENT visibility", sys_inst)
    self.assertIn("--- Full Raw Build Output Log (Tail) ---", prompt_text)
    self.assertIn("--- All Extracted Diagnostics in Current Build ---",
                  prompt_text)
    self.assertIn("--- Git Diff of Modifications in Current Session ---",
                  prompt_text)
    self.assertIn("--- Prior Iteration Attempt Trajectory ---", prompt_text)

  def test_base_resolver_session_changes_tracking(self):
    """Verifies BaseResolver records AgentChangeRecord into session_changes."""
    with tempfile.TemporaryDirectory() as tmpdir:
      test_file = os.path.join(tmpdir, "cobalt", "sample.cc")
      os.makedirs(os.path.dirname(test_file), exist_ok=True)
      with open(test_file, "w", encoding="utf-8") as f:
        f.write("int value = 1;\n")

      session_changes = []

      class DummyTrackingResolver(BaseResolver):
        """Dummy tracking resolver for testing session changes."""

        @property
        def name(self):
          return "DummyTracking"

        def run_command(self, iteration):
          if iteration == 1:
            return False, "sample.cc:1: error: mismatch", ""
          return True, "Build succeeded", ""

        def extract_diagnostics(self, build_output, siso_output):
          del build_output, siso_output
          return ["sample.cc:1: error: mismatch"]

        def resolve_diagnostic(self, diagnostic, **kwargs):
          del diagnostic, kwargs
          patch = ("### **FILE**: `cobalt/sample.cc`\n"
                   "<<<<<<< SEARCH\n"
                   "int value = 1;\n"
                   "=======\n"
                   "int value = 2;\n"
                   ">>>>>>> REPLACE\n")
          return patch, "gemini-3.7-flash", "cobalt/sample.cc"

      resolver = DummyTrackingResolver(
          repo_path=tmpdir,
          max_iterations=5,
          session_changes=session_changes,
      )

      # Run resolution loop: Iteration 1 will apply patch and record it;
      # Iteration 2 will succeed.
      ok = resolver.run_resolution_loop()
      self.assertTrue(ok)
      self.assertGreaterEqual(len(session_changes), 1)
      first_change = session_changes[0]
      self.assertEqual(first_change.phase, "DummyTracking")
      self.assertEqual(first_change.target_file, "cobalt/sample.cc")
      self.assertTrue(first_change.applied_cleanly)
      self.assertIn("int value = 2;", first_change.changes)

  def test_conflict_resolver_records_session_changes(self):
    """Verifies resolve_file_conflicts appends records to session_changes."""
    with tempfile.TemporaryDirectory() as tmpdir:
      test_file = os.path.join(tmpdir, "sample_conflict.cc")
      with open(test_file, "w", encoding="utf-8") as f:
        f.write(SAMPLE_CPP_CONFLICT)

      session_changes = []
      ok = resolve_file_conflicts(
          file_path=test_file,
          repo_path=tmpdir,
          git_context="branch: test",
          session_changes=session_changes,
          mock_mode=True,
      )
      self.assertTrue(ok)
      self.assertEqual(len(session_changes), 1)
      rec = session_changes[0]
      self.assertEqual(rec.phase, "resolve_conflicts")
      self.assertEqual(rec.target_file, "sample_conflict.cc")
      self.assertTrue(rec.applied_cleanly)
      self.assertIn("InitChromiumDefaultMediaPipeline()", rec.changes)

  def test_expert_prompt_modified_files_sorted_by_iteration(self):
    """Verifies modified files in prompt are sorted by iteration number."""
    mock_engine = mock.MagicMock()
    captured_kwargs = {}

    def mock_guidance(**kwargs):
      captured_kwargs.update(kwargs)
      return {"status": "SUCCESS", "guidance": "Fix it."}

    mock_engine.generate_expert_guidance.side_effect = mock_guidance

    session_changes = [
        AgentChangeRecord(
            phase="resolve_conflicts",
            iteration=2,
            target_file="z_conflict.cc",
            file_changes={"z_conflict.cc": "// diff z"},
        ),
        AgentChangeRecord(
            phase="resolve_conflicts",
            iteration=1,
            target_file="a_conflict.cc",
            file_changes={"a_conflict.cc": "// diff a"},
        ),
        AgentChangeRecord(
            phase="autoninja",
            iteration=2,
            target_file="z_compiler.cc",
            file_changes={"z_compiler.cc": "// diff z compiler"},
        ),
        AgentChangeRecord(
            phase="autoninja",
            iteration=1,
            target_file="a_compiler.cc",
            file_changes={"a_compiler.cc": "// diff a compiler"},
        ),
    ]

    class TestResolver(BaseResolver):
      """Mock resolver for testing expert prompt injection."""

      @property
      def name(self):
        return "autoninja"

      def run_command(self, iteration):
        if iteration == 1:
          return False, "FAILED: test.o\ntest.cc:1: error: test", ""
        return True, "Build succeeded", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return ["test.cc:1: error: test"]

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return "patch", "flash", "test.cc"

    with tempfile.TemporaryDirectory() as tmpdir:
      test_file = os.path.join(tmpdir, "test.cc")
      with open(test_file, "w", encoding="utf-8") as f:
        f.write("int a = 1;\n")

      resolver = TestResolver(
          repo_path=tmpdir,
          max_iterations=2,
          engine=mock_engine,
          session_changes=session_changes,
      )

      ok = resolver.run_resolution_loop()
      self.assertTrue(ok)
      self.assertGreaterEqual(len(captured_kwargs),
                              1)  # Captured expert guidance calls
      traj = captured_kwargs.get("trajectory_history", "")
      self.assertIn("=== Files Modified in Current Session (4 files) ===", traj)

    # Verify order is by phase and iteration number, not alphabetically
    # by filename
    lines = [
        line.strip()
        for line in traj.splitlines()
        if line.strip().startswith("- ") and "Iteration" in line
    ]
    self.assertEqual(len(lines), 4)
    self.assertEqual(lines[0],
                     "- [resolve_conflicts] Iteration 1: `a_conflict.cc`")
    self.assertEqual(lines[1],
                     "- [resolve_conflicts] Iteration 2: `z_conflict.cc`")
    self.assertEqual(lines[2], "- Iteration 1: `a_compiler.cc`")
    self.assertEqual(lines[3], "- Iteration 2: `z_compiler.cc`")

  def test_tool_get_history_and_last_ten_default(self):
    """Verifies trajectory defaults to 10 and TOOL_GET_HISTORY handler."""
    session_changes = [
        AgentChangeRecord(
            phase="Phase4",
            iteration=i,
            target_file=f"file_{i}.cc",
            file_changes={f"file_{i}.cc": f"// change {i}"},
            error=f"error in {i}" if i % 2 == 0 else None,
        ) for i in range(1, 26)  # 25 records
    ]

    # 1. TOOL_GET_HISTORY: 5 (last 5 records)
    res_last_5 = execute_local_tool(
        "TOOL_GET_HISTORY: 5", ".", session_changes=session_changes)
    self.assertIn("=== Last 5 Change Records (out of 25) ===", res_last_5)
    self.assertIn("file_25.cc", res_last_5)
    self.assertIn("file_21.cc", res_last_5)
    self.assertNotIn("file_19.cc", res_last_5)

    # 2. TOOL_GET_HISTORY: iteration 3
    res_iter_3 = execute_local_tool(
        "TOOL_GET_HISTORY: iteration 3", ".", session_changes=session_changes)
    self.assertIn("=== Change Record for Iteration 3 ===", res_iter_3)
    self.assertIn("file_3.cc", res_iter_3)
    self.assertNotIn("file_4.cc", res_iter_3)

    # 3. TOOL_GET_HISTORY: 2-4 (iteration range)
    res_range = execute_local_tool(
        "TOOL_GET_HISTORY: 2-4", ".", session_changes=session_changes)
    self.assertIn("=== Change Records for Iterations 2-4 (3 records) ===",
                  res_range)
    self.assertIn("file_2.cc", res_range)
    self.assertIn("file_3.cc", res_range)
    self.assertIn("file_4.cc", res_range)
    self.assertNotIn("file_5.cc", res_range)

    # 4. TOOL_GET_HISTORY: all
    res_all = execute_local_tool(
        "TOOL_GET_HISTORY: all", ".", session_changes=session_changes)
    self.assertIn("=== Full Change History (25 records) ===", res_all)
    self.assertIn("file_1.cc", res_all)
    self.assertIn("file_25.cc", res_all)

    # 5. TOOL_GET_HISTORY: file_7.cc (filepath lookup)
    res_file = execute_local_tool(
        "TOOL_GET_HISTORY: file_7.cc", ".", session_changes=session_changes)
    self.assertIn("=== Change Records for 'file_7.cc' (1 records) ===",
                  res_file)
    self.assertIn("file_7.cc", res_file)
    self.assertNotIn("file_8.cc", res_file)

    # 6. TOOL_GET_HISTORY: non_existent.cc
    res_none = execute_local_tool(
        "TOOL_GET_HISTORY: non_existent.cc",
        ".",
        session_changes=session_changes)
    self.assertIn("No change records found matching file 'non_existent.cc'",
                  res_none)

  def test_parse_compiler_errors_fatal_and_action_failures(self):
    """Verifies parse_compiler_errors matches FATAL logs and action errors."""
    with tempfile.TemporaryDirectory() as tmpdir:
      # Set up mock blink directory and BUILD.gn
      blink_dir = os.path.join(tmpdir, "third_party", "blink", "renderer",
                               "platform")
      os.makedirs(blink_dir, exist_ok=True)
      gn_path = os.path.join(blink_dir, "BUILD.gn")
      with open(gn_path, "w", encoding="utf-8") as f:
        f.write("# Dummy BUILD.gn\n"
                "compiled_action(\"character_data\") {\n"
                "  tool = \":character_data_generator\"\n"
                "}\n")

      # Case 1: FATAL crash inside compiled action generator
      siso_output = (
          "FAILED: ./gen/third_party/blink/renderer/platform/"
          "character_property_data.cc ACTION "
          "//third_party/blink/renderer/platform:character_data\n"
          "[0910/211026.345187:FATAL:third_party/blink/renderer/platform/text/"
          "character_property_data_generator.cc:48] Check failed: "
          "U_SUCCESS(error). ulocdata_getCLDRVersion: "
          "(2)U_MISSING_RESOURCE_ERROR\n")
      diags = parse_compiler_errors(siso_output, tmpdir)
      self.assertEqual(len(diags), 1)
      d = diags[0]
      self.assertIn("character_property_data_generator.cc", d.file_path)
      self.assertEqual(d.line_number, 48)
      self.assertIn("ulocdata_getCLDRVersion", d.error_message)
      self.assertTrue(
          any("Failing Action: "
              "//third_party/blink/renderer/platform:character_data" in n
              for n in d.notes))

      # Case 2: Pure GN Action failure without fatal log (fallback to BUILD.gn)
      action_only_output = (
          "FAILED: obj/stamp ACTION "
          "//third_party/blink/renderer/platform:character_data\n"
          "command failed with exit code 1\n")
      diags_act = parse_compiler_errors(action_only_output, tmpdir)
      self.assertEqual(len(diags_act), 1)
      self.assertEqual(diags_act[0].file_path, gn_path)
      self.assertEqual(diags_act[0].line_number, 2)
      self.assertIn("character_data", diags_act[0].error_message)

      # Case 3: Python traceback in action script
      py_script = os.path.join(tmpdir, "generate.py")
      with open(py_script, "w", encoding="utf-8") as f:
        f.write("print('hello')\n")
      py_output = ("FAILED: out/gen.cc ACTION "
                   "//third_party/blink/renderer/platform:character_data\n"
                   "Traceback (most recent call last):\n"
                   f"  File \"{py_script}\", line 1, in <module>\n"
                   "    raise ValueError('broken config')\n"
                   "ValueError: broken config\n")
      diags_py = parse_compiler_errors(py_output, tmpdir)
      self.assertEqual(len(diags_py), 1)
      self.assertEqual(diags_py[0].file_path, py_script)
      self.assertEqual(diags_py[0].line_number, 1)
      self.assertIn("ValueError: broken config", diags_py[0].error_message)

  def test_extract_meaningful_error_summary(self):
    """Verifies generic headers are stripped from error_summary."""
    raw = ("Siso output:\n"
           "ninja: Entering directory out/android\n"
           "FAILED: out/obj.o\n"
           "foo.cc:10: error: undefined bar\n")
    summary = extract_meaningful_error_summary(raw)
    self.assertEqual(summary, "FAILED: out/obj.o")

    direct_err = "foo.cc:10: error: undefined bar"
    self.assertEqual(extract_meaningful_error_summary(direct_err), direct_err)


class ToolDirectiveExtractionTest(unittest.TestCase):
  """Pins the two tool-directive parsers against each other.

  conflicts.py previously used a local '^TOOL_...$' regex that missed
  every indented directive and merged run-on directives into one
  corrupted path. The fix routes prose through extract_tool_commands and
  code through extract_line_anchored_tool_commands; these tests pin why
  both exist, since collapsing them would reintroduce one bug or the
  other.
  """

  CPP_WITH_TOOL_ENUM = ("switch (kind) {\n"
                        "  case TOOL_TIP: return 1;\n"
                        "  case TOOL_BAR: return 2;\n"
                        "}")

  def test_indented_directive_is_found(self):
    """The old column-0 anchor missed these entirely."""
    text = "I need:\n  TOOL_READ_FILE: a/b.h 1-10"
    self.assertEqual(
        extract_line_anchored_tool_commands(text),
        ["TOOL_READ_FILE: a/b.h 1-10"])

  def test_in_code_enum_is_not_a_directive(self):
    """An indented C++ 'case TOOL_TIP:' must not be run as a tool."""
    self.assertEqual(
        extract_line_anchored_tool_commands(self.CPP_WITH_TOOL_ENUM), [])

  def test_prose_scanner_does_flag_in_code_enum(self):
    """Documents why the code path may not use the prose scanner."""
    self.assertEqual(len(extract_tool_commands(self.CPP_WITH_TOOL_ENUM)), 2)

  def test_prose_scanner_splits_run_on_directives(self):
    """Documents why the prose path may not use the line-anchored one."""
    runon = ("TOOL_READ_FILE: media/types.mojom"
             "TOOL_READ_FILE: media/base/decoder.h 1-40")
    self.assertEqual(len(extract_tool_commands(runon)), 2)
    self.assertEqual(len(extract_line_anchored_tool_commands(runon)), 1)

  def test_plain_code_yields_no_directives(self):
    self.assertEqual(
        extract_line_anchored_tool_commands("int x = 1;\nreturn x;"), [])

  def test_duplicates_are_collapsed(self):
    text = "TOOL_GREP: kStarboard\nTOOL_GREP: kStarboard"
    self.assertEqual(
        extract_line_anchored_tool_commands(text), ["TOOL_GREP: kStarboard"])

  def test_directive_cap_is_honored(self):
    text = "\n".join(f"TOOL_GREP: sym{i}" for i in range(10))
    self.assertEqual(len(extract_line_anchored_tool_commands(text)), 3)


class ChangeRecordReviewContextTest(unittest.TestCase):
  """trigger / rationale / model / conflict sides on AgentChangeRecord.

  These fields exist so the humans who take over an AI PR can see, per
  change, what prompted it, why the model did it that way, and for a
  conflict what the two sides were, without replaying the run.
  """

  def test_record_defaults_and_to_dict(self):
    rec = AgentChangeRecord("p", 1, "a.cc")
    self.assertEqual(rec.trigger, "")
    self.assertEqual(rec.rationale, "")
    self.assertEqual(rec.model, "")
    self.assertEqual(rec.upstream_side, "")
    self.assertEqual(rec.cobalt_side, "")
    d = AgentChangeRecord(
        "p",
        1,
        "a.cc",
        trigger="a.cc:1: error",
        rationale="because",
        model="m",
        upstream_side="int x = 2;",
        cobalt_side="int x = 1;").to_dict()
    self.assertEqual(d["trigger"], "a.cc:1: error")
    self.assertEqual(d["rationale"], "because")
    self.assertEqual(d["model"], "m")
    self.assertEqual(d["upstream_side"], "int x = 2;")
    self.assertEqual(d["cobalt_side"], "int x = 1;")

  def test_guidance_summary_drops_tool_transcripts(self):
    guidance = ("Upstream renamed settings to settings_. Keep the Cobalt "
                "block and rename it.\nTOOL_READ_FILE: a.h 1-20\n\n"
                "Tool Call: `TOOL_UPSTREAM_DIFF: a.h`\nResult:\n```\n"
                "- settings\n+ settings_\n```")
    self.assertEqual(
        guidance_summary(guidance),
        "Upstream renamed settings to settings_. Keep the Cobalt block and "
        "rename it.")
    self.assertEqual(guidance_summary(""), "")
    self.assertTrue(guidance_summary("x" * 2000).endswith(" [...]"))

  def test_patch_explanation_keeps_text_not_blocks(self):
    patch = ("The getter now returns std::optional, so unwrap it.\n\n"
             "FILE: a.cc\n<<<<<<< SEARCH\nfoo(x);\n=======\n"
             "if (x) foo(*x);\n>>>>>>> REPLACE\n\nThis matches upstream.")
    text = patch_explanation(patch)
    self.assertIn("returns std::optional", text)
    self.assertIn("This matches upstream.", text)
    self.assertNotIn("SEARCH", text)
    self.assertNotIn("foo(*x)", text)
    self.assertNotIn("FILE: a.cc", text)

  def test_patch_explanation_strips_unified_diff_body(self):
    # The blank line inside the hunk is a context line whose leading space
    # the model dropped; it must not end the hunk.
    patch = ("Drop the stray marker.\n--- a/a.cc\n+++ b/a.cc\n"
             "@@ -1,3 +1,2 @@\n-=======\n\n keep_this;\n\nThis matches "
             "upstream.\n")
    self.assertEqual(
        patch_explanation(patch),
        "Drop the stray marker.\nThis matches upstream.")
    self.assertEqual(patch_explanation(""), "")

  def test_phase4_record_carries_trigger_rationale_and_model(self):
    """run_resolution_loop fills the review fields from the iteration."""
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "cobalt", "sample.cc")
      os.makedirs(os.path.dirname(path))
      with open(path, "w", encoding="utf-8") as f:
        f.write("int value = 1;\n")
      engine = mock.MagicMock()
      engine.flash_model = "flash-test"
      engine.expert_model = "expert-test"
      engine.generate_expert_guidance.return_value = {
          "guidance": "Upstream bumped the value; follow it."
      }
      changes = []

      class Resolver(BaseResolver):
        """One failing iteration, then success."""

        @property
        def name(self):
          return "P4"

        def run_command(self, iteration):
          if iteration == 1:
            return False, "cobalt/sample.cc:1: error: mismatch", ""
          return True, "", ""

        def extract_diagnostics(self, build_output, siso_output):
          del build_output, siso_output
          return [
              Diagnostic(
                  file_path=os.path.join(tmpdir, "cobalt", "sample.cc"),
                  line_number=1,
                  error_message="error: mismatch")
          ]

        def resolve_diagnostic(self, diagnostic, **kwargs):
          del diagnostic, kwargs
          return (("Upstream changed the initial value to 2.\n"
                   "FILE: cobalt/sample.cc\n<<<<<<< SEARCH\nint value = 1;\n"
                   "=======\nint value = 2;\n>>>>>>> REPLACE\n"), "flash-test",
                  "cobalt/sample.cc")

      resolver = Resolver(
          repo_path=tmpdir,
          engine=engine,
          max_iterations=3,
          session_changes=changes)
      self.assertTrue(resolver.run_resolution_loop())

      rec = changes[0]
      self.assertTrue(rec.applied_cleanly)
      self.assertTrue(
          rec.trigger.startswith(
              os.path.join(tmpdir, "cobalt", "sample.cc") +
              ":1: error: mismatch"))
      self.assertEqual(rec.rationale,
                       "Upstream changed the initial value to 2.")
      self.assertEqual(rec.model, "flash-test")
      self.assertEqual(rec.upstream_side, "")  # not a conflict
      self.assertIsNone(rec.error)

  def test_phase4_rationale_falls_back_to_expert_plan(self):
    """A patch with no explanation keeps the pre-flight plan as rationale."""
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "a.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write("int value = 1;\n")
      engine = mock.MagicMock()
      engine.flash_model = "flash-test"
      engine.expert_model = "expert-test"
      engine.generate_expert_guidance.return_value = {
          "guidance": ("Bump the value.\n\nTool Call: `TOOL_READ_FILE: a.cc`"
                       "\nResult:\n```\nint value = 1;\n```")
      }
      changes = []

      class Resolver(BaseResolver):
        """Patch without any explanation text."""

        @property
        def name(self):
          return "P4"

        def run_command(self, iteration):
          return (iteration > 1), "a.cc:1: error: mismatch", ""

        def extract_diagnostics(self, build_output, siso_output):
          del build_output, siso_output
          return [
              Diagnostic(
                  file_path=path,
                  line_number=1,
                  error_message="error: mismatch")
          ]

        def resolve_diagnostic(self, diagnostic, **kwargs):
          del diagnostic, kwargs
          return ("<<<<<<< SEARCH\nint value = 1;\n=======\nint value = 2;\n"
                  ">>>>>>> REPLACE\n"), "flash-test", "a.cc"

      Resolver(
          repo_path=tmpdir,
          engine=engine,
          max_iterations=3,
          session_changes=changes).run_resolution_loop()
      self.assertEqual(changes[0].rationale, "Bump the value.")

  def test_phase1_record_carries_block_sides_and_expert_plan(self):
    """resolve_file_conflicts fills the review fields per conflict block."""
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "sample_conflict.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write(SAMPLE_CPP_CONFLICT)
      engine = mock.MagicMock()
      engine.generate_expert_guidance.return_value = {
          "guidance": ("Keep the Starboard pipeline; upstream only renamed "
                       "the default init.\n\nTool Call: `TOOL_UPSTREAM_DIFF: "
                       "sample_conflict.cc`\nResult:\n```\n-old\n+new\n```")
      }
      engine.resolve_conflict.return_value = {
          "replacement": ("#if BUILDFLAG(USE_STARBOARD_MEDIA)\n"
                          "  InitStarboardMediaPipeline();\n#else\n"
                          "  InitChromiumDefaultMediaPipeline();\n#endif"),
          "model_used": "expert-test",
      }
      changes = []
      self.assertTrue(
          resolve_file_conflicts(
              file_path=path,
              repo_path=tmpdir,
              git_context="",
              engine=engine,
              session_changes=changes))
      rec = changes[-1]
      self.assertEqual(rec.phase, "resolve_conflicts")
      self.assertEqual(rec.trigger,
                       "Conflict block #1 in sample_conflict.cc (lines 4-10)")
      self.assertEqual(rec.rationale, ("Keep the Starboard pipeline; upstream "
                                       "only renamed the default init."))
      self.assertEqual(rec.model, "expert-test")
      # SAMPLE_CPP_CONFLICT is a plain merge ('>>>>>>> origin/main'), so
      # upstream is the theirs side and Cobalt is HEAD.
      self.assertEqual(rec.upstream_side,
                       "  InitChromiumDefaultMediaPipeline();\n")
      self.assertEqual(rec.cobalt_side,
                       ("#if BUILDFLAG(USE_STARBOARD_MEDIA)\n"
                        "  InitStarboardMediaPipeline();\n#endif\n"))

  def test_phase1_expert_tool_results_are_fed_back_for_a_final_plan(self):
    """A tool-first expert gets its results back and its conclusion is kept.

    Before, Phase 1 called the expert once: a reply that started with a
    TOOL_ request left the worker with that line plus raw tool output, and
    the record with an empty rationale. Now the loop continues until the
    expert writes a plan.
    """
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "sample_conflict.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write(SAMPLE_CPP_CONFLICT)
      histories = []

      def guidance(**kwargs):
        histories.append(kwargs.get("investigation_history", ""))
        if len(histories) == 1:
          return {"guidance": "TOOL_READ_FILE: sample_conflict.cc 1-5"}
        return {
            "guidance": "Upstream only renamed the default init; keep "
                        "the Starboard branch."
        }

      engine = mock.MagicMock()
      engine.generate_expert_guidance.side_effect = guidance
      seen_worker_guidance = []

      def resolve(**kwargs):
        seen_worker_guidance.append(kwargs.get("expert_guidance", ""))
        return {
            "replacement": "  InitStarboardMediaPipeline();",
            "model_used": "flash-test"
        }

      engine.resolve_conflict.side_effect = resolve
      changes = []
      with mock.patch(
          "base_resolver.execute_local_tool", return_value="1: #include ..."):
        self.assertTrue(
            resolve_file_conflicts(
                file_path=path,
                repo_path=tmpdir,
                git_context="",
                engine=engine,
                session_changes=changes))
      # Two expert rounds: the tool result was fed back.
      self.assertEqual(len(histories), 2)
      self.assertIn("TOOL_READ_FILE: sample_conflict.cc 1-5", histories[1])
      self.assertIn("1: #include ...", histories[1])
      # The worker saw the final plan and the tool transcript.
      self.assertTrue(seen_worker_guidance[0].startswith(
          "Upstream only renamed the default init"))
      self.assertIn("Tool Call: `TOOL_READ_FILE", seen_worker_guidance[0])
      # The record keeps the plan, not the tool request.
      self.assertEqual(
          changes[-1].rationale,
          "Upstream only renamed the default init; keep the "
          "Starboard branch.")

  def test_rationale_placeholder_when_expert_never_writes_a_plan(self):
    """Tool requests through the whole budget leave NO_RATIONALE, not ""."""
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "sample_conflict.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write(SAMPLE_CPP_CONFLICT)
      engine = mock.MagicMock()
      engine.generate_expert_guidance.return_value = {
          "guidance": "TOOL_READ_FILE: sample_conflict.cc 1-5"
      }
      engine.resolve_conflict.return_value = {
          "replacement": "  InitStarboardMediaPipeline();",
          "model_used": "flash-test",
      }
      changes = []
      with mock.patch("base_resolver.execute_local_tool", return_value="x"):
        resolve_file_conflicts(
            file_path=path,
            repo_path=tmpdir,
            git_context="",
            engine=engine,
            session_changes=changes)
      self.assertEqual(engine.generate_expert_guidance.call_count,
                       EXPERT_MAX_ROUNDS)
      self.assertEqual(changes[-1].rationale, NO_RATIONALE)

  def test_phase1_mock_mode_records_mock_model(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      path = os.path.join(tmpdir, "sample_conflict.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write(SAMPLE_CPP_CONFLICT)
      changes = []
      resolve_file_conflicts(
          file_path=path,
          repo_path=tmpdir,
          git_context="",
          session_changes=changes,
          mock_mode=True)
      self.assertEqual(changes[0].model, "mock")
      self.assertIn("Conflict block #1", changes[0].trigger)
      self.assertIn("InitStarboardMediaPipeline", changes[0].cobalt_side)

  def test_format_change_log_groups_by_file(self):
    recs = [
        AgentChangeRecord(
            "resolve_conflicts",
            1,
            "a.cc",
            file_changes={"a.cc": "x"},
            trigger="Conflict block "
            "#1 in a.cc (lines 3-9)",
            rationale="Keep Cobalt\n"
            "side.",
            model="m1",
            commit="deadbeef1234"),
        AgentChangeRecord(
            "P4",
            2,
            "a.cc",
            file_changes={"a.cc": "y"},
            trigger="a.cc:4: error: no member",
            error="a.cc:9: "
            "error: other",
            model="m2"),
        AgentChangeRecord(
            "P4",
            3,
            "b.cc",
            file_changes={"b.cc": "z"},
            applied_cleanly=False,
            error="rejected"),
    ]
    log_md = format_change_log(recs)
    self.assertIn("## 5. Change Log", log_md)
    self.assertIn("### `a.cc`", log_md)
    self.assertNotIn("### `b.cc`", log_md)  # not applied: left out
    self.assertIn(
        "- **resolve_conflicts** iteration 1, m1, `deadbeef1234` (ok)", log_md)
    self.assertIn("  - why: Keep Cobalt side.", log_md)
    self.assertIn("- **P4** iteration 2, m2 (then: a.cc:9: error: other)",
                  log_md)
    self.assertIn("  - trigger: a.cc:4: error: no member", log_md)
    self.assertEqual(format_change_log([]), "")


class FormatChangeHistoryTest(unittest.TestCase):
  """Prior-attempt lines built from AgentChangeRecords."""

  def test_outcomes(self):
    records = [
        AgentChangeRecord(
            "p",
            1,
            "a.cc",
            error="rejected: x.h is upstream",
            applied_cleanly=False),
        AgentChangeRecord("p", 2, "a.cc", error="no member named X"),
        AgentChangeRecord("p", 3, "b.cc", error=None),
    ]
    lines = format_change_history(records).splitlines()
    self.assertEqual(
        lines[0], "- Iteration 1: patch for a.cc was not applied: "
        "rejected: x.h is upstream")
    self.assertEqual(lines[1],
                     "- Iteration 2: changed a.cc -> no member named X")
    self.assertEqual(lines[2], "- Iteration 3: changed b.cc -> passed")

  def test_window_keeps_only_trailing_records(self):
    records = [AgentChangeRecord("p", i, f"f{i}.cc") for i in range(12)]
    history = format_change_history(records)
    self.assertEqual(len(history.splitlines()), 6)
    self.assertIn("f11.cc", history)
    self.assertNotIn("f5.cc", history)

  def test_empty_input_yields_empty_string(self):
    self.assertEqual(format_change_history([]), "")

  def test_resolver_only_sees_its_own_phase(self):

    class _Resolver(BaseResolver):
      """Minimal resolver named "gn"."""

      @property
      def name(self):
        return "gn"

      def run_command(self, iteration):
        del iteration
        return True, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return []

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return "", "", ""

    shared = [
        AgentChangeRecord("conflicts", 1, "DEPS"),
        AgentChangeRecord("gn", 1, "BUILD.gn", error="unknown target"),
    ]
    resolver = _Resolver(tempfile.gettempdir(), session_changes=shared)
    self.assertEqual(resolver.change_history_prompt(),
                     "- Iteration 1: changed BUILD.gn -> unknown target")


class RejectedPatchLoopTest(unittest.TestCase):
  """A guard-rejected patch is recorded with its reason and is not 'stuck'."""

  def _run(self, patches, iterations):
    """Runs a loop whose build always fails with the same error."""

    class _Resolver(BaseResolver):
      """Resolver returning the given patches in order."""

      @property
      def name(self):
        return "compile"

      def run_command(self, iteration):
        del iteration
        return False, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return [Diagnostic(file_path=src, error_message="same error")]

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return patches.pop(0), "flash", "cobalt/a.cc"

    with tempfile.TemporaryDirectory() as tmpdir:
      src = os.path.join(tmpdir, "cobalt", "a.cc")
      os.makedirs(os.path.dirname(src))
      with open(src, "w", encoding="utf-8") as f:
        f.write("int a = 1;\n")
      resolver = _Resolver(tmpdir, max_iterations=iterations)
      with mock.patch.object(resolver, "_revert_to_baseline") as revert:
        self.assertFalse(resolver.run_resolution_loop())
    return resolver.session_changes, revert

  def test_rejection_reason_is_recorded_and_not_counted_as_stuck(self):
    rejected = ("FILE: out/gen/x.h\n<<<<<<< SEARCH\nint a = 1;\n=======\n"
                "int a = 2;\n>>>>>>> REPLACE")
    records, revert = self._run([rejected] * 5, iterations=5)
    self.assertEqual(len(records), 5)
    for rec in records:
      self.assertFalse(rec.applied_cleanly)
      self.assertTrue(
          rec.error.startswith("rejected: out/gen/x.h is a "
                               "generated build artifact"))
    revert.assert_not_called()

  def test_applied_patches_with_same_error_trigger_revert(self):
    applied = [
        f"<<<<<<< SEARCH\nint a = {i};\n=======\nint a = {i + 1};\n"
        ">>>>>>> REPLACE" for i in range(1, 5)
    ]
    records, revert = self._run(applied, iterations=4)
    self.assertTrue(all(r.applied_cleanly for r in records))
    revert.assert_called_once()

  def test_rejected_unified_diff_reason_is_recorded(self):
    diff = ("--- a/out/gen/x.h\n+++ b/out/gen/x.h\n@@ -1,1 +1,1 @@\n"
            "-int a = 1;\n+int a = 2;\n")
    records, _ = self._run([diff], iterations=1)
    self.assertTrue(records[0].error.startswith(
        "rejected: out/gen/x.h is a generated build artifact"))

  def test_unmatched_search_is_reported(self):
    unmatched = ("<<<<<<< SEARCH\nint b = 0;\n=======\nint b = 1;\n"
                 ">>>>>>> REPLACE")
    records, _ = self._run([unmatched], iterations=1)
    self.assertEqual(
        records[0].error, "SEARCH text not found in the current file; "
        "re-read the file and copy the lines exactly")

  def test_expert_preflight_notifies_when_tool_budget_exhausted(self):
    seen_histories = []

    class _MockEngine:
      flash_model = "flash"
      expert_model = "expert"

      def generate_expert_guidance(self, **kwargs):
        seen_histories.append(kwargs.get("investigation_history", ""))
        if len(seen_histories) <= 2:
          return {"guidance": "TOOL_READ_FILE: cobalt/a.cc 1-5"}
        return {"guidance": "Final synthesized architectural plan."}

    class _DummyResolver(BaseResolver):
      """Resolver stub for testing _expert_preflight."""

      @property
      def name(self):
        return "compile"

      def run_command(self, iteration):
        del iteration
        return True, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return []

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return "", "flash", "cobalt/a.cc"

    with tempfile.TemporaryDirectory() as tmpdir:
      src = os.path.join(tmpdir, "cobalt", "a.cc")
      os.makedirs(os.path.dirname(src))
      with open(src, "w", encoding="utf-8") as f:
        f.write("int a = 1;\n")
      resolver = _DummyResolver(tmpdir, engine=_MockEngine())
      # pylint: disable=protected-access
      plan = resolver._expert_preflight(
          [Diagnostic(file_path=src, error_message="err")],
          "raw log",
          stuck=False,
          stuck_count=0,
      )
    self.assertEqual(plan, "Final synthesized architectural plan.")
    self.assertEqual(len(seen_histories), 3)
    self.assertIn("TOOL BUDGET EXHAUSTED", seen_histories[2])


class PipelineFlagsTest(unittest.TestCase):
  """Command-line flags of run_rebase_pipeline."""

  def test_both_models_required(self):
    import run_rebase_pipeline  # pylint: disable=import-outside-toplevel
    parser = run_rebase_pipeline.build_arg_parser()
    parser.parse_args(["--model", "m", "--expert-model", "e"])
    with mock.patch("sys.stderr"), self.assertRaises(SystemExit):
      parser.parse_args(["--model", "m"])


class RestoreFilesOutsideBuildTest(unittest.TestCase):
  """restore_files_outside_build with a real git repo and a fake gn."""

  # Fake `gn gen <dir> --ide=json`: cobalt_apk depends on :lib (a.cc and the
  # public header b.h); :other (c.cc) is not in its closure.
  _FAKE_GN = """#!/usr/bin/env python3
import json, os, sys
targets = {
    "//cobalt/android:cobalt_apk": {"deps": ["//lib:lib"]},
    "//cobalt/android:cobalt_apk(//build/toolchain/linux:clang_x64)": {},
    "//lib:lib": {"sources": ["//a.cc"], "public": ["//b.h"]},
    "//other:other": {"sources": ["//c.cc"], "public": "*"},
}
with open(os.path.join(sys.argv[2], "project.json"), "w") as f:
  json.dump({"targets": targets}, f)
"""

  def test_restores_only_unbuilt_files(self):
    with tempfile.TemporaryDirectory() as repo, \
         tempfile.TemporaryDirectory() as bin_dir:
      gn = os.path.join(bin_dir, "gn")
      with open(gn, "w", encoding="utf-8") as f:
        f.write(self._FAKE_GN)
      os.chmod(gn, 0o755)

      def git(*args):
        subprocess.run(["git", *args],
                       cwd=repo,
                       check=True,
                       capture_output=True)

      def write(rel, text):
        os.makedirs(os.path.dirname(os.path.join(repo, rel)), exist_ok=True)
        with open(os.path.join(repo, rel), "w", encoding="utf-8") as f:
          f.write(text)

      files = ("a.cc", "b.h", "c.cc", "TestExpectations", "DEPS", "BUILD.gn")
      git("init", "-q")
      for rel in files:
        write(rel, "old\n")
      git("add", "-A")
      git("-c", "user.name=t", "-c", "user.email=t@t", "commit", "-qm", "s")
      start_sha = subprocess.run(["git", "rev-parse", "HEAD"],
                                 cwd=repo,
                                 capture_output=True,
                                 text=True,
                                 check=True).stdout.strip()
      for rel in files:
        write(rel, "new\n")
      write("out/android-arm_devel/args.gn", "target_os = \"android\"\n")

      path = bin_dir + os.pathsep + os.environ["PATH"]
      with mock.patch.dict(os.environ, {"PATH": path}):
        restored = restore_files_outside_build(repo, start_sha,
                                               "android-arm_devel",
                                               "cobalt_apk")

      self.assertEqual(restored, ["TestExpectations", "c.cc"])
      for rel in files:
        with open(os.path.join(repo, rel), encoding="utf-8") as f:
          expected = "old\n" if rel in restored else "new\n"
          self.assertEqual(f.read(), expected, rel)


class RepoPathSafetyTest(unittest.TestCase):
  """Paths from AI tool calls / patches must stay inside the repository."""

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.root = self._tmp.name
    self.repo = os.path.join(self.root, "src")
    os.makedirs(os.path.join(self.repo, "base"))
    self.inside = os.path.join(self.repo, "base", "foo.h")
    with open(self.inside, "w", encoding="utf-8") as f:
      f.write("// foo\n")
    self.outside = os.path.join(self.root, "secret.txt")
    with open(self.outside, "w", encoding="utf-8") as f:
      f.write("TOP-SECRET-CONTENT\n")

  def tearDown(self):
    self._tmp.cleanup()

  def test_is_within_repo(self):
    self.assertTrue(is_within_repo(self.inside, self.repo))
    self.assertTrue(is_within_repo(self.repo, self.repo))
    self.assertFalse(is_within_repo(self.outside, self.repo))
    self.assertFalse(is_within_repo(self.repo + "_other/x", self.repo))
    self.assertFalse(is_within_repo("", self.repo))

  def test_relative_and_compiler_paths_still_resolve(self):
    self.assertEqual(
        resolve_repo_file_path("base/foo.h", self.repo), self.inside)
    self.assertEqual(
        resolve_repo_file_path("//base/foo.h", self.repo), self.inside)
    # Compiler output relative to out/<dir>.
    self.assertEqual(
        resolve_repo_file_path("../../base/foo.h", self.repo), self.inside)

  def test_absolute_path_inside_repo_resolves(self):
    self.assertEqual(
        resolve_repo_file_path(self.inside, self.repo), self.inside)

  def test_absolute_path_outside_repo_rejected(self):
    self.assertEqual(resolve_repo_file_path(self.outside, self.repo), "")
    self.assertEqual(resolve_repo_file_path("/etc/passwd", self.repo), "")

  def test_traversal_outside_repo_rejected(self):
    self.assertEqual(
        resolve_repo_file_path("base/../../secret.txt", self.repo), "")

  def test_read_file_tool_refuses_outside_repo(self):
    out = execute_local_tool(f"TOOL_READ_FILE: {self.outside}", self.repo)
    self.assertNotIn("TOP-SECRET-CONTENT", out or "")
    self.assertIn("[ERROR]", out or "")

  def test_patch_target_outside_repo_rejected(self):
    self.assertIn("outside the repository",
                  patch_target_rejection("", self.outside, self.repo))

  def test_symlink_to_outside_is_outside_repo(self):
    link = os.path.join(self.repo, "base", "link.txt")
    os.symlink(self.outside, link)
    self.assertFalse(is_within_repo(link, self.repo))
    self.assertEqual(resolve_repo_file_path("base/link.txt", self.repo), "")

  def test_pipeline_ci_and_metadata_files_rejected(self):
    for rel, reason in (
        (".github/workflows/x.yaml", "run by the rebase pipeline or CI"),
        ("cobalt/build/gn.py", "run by the rebase pipeline or CI"),
        ("cobalt/devinfra/rebase/patching.py", "run by the rebase pipeline"),
        ("build/config/siso/configure_siso.py", "run by the rebase pipeline"),
        ("base/OWNERS", "repository metadata"),
        ("base/PRESUBMIT.py", "repository metadata"),
    ):
      target = os.path.join(self.repo, rel)
      self.assertIn(reason, patch_target_rejection(target, rel, self.repo), rel)
    self.assertEqual(
        patch_target_rejection(self.inside, "base/foo.h", self.repo), "")

  def test_rejection_uses_resolved_path_not_model_spelling(self):
    target = os.path.join(self.repo, "cobalt", "build", "configs", "x.gni")
    self.assertIn(
        "global build config",
        patch_target_rejection(target, "//cobalt/build/configs/x.gni",
                               self.repo))


class AutoninjaHelpersSafetyTest(unittest.TestCase):
  """Defensive behavior of autoninja path helpers."""

  def test_build_file_walk_terminates(self):
    with tempfile.TemporaryDirectory() as repo:
      self.assertIsNone(find_build_file_for_object("obj/a/b/c.o", repo))
      os.makedirs(os.path.join(repo, "a"))
      gn = os.path.join(repo, "a", "BUILD.gn")
      with open(gn, "w", encoding="utf-8") as f:
        f.write("")
      self.assertEqual(find_build_file_for_object("obj/a/b/c.o", repo), gn)


class AtomicPatchTest(unittest.TestCase):
  """Multi-block SEARCH/REPLACE patches apply all-or-nothing."""

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.repo = self._tmp.name
    self.a = os.path.join(self.repo, "a.cc")
    self.b = os.path.join(self.repo, "b.cc")
    with open(self.a, "w", encoding="utf-8") as f:
      f.write("int a = 1;\nint a2 = 2;\n")
    with open(self.b, "w", encoding="utf-8") as f:
      f.write("int b = 1;\n")

  def tearDown(self):
    self._tmp.cleanup()

  @staticmethod
  def _block(path, search, replace):
    return (f"FILE: {path}\n<<<<<<< SEARCH\n{search}\n=======\n{replace}\n"
            ">>>>>>> REPLACE\n")

  def test_failing_block_rolls_back_earlier_blocks(self):
    with open(self.a, "rb") as f:
      before = f.read()
    patch = (
        self._block("a.cc", "int a = 1;", "int a = 10;") +
        self._block("b.cc", "int does_not_exist;", "int x;"))
    self.assertEqual(_apply(patch, self.repo), [])
    with open(self.a, "rb") as f:
      self.assertEqual(f.read(), before)

  def test_multiple_blocks_same_file_all_apply(self):
    patch = (
        self._block("a.cc", "int a = 1;", "int a = 10;") +
        self._block("a.cc", "int a2 = 2;", "int a2 = 20;"))
    self.assertEqual(_apply(patch, self.repo), [self.a])
    with open(self.a, encoding="utf-8") as f:
      content = f.read()
    self.assertIn("int a = 10;", content)
    self.assertIn("int a2 = 20;", content)


class ExecScriptGuardTest(unittest.TestCase):
  """Any change to an exec_script() call stops the pipeline for a human."""

  _GN = ('x = exec_script("a.py",\n'
         '                [ "--safe" ],\n'
         '                "value")  # uses ")" in a comment\n'
         "deps = []\n")

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.repo = self._tmp.name
    self.gn = os.path.join(self.repo, "BUILD.gn")
    with open(self.gn, "w", encoding="utf-8") as f:
      f.write(self._GN)

  def tearDown(self):
    self._tmp.cleanup()

  def _changes(self, patch):
    return exec_script_changes(parse_patch(patch, self.repo), self.repo)

  def test_change_inside_multiline_call_is_detected(self):
    patch = ('FILE: BUILD.gn\n<<<<<<< SEARCH\n                [ "--safe" ],\n'
             '=======\n                [ "--evil" ],\n>>>>>>> REPLACE\n')
    self.assertEqual(self._changes(patch), ["BUILD.gn"])

  def test_added_call_in_unified_diff_is_detected(self):
    patch = ("--- a/BUILD.gn\n+++ b/BUILD.gn\n@@ -4 +4,2 @@\n deps = []\n"
             '+y = exec_script("b.py", [], "value")\n')
    self.assertEqual(self._changes(patch), ["BUILD.gn"])

  def test_edit_elsewhere_in_file_is_allowed(self):
    patch = ("FILE: BUILD.gn\n<<<<<<< SEARCH\ndeps = []\n=======\n"
             'deps = [ ":b" ]\n>>>>>>> REPLACE\n')
    self.assertEqual(self._changes(patch), [])

  def test_resolution_loop_stops_and_records_reason(self):
    patch = ('FILE: BUILD.gn\n<<<<<<< SEARCH\n                [ "--safe" ],\n'
             '=======\n                [ "--evil" ],\n>>>>>>> REPLACE\n')

    class _Resolver(BaseResolver):
      """Build always fails; the model always proposes the exec change."""

      @property
      def name(self):
        return "gn"

      def run_command(self, iteration):
        del iteration
        return False, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return [Diagnostic(error_message="gn error")]

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return patch, "flash", "BUILD.gn"

    resolver = _Resolver(self.repo, max_iterations=5)
    self.assertFalse(resolver.run_resolution_loop())
    self.assertTrue(resolver.needs_human())
    self.assertEqual(len(resolver.session_changes), 1)
    self.assertTrue(
        resolver.session_changes[0].error.startswith(NEEDS_HUMAN_PREFIX))
    with open(self.gn, encoding="utf-8") as f:
      self.assertEqual(f.read(), self._GN)

  _CONFLICT = ('x = exec_script("a.py",\n'
               "<<<<<<< HEAD\n"
               '                [ "--old" ],\n'
               "=======\n"
               '                [ "--new" ],\n'
               ">>>>>>> upstream\n"
               '                "value")\n')

  def _resolve_conflict(self, replacement=None):
    """Runs Phase 1 on the GN conflict; mock mode takes the upstream side."""
    with open(self.gn, "w", encoding="utf-8") as f:
      f.write(self._CONFLICT)
    engine = None
    if replacement is not None:
      engine = mock.MagicMock()
      engine.generate_expert_guidance.return_value = {"guidance": ""}
      engine.resolve_conflict.return_value = {"replacement": replacement}
    changes = []
    ok = resolve_file_conflicts(
        file_path=self.gn,
        repo_path=self.repo,
        git_context="",
        engine=engine,
        session_changes=changes,
        mock_mode=replacement is None)
    with open(self.gn, encoding="utf-8") as f:
      return ok, changes, f.read()

  def test_phase1_allows_upstream_side_of_exec_script_conflict(self):
    ok, _, content = self._resolve_conflict()
    self.assertTrue(ok)
    self.assertIn('[ "--new" ]', content)

  def test_phase1_allows_cobalt_side_of_exec_script_conflict(self):
    ok, _, content = self._resolve_conflict('                [ "--old" ],')
    self.assertTrue(ok)
    self.assertIn('[ "--old" ]', content)

  def test_phase1_allows_both_sides_without_markers(self):
    ok, _, content = self._resolve_conflict(
        '                [ "--old" ],\n                [ "--new" ],')
    self.assertTrue(ok)
    self.assertNotIn("<<<<<<<", content)

  def test_phase1_stops_on_model_written_exec_script(self):
    ok, changes, content = self._resolve_conflict(
        '                [ "--evil" ],')
    self.assertFalse(ok)
    self.assertTrue(changes[-1].error.startswith(NEEDS_HUMAN_PREFIX))
    self.assertEqual(content, self._CONFLICT)

  def test_report_lists_needs_human_reasons(self):
    record = AgentChangeRecord(
        phase="gn",
        iteration=1,
        target_file="BUILD.gn",
        error=NEEDS_HUMAN_PREFIX + "the fix changes exec_script() in BUILD.gn",
        applied_cleanly=False)
    path = write_rebase_report(
        self.repo,
        "android-arm",
        "devel",
        target="cobalt",
        model="flash",
        expert_model="expert",
        status="NEEDS HUMAN (Phase 3: GN Generation)",
        elapsed_seconds=1.0,
        repo_path=self.repo,
        session_changes=[record])
    with open(path, encoding="utf-8") as f:
      report = f.read()
    self.assertIn(
        "## Needs Human Decision\n- the fix changes exec_script() "
        "in BUILD.gn\n", report)

  def test_report_phase_table_uses_real_status(self):
    path = write_rebase_report(
        self.repo,
        "android-arm",
        "devel",
        target="cobalt",
        model="flash",
        expert_model="expert",
        status="FAILED (Phase 3: GN Generation)",
        elapsed_seconds=1.0,
        phase_status={
            "Phase 1": "Skipped",
            "Phase 2": "[OK] Completed",
            "Phase 3": "[FAILED]",
        },
        repo_path=self.repo)
    with open(path, encoding="utf-8") as f:
      rows = [l for l in f.read().splitlines() if l.startswith("| **Phase")]
    self.assertEqual([r.rsplit("|", 2)[1].strip() for r in rows],
                     ["Skipped", "[OK] Completed", "[FAILED]", "Not run"])

  def test_applied_patch_is_recorded_before_dependent_phase_runs(self):

    class _Resolver(BaseResolver):
      """Build fails; the re-run triggered by the patch then fails."""

      @property
      def name(self):
        return "compile"

      def run_command(self, iteration):
        del iteration
        return False, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del build_output, siso_output
        return [Diagnostic(error_message="error")]

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return ("FILE: BUILD.gn\n<<<<<<< SEARCH\ndeps = []\n=======\n"
                'deps = [ ":b" ]\n>>>>>>> REPLACE\n'), "flash", "BUILD.gn"

    def fail(modified_files):
      del modified_files
      raise RuntimeError("gn gen failed after a build fix")

    resolver = _Resolver(self.repo, max_iterations=2, on_patch_applied_fn=fail)
    with self.assertRaises(RuntimeError):
      resolver.run_resolution_loop()
    self.assertEqual(len(resolver.session_changes), 1)
    self.assertTrue(resolver.session_changes[0].applied_cleanly)


class ToolFixesTest(unittest.TestCase):
  """Read-only tool parsing, limits and timeouts."""

  def _cmd(self, directive):
    with mock.patch("tools.subprocess.run") as run:
      run.return_value = mock.Mock(stdout="x", stderr="", returncode=0)
      execute_local_tool(directive, "/repo")
    return run.call_args[0][0], run.call_args[1]

  def test_grep_keeps_unquoted_multi_word_query(self):
    cmd, _ = self._cmd("TOOL_GREP: struct AudioDecoderConfig")
    self.assertEqual(cmd[cmd.index("-e") + 1], "struct AudioDecoderConfig")
    self.assertIn("*.cc", cmd)

  def test_grep_last_token_path_is_filter(self):
    cmd, _ = self._cmd(
        "TOOL_GREP: struct AudioDecoderConfig media/mojom/types.mojom")
    self.assertEqual(cmd[cmd.index("-e") + 1], "struct AudioDecoderConfig")
    self.assertEqual(cmd[-2:], ["--", "media/mojom/types.mojom"])

  def test_git_log_count_is_capped_and_commands_time_out(self):
    cmd, kwargs = self._cmd("TOOL_GIT_LOG: 500 base")
    self.assertIn("-n50", cmd)
    self.assertIn("timeout", kwargs)

  def test_find_file_lists_tracked_files_by_name(self):
    cmd, _ = self._cmd("TOOL_FIND_FILE: supplementable")
    self.assertEqual(
        cmd, ["git", "ls-files", "--", ":(glob,icase)**/*supplementable*"])

  def test_gclient_sync_tool_removed(self):
    with mock.patch("tools.subprocess.run") as run:
      out = execute_local_tool("TOOL_GCLIENT_SYNC", "/repo")
    run.assert_not_called()
    self.assertIn("[ERROR]", out)

  def test_conflict_priority_uses_full_path(self):
    files = ["base/a.cc", "tools/clang/scripts/update.py", "DEPS", "BUILD.gn"]
    self.assertEqual(
        sorted(files, key=sort_conflict_priority),
        ["DEPS", "tools/clang/scripts/update.py", "BUILD.gn", "base/a.cc"])

  def test_failed_file_leaves_no_applied_conflict_records(self):
    with tempfile.TemporaryDirectory() as repo:
      path = os.path.join(repo, "a.cc")
      with open(path, "w", encoding="utf-8") as f:
        f.write("<<<<<<< HEAD\na\n=======\nb\n>>>>>>> upstream\nmid\n"
                "<<<<<<< HEAD\nc\n=======\nd\n>>>>>>> upstream\n")
      replies = iter(["b"])
      engine = mock.MagicMock()
      engine.generate_expert_guidance.return_value = {"guidance": ""}
      engine.resolve_conflict.side_effect = lambda **kw: {
          "replacement": next(replies, "<<<<<<< still conflicted")
      }
      changes = []
      ok = resolve_file_conflicts(
          file_path=path,
          repo_path=repo,
          git_context="",
          engine=engine,
          session_changes=changes)
      self.assertFalse(ok)
      self.assertEqual([c.applied_cleanly for c in changes], [False])


class ResolverInitTest(unittest.TestCase):
  """Resolver constructors reject unknown keyword arguments."""

  def test_misspelled_kwarg_raises(self):
    with self.assertRaises(TypeError):
      GNGenResolver("/tmp", "linux-x64x11", "devel", max_iteration=3)  # pylint: disable=unexpected-keyword-arg


class DiagnosticTest(unittest.TestCase):
  """Common Diagnostic base and its use in run_resolution_loop."""

  def test_trace_includes_location_snippet_and_notes(self):
    d = Diagnostic(
        error_message="boom",
        file_path="a.cc",
        line_number=7,
        raw_snippet="ctx",
        notes=["note: here"])
    self.assertEqual(d.trace(), "a.cc:7: boom\nSnippet:\nctx\nnote: here")
    self.assertEqual(Diagnostic(error_message="raw").trace(), "raw")

  def test_gclient_trace_prefers_sync_trace(self):
    d = GClientSyncDiagnostic(
        error_message="e", raw_output="out", diagnostic_trace="TRACE")
    self.assertEqual(d.trace(), "TRACE")
    self.assertEqual(d.file_path, "DEPS")

  def _resolver(self, outputs, seen):
    """Resolver whose command fails with each of `outputs`, then succeeds."""

    class _Resolver(BaseResolver):
      """Minimal resolver driven by canned command outputs."""

      @property
      def name(self):
        return "DiagTest"

      def run_command(self, iteration):
        if iteration <= len(outputs):
          return False, outputs[iteration - 1], ""
        return True, "", ""

      def extract_diagnostics(self, build_output, siso_output):
        del siso_output
        return [build_output]  # Bare string, as older subclasses return.

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del kwargs
        seen.append(diagnostic)
        return "", "flash", ""

    return _Resolver(tempfile.gettempdir(), max_iterations=5)

  def test_loop_wraps_bare_string_diagnostics(self):
    seen = []
    self.assertTrue(self._resolver(["raw failure"], seen).run_resolution_loop())
    self.assertEqual(len(seen), 1)
    self.assertIsInstance(seen[0], Diagnostic)
    self.assertEqual(seen[0].error_message, "raw failure")


class SearchReplaceMatchLevelsTest(unittest.TestCase):
  """apply_search_replace match levels and write-back."""

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.path = os.path.join(self._tmp.name, "f.cc")

  def tearDown(self):
    self._tmp.cleanup()

  def _run(self, content, search, replace):
    with open(self.path, "w", encoding="utf-8") as f:
      f.write(content)
    ok = apply_search_replace(self.path, search, replace)
    with open(self.path, encoding="utf-8") as f:
      return ok, f.read()

  def test_exact_substring(self):
    self.assertEqual(
        self._run("a = 1; b = 2;\n", "b = 2;", "b = 3;"),
        (True, "a = 1; b = 3;\n"))

  def test_trimmed_substring(self):
    self.assertEqual(
        self._run("x\n  y();\nz\n", "\n  y();  \n\n", "  w();\n"),
        (True, "x\n  w();\nz\n"))

  def test_reindented_lines_keep_rest_of_file(self):
    # No trailing newline at EOF: lines outside the window stay untouched.
    self.assertEqual(
        self._run("a\n    b\n    c\nlast", "b\nc", "B\nC"),
        (True, "a\nB\nC\nlast"))

  def test_collapsed_whitespace_and_line_numbers(self):
    self.assertEqual(
        self._run("int  x =  1;\nint y;\n", "12: int x = 1;\n13: int y;",
                  "14: int x = 2;\nint y;"), (True, "int x = 2;\nint y;\n"))

  def test_no_match_leaves_file_unchanged(self):
    self.assertEqual(self._run("a\nb\n", "missing", "x"), (False, "a\nb\n"))

  def test_trimmed_match_skips_mid_line_fragment(self):
    # "foo();" also occurs inside the first line; only the whole line is
    # a valid fuzzy match.
    self.assertEqual(
        self._run("x = foo();\n  foo();\n", "foo();  \n", "bar();"),
        (True, "x = foo();\n  bar();\n"))

  def test_trimmed_fragment_alone_does_not_match(self):
    self.assertEqual(
        self._run("x = foo();\n", " foo(); ", "bar();"),
        (False, "x = foo();\n"))

  def test_whole_line_match_keeps_missing_final_newline(self):
    self.assertEqual(
        self._run("a\n  b\n    last", "b\nlast", "B\nLAST"),
        (True, "a\nB\nLAST"))

  def test_fuzzy_match_spans_blank_lines(self):
    self.assertEqual(
        self._run("  a();\n\n  b();\nc();\n", "a();\n\nb();\n", "x();\n"),
        (True, "x();\nc();\n"))

  def test_numbered_code_in_unnumbered_block_is_kept(self):
    content = "  99: \"OK\",\n  100: \"Continue\",\n};\n"
    self.assertEqual(
        self._run(content, "  100: \"Continue\",\n};",
                  "  100: \"Continue\",\n  101: \"Switching\",\n};"),
        (True, "  99: \"OK\",\n  100: \"Continue\",\n"
         "  101: \"Switching\",\n};\n"))


class ParsePatchTest(unittest.TestCase):
  """parse_patch result is shared by apply and the change record."""

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.repo = self._tmp.name
    os.makedirs(os.path.join(self.repo, "base"))
    self.a = os.path.join(self.repo, "base", "a.cc")
    with open(self.a, "w", encoding="utf-8") as f:
      f.write("int a = 1;\nint b = 2;\n")

  def tearDown(self):
    self._tmp.cleanup()

  def test_blocks_resolved_and_grouped_per_file(self):
    patch = ("```\nFILE: base/a.cc\n<<<<<<< SEARCH\nint a = 1;\n=======\n"
             "int a = 10;\n>>>>>>> REPLACE\n\nFILE: //base/a.cc\n"
             "<<<<<<< SEARCH\nint b = 2;\n=======\nint b = 20;\n"
             ">>>>>>> REPLACE\n```")
    parsed = parse_patch(patch, self.repo)
    self.assertEqual([b.target_file for b in parsed.blocks], [self.a, self.a])
    self.assertEqual(apply_parsed_patch(parsed, self.repo), [self.a])
    changes = patch_file_changes(parsed, self.repo)
    self.assertEqual(list(changes), [os.path.join("base", "a.cc")])
    self.assertEqual(changes[os.path.join("base", "a.cc")].count("SEARCH"), 2)

  def test_delete_and_search_blocks_both_apply_in_order(self):
    patch = ("FILE: base/a.cc\n<<<<<<< DELETE\nint b = 2;\n>>>>>>> DELETE\n"
             "<<<<<<< SEARCH\nint a = 1;\n=======\n"
             "int a = 3;\n>>>>>>> REPLACE")
    parsed = parse_patch(patch, self.repo)
    # The SEARCH block has no FILE header and reuses the previous one.
    self.assertEqual([(b.kind, b.target_file) for b in parsed.blocks],
                     [("DELETE", self.a), ("SEARCH", self.a)])
    self.assertEqual(apply_parsed_patch(parsed, self.repo), [self.a])
    with open(self.a, encoding="utf-8") as f:
      self.assertEqual(f.read(), "int a = 3;\n\n")

  def test_unified_diff_keeps_paths_starting_with_a_or_b(self):
    os.makedirs(os.path.join(self.repo, "build"))
    path = os.path.join(self.repo, "build", "b.gn")
    with open(path, "w", encoding="utf-8") as f:
      f.write("x = 1\n")
    for header in ("--- build/b.gn\n+++ build/b.gn\n",
                   "--- a/build/b.gn\n+++ b/build/b.gn\n"):
      with open(path, "w", encoding="utf-8") as f:
        f.write("x = 1\n")
      diff = header + "@@ -1 +1 @@\n-x = 1\n+x = 2\n"
      self.assertEqual(_apply(diff, self.repo), [path], header)
      with open(path, encoding="utf-8") as f:
        self.assertEqual(f.read(), "x = 2\n")

  def test_unified_diff_skips_dev_null_header(self):
    diff = ("--- /dev/null\n+++ b/base/a.cc\n@@ -1 +1 @@\n"
            "-int a = 1;\n+int a = 7;\n")
    self.assertEqual(_apply(diff, self.repo), [self.a])

  def test_default_file_used_when_header_missing(self):
    patch = "<<<<<<< SEARCH\nint a = 1;\n=======\nint a = 5;\n>>>>>>> REPLACE"
    parsed = parse_patch(patch, self.repo, default_file="base/a.cc")
    self.assertEqual(parsed.blocks[0].target_file, self.a)

  def test_unified_diff_recorded_by_header(self):
    diff = "--- a/base/a.cc\n+++ b/base/a.cc\n@@ -1 +1 @@\n-int a = 1;\n+int a;"
    parsed = parse_patch(diff, self.repo)
    self.assertEqual(parsed.blocks, [])
    self.assertEqual(
        patch_file_changes(parsed, self.repo),
        {os.path.join("base", "a.cc"): diff})

  def test_outside_repo_target_rejected(self):
    patch = ("FILE: ../../etc/passwd\n<<<<<<< SEARCH\nroot\n=======\nx\n"
             ">>>>>>> REPLACE")
    parsed = parse_patch(patch, self.repo)
    self.assertEqual(parsed.blocks[0].target_file, "")
    self.assertEqual(apply_parsed_patch(parsed, self.repo), [])


class GitToolOptionInjectionTest(unittest.TestCase):
  """Model-supplied tool arguments must not be parsed as git options."""

  def test_git_diff_rejects_output_option(self):
    with tempfile.TemporaryDirectory() as repo:
      target = os.path.join(repo, "pwned")
      with mock.patch("tools.subprocess.run") as run:
        out = execute_local_tool(f"TOOL_GIT_DIFF: --output={target}", repo)
      run.assert_not_called()
      self.assertIn("[ERROR]", out or "")
      self.assertFalse(os.path.exists(target))

  def test_git_diff_allows_read_only_flags_and_paths(self):
    with mock.patch("tools.subprocess.run") as run:
      run.return_value = mock.Mock(stdout="diff", returncode=0)
      execute_local_tool("TOOL_GIT_DIFF: --stat -U5 HEAD -- -odd-name.cc",
                         "/repo")
    cmd = run.call_args[0][0]
    self.assertEqual(cmd[:2], ["git", "diff"])
    self.assertIn("--no-ext-diff", cmd)
    self.assertEqual(cmd[-5:], ["--stat", "-U5", "HEAD", "--", "-odd-name.cc"])

  def test_git_show_rejects_option_ref(self):
    with mock.patch("tools.subprocess.run") as run:
      out = execute_local_tool("TOOL_GIT_SHOW: --output=/tmp/x", "/repo")
    run.assert_not_called()
    self.assertIn("[ERROR]", out or "")

  def test_grep_query_passed_with_e_flag(self):
    with mock.patch("tools.subprocess.run") as run:
      run.return_value = mock.Mock(stdout="", returncode=1)
      execute_local_tool("TOOL_GREP: --open-files-in-pager=sh", "/repo")
    cmd = run.call_args[0][0]
    self.assertEqual(cmd[cmd.index("-e") + 1], "--open-files-in-pager=sh")


class UniqueTrackedFileFallbackTest(unittest.TestCase):
  """Basename fallback only resolves unique tracked files."""

  def setUp(self):
    self._tmp = tempfile.TemporaryDirectory()  # pylint: disable=consider-using-with
    self.repo = self._tmp.name
    for rel in ("x/BUILD.gn", "y/BUILD.gn", "x/unique.cc", "y/dir/foo.cc",
                "z/foo.cc"):
      path = os.path.join(self.repo, rel)
      os.makedirs(os.path.dirname(path), exist_ok=True)
      with open(path, "w", encoding="utf-8") as f:
        f.write("")
    subprocess.run(["git", "init", "-q"], cwd=self.repo, check=True)
    subprocess.run(["git", "add", "-A"], cwd=self.repo, check=True)

  def tearDown(self):
    self._tmp.cleanup()

  def test_unique_basename_resolves(self):
    self.assertEqual(
        _find_unique_tracked_file("unique.cc", self.repo),
        os.path.join(self.repo, "x", "unique.cc"))
    self.assertEqual(
        resolve_repo_file_path("gen/elsewhere/unique.cc", self.repo),
        os.path.join(self.repo, "x", "unique.cc"))

  def test_ambiguous_basename_rejected(self):
    self.assertEqual(_find_unique_tracked_file("BUILD.gn", self.repo), "")
    self.assertEqual(_find_unique_tracked_file("foo.cc", self.repo), "")

  def test_path_suffix_disambiguates(self):
    self.assertEqual(
        _find_unique_tracked_file("dir/foo.cc", self.repo),
        os.path.join(self.repo, "y", "dir", "foo.cc"))

  def test_glob_characters_rejected(self):
    self.assertEqual(_find_unique_tracked_file("uniq*.cc", self.repo), "")


def _git_repo_with(tmpdir, files):
  """Creates a git repo in tmpdir with `files` ({rel: content}) committed."""
  for rel, content in files.items():
    path = os.path.join(tmpdir, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
      f.write(content)
  for cmd in (["git", "init", "-q"], ["git", "add", "-A"], [
      "git", "-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m",
      "base"
  ]):
    subprocess.run(cmd, cwd=tmpdir, check=True, capture_output=True)


def _git_log(tmpdir):
  """[(subject, body)] of the commits after the base commit, oldest first."""
  out = subprocess.run(
      ["git", "log", "--reverse", "--format=%s%x00%b%x01", "HEAD"],
      cwd=tmpdir,
      capture_output=True,
      text=True,
      check=True).stdout
  entries = [e for e in out.split("\x01") if e.strip()]
  return [tuple(e.strip("\n").split("\x00", 1)) for e in entries][1:]


class CommitPerFixTest(unittest.TestCase):
  """One commit per fixed error / resolved file, with the reason in it."""

  @staticmethod
  def _patch(old, new, rel="cobalt/a.cc", why=""):
    text = f"{why}\n" if why else ""
    return (f"{text}FILE: {rel}\n<<<<<<< SEARCH\n{old}\n=======\n{new}\n"
            ">>>>>>> REPLACE\n")

  def _resolver_class(self, errors, patches):
    """A BaseResolver whose iteration i fails with errors[i-1] (None = ok)."""

    class _Resolver(BaseResolver):
      """Scripted build outcomes and patches."""

      @property
      def name(self):
        return "P4"

      def run_command(self, iteration):
        err = errors[iteration - 1]
        if err is None:
          return True, "", ""
        return False, err, ""

      def extract_diagnostics(self, build_output, siso_output):
        del siso_output
        path, _, msg = build_output.partition(": ")
        return [
            Diagnostic(
                file_path=os.path.join(self.repo_path, path),
                line_number=1,
                error_message=msg)
        ]

      def resolve_diagnostic(self, diagnostic, **kwargs):
        del diagnostic, kwargs
        return patches.pop(0), "flash-test", "cobalt/a.cc"

    return _Resolver

  def test_commit_when_error_changes_without_engine(self):
    """No engine: the error changing after a patch is the whole rule."""
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"cobalt/a.cc": "int a = 1;\nint b = 1;\n"})
      cls = self._resolver_class(
          ["cobalt/a.cc: error: bad a", "cobalt/a.cc: error: bad b", None], [
              self._patch("int a = 1;", "int a = 2;", why="a must be 2."),
              self._patch("int b = 1;", "int b = 2;", why="b must be 2."),
          ])
      changes = []
      resolver = cls(
          tmpdir,
          max_iterations=3,
          session_changes=changes,
          commit_per_fix=True)
      self.assertTrue(resolver.run_resolution_loop())

      log_entries = _git_log(tmpdir)
      self.assertEqual(len(log_entries), 2)
      self.assertEqual(log_entries[0][0], "cobalt/a.cc: bad a")
      self.assertIn("Trigger:", log_entries[0][1])
      self.assertIn("Why: a must be 2.", log_entries[0][1])
      self.assertIn("Model: flash-test", log_entries[0][1])
      self.assertEqual(log_entries[1][0], "cobalt/a.cc: bad b")
      self.assertIn("Outcome: build passed", log_entries[1][1])
      self.assertTrue(all(c.commit for c in changes))
      self.assertNotEqual(changes[0].commit, changes[1].commit)
      # Nothing left uncommitted.
      status = subprocess.run(["git", "status", "--porcelain"],
                              cwd=tmpdir,
                              capture_output=True,
                              text=True,
                              check=True).stdout
      self.assertEqual(status, "")

  def test_same_error_holds_and_loop_end_flushes_as_wip(self):
    """Patches that did not change the error are held, then flushed as WIP."""
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"cobalt/a.cc": "int a = 1;\n"})
      cls = self._resolver_class(["cobalt/a.cc: error: bad a"] * 2, [
          self._patch("int a = 1;", "int a = 2;"),
          self._patch("int a = 2;", "int a = 3;"),
      ])
      changes = []
      resolver = cls(
          tmpdir,
          max_iterations=2,
          session_changes=changes,
          commit_per_fix=True)
      self.assertFalse(resolver.run_resolution_loop())

      log_entries = _git_log(tmpdir)
      self.assertEqual(len(log_entries), 1)
      subject, body = log_entries[0]
      self.assertTrue(subject.startswith("WIP cobalt/a.cc: bad a"))
      self.assertIn("Unverified", body)
      self.assertIn("Why (P4 iteration 1)", body)
      self.assertIn("Why (P4 iteration 2)", body)
      self.assertEqual(changes[0].commit, changes[1].commit)

  def test_engine_judgement_decides_and_names_the_commit(self):
    """With an engine, HOLD keeps the patches pending; COMMIT uses its text."""
    judgements = iter([
        '{"decision": "HOLD", "reason": "same problem, new message"}',
        ('{"decision": "COMMIT", "subject": "media: follow upstream rename",'
         ' "body": "Upstream renamed Foo to Bar.", "reason": "done"}'),
        '{"subject": "cobalt/a.cc: bump b", "body": "b follows upstream."}',
    ])
    engine = mock.MagicMock()
    engine.flash_model = "flash-test"
    engine.expert_model = "expert-test"
    engine.generate_expert_guidance.return_value = {"guidance": "Fix it."}
    engine.judge_commit.side_effect = lambda **kw: {
        "judgement": next(judgements)
    }
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"cobalt/a.cc": "int a = 1;\nint b = 1;\n"})
      cls = self._resolver_class([
          "cobalt/a.cc: error: bad a", "cobalt/a.cc: error: bad a again",
          "cobalt/a.cc: error: bad b", None
      ], [
          self._patch("int a = 1;", "int a = 2;"),
          self._patch("int a = 2;", "int a = 3;"),
          self._patch("int b = 1;", "int b = 2;"),
      ])
      changes = []
      resolver = cls(
          tmpdir,
          engine=engine,
          max_iterations=4,
          session_changes=changes,
          commit_per_fix=True)
      self.assertTrue(resolver.run_resolution_loop())

      log_entries = _git_log(tmpdir)
      self.assertEqual([s for s, _ in log_entries],
                       ["media: follow upstream rename", "cobalt/a.cc: bump b"])
      body = log_entries[0][1]
      self.assertTrue(body.startswith("Upstream renamed Foo to Bar."))
      self.assertIn("Trigger: cobalt/a.cc:1: error: bad a", body)
      self.assertIn("Outcome: cobalt/a.cc:1: error: bad b", body)
      self.assertNotIn("Why", body)
      self.assertIn("Outcome: build passed", log_entries[1][1])
      # The two held patches went into one commit.
      self.assertEqual(changes[0].commit, changes[1].commit)
      self.assertNotEqual(changes[1].commit, changes[2].commit)
      # Asked once per commit: decide for the error change, message only
      # when the build passed.
      calls = engine.judge_commit.call_args_list
      self.assertEqual([c.kwargs["decide"] for c in calls], [True, True, False])
      self.assertIn("int a = 2;", calls[0].kwargs["diff"])
      self.assertIn("bad a again", calls[0].kwargs["outcome"])
      self.assertEqual(calls[2].kwargs["outcome"], "build passed")

  def test_off_by_default_leaves_working_tree_uncommitted(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"cobalt/a.cc": "int a = 1;\n"})
      cls = self._resolver_class(["cobalt/a.cc: error: bad a", None],
                                 [self._patch("int a = 1;", "int a = 2;")])
      self.assertTrue(cls(tmpdir, max_iterations=2).run_resolution_loop())
      self.assertEqual(_git_log(tmpdir), [])

  def test_phase1_commits_each_resolved_file(self):
    engine = mock.MagicMock()
    engine.generate_expert_guidance.return_value = {
        "guidance": "Keep the Starboard pipeline; upstream renamed the init."
    }
    engine.resolve_conflict.return_value = {
        "replacement": ("#if BUILDFLAG(USE_STARBOARD_MEDIA)\n"
                        "  InitStarboardMediaPipeline();\n#else\n"
                        "  InitChromiumDefaultMediaPipeline();\n#endif"),
        "model_used": "expert-test",
    }
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(
          tmpdir, {
              "cobalt/one.cc": SAMPLE_CPP_CONFLICT,
              "cobalt/two.cc": SAMPLE_CPP_CONFLICT,
          })
      changes = []
      resolver = ConflictResolver(
          tmpdir, engine=engine, session_changes=changes, commit_per_fix=True)
      with mock.patch(
          "conflicts.find_all_conflicted_files",
          return_value=["cobalt/one.cc", "cobalt/two.cc"]):
        self.assertTrue(resolver.run_resolution_loop())

      log_entries = _git_log(tmpdir)
      # The mock engine's judge_commit answer is unreadable, so the
      # record-built message is used.
      self.assertEqual([s for s, _ in log_entries], [
          "cobalt/one.cc: resolve merge conflict",
          "cobalt/two.cc: resolve merge conflict",
      ])
      self.assertIn("Block #1 (lines 4-10): Keep the Starboard pipeline",
                    log_entries[0][1])
      self.assertIn("Conflict block: #1 (lines 4-10)", log_entries[0][1])
      self.assertIn("Model: expert-test", log_entries[0][1])
      kw = engine.judge_commit.call_args_list[0].kwargs
      self.assertFalse(kw["decide"])
      self.assertIn("Keep the Starboard pipeline", kw["rationales"])
      self.assertIn("InitStarboardMediaPipeline", kw["diff"])
      self.assertEqual(len(changes), 2)
      self.assertNotEqual(changes[0].commit, changes[1].commit)
      shown = subprocess.run(["git", "show", "--stat", "--format=", "HEAD"],
                             cwd=tmpdir,
                             capture_output=True,
                             text=True,
                             check=True).stdout
      self.assertIn("cobalt/two.cc", shown)
      self.assertNotIn("cobalt/one.cc", shown)

  def test_parse_judgement(self):
    self.assertEqual(parse_judgement(""), {})
    self.assertEqual(parse_judgement("not json"), {})
    self.assertEqual(parse_judgement('{"decision": "MAYBE"}'), {})
    got = parse_judgement('```json\n{"decision": "commit", "subject": "s",'
                          ' "body": "b", "reason": "r"}\n```')
    self.assertEqual(got, {
        "decision": "COMMIT",
        "subject": "s",
        "body": "b",
        "reason": "r"
    })

  def test_fix_commit_message_subject_from_trigger(self):
    rec = AgentChangeRecord(
        "P4",
        3,
        "cobalt/media/foo.cc",
        file_changes={"cobalt/media/foo.cc": "x"},
        applied_cleanly=True,
        trigger=("../../cobalt/media/foo.cc:12:5: error: no member named "
                 "'Bar' in 'Foo'; did you mean 'Baz'? " + "x" * 100),
        rationale="Upstream renamed Bar to Baz.",
        model="flash-test")
    subject, body = fix_commit_message([rec], "P4")
    self.assertTrue(subject.startswith("cobalt/media/foo.cc: no member named"))
    self.assertLessEqual(len(subject), 72)
    self.assertIn("Why: Upstream renamed Bar to Baz.", body)
    self.assertNotIn("Files:", body)
    # A model-proposed subject wins; the trailer stays.
    subject, body = fix_commit_message([rec], "P4", "media: rename Bar",
                                       "Because.")
    self.assertEqual(subject, "media: rename Bar")
    self.assertTrue(body.startswith("Because.\n\nTrigger:"))
    # The model's prose is re-wrapped at 72 columns.
    _, body = fix_commit_message([rec], "P4", "s", "word " * 30)
    self.assertTrue(all(len(l) <= 72 for l in body.splitlines()[:3]))
    self.assertEqual(body.count("\n", 0, body.index("Trigger")), 4)
    # Proposed subjects over 72 chars or multiline are capped.
    long_subject = "media: " + "very long description " * 10 + "\nsecond line"
    subject, _ = fix_commit_message([rec], "P4", long_subject)
    self.assertLessEqual(len(subject), 72)
    self.assertNotIn("\n", subject)
    self.assertTrue(subject.endswith("..."))

  def test_revert_to_baseline_drops_held_patches_of_that_file(self):
    from base_resolver import _LoopState  # pylint: disable=import-outside-toplevel
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {
          "cobalt/a.cc": "int a = 1;\n",
          "cobalt/b.cc": "int b = 1;\n"
      })
      resolver = self._resolver_class([], [])(tmpdir, commit_per_fix=True)
      resolver.committer = FixCommitter(tmpdir, "P4")
      for i, rel in enumerate(("cobalt/a.cc", "cobalt/b.cc"), 1):
        resolver.committer.add(
            AgentChangeRecord("P4", i, rel, file_changes={rel: "x"}), "bad a")
      with open(
          os.path.join(tmpdir, "cobalt/a.cc"), "w", encoding="utf-8") as f:
        f.write("int a = 2;\n")
      state = _LoopState(last_error="bad a", stuck_count=3)
      resolver._revert_to_baseline(  # pylint: disable=protected-access
          os.path.join(tmpdir, "cobalt/a.cc"), state)
      self.assertEqual([r.target_file for r in resolver.committer.pending],
                       ["cobalt/b.cc"])
      self.assertEqual(resolver.committer.trigger, "bad a")
      resolver._revert_to_baseline(  # pylint: disable=protected-access
          os.path.join(tmpdir, "cobalt/b.cc"), state)
      self.assertEqual(resolver.committer.pending, [])
      self.assertEqual(resolver.committer.trigger, "")

  def test_commit_skips_local_pre_commit_hooks(self):
    """A hook that rewrites or rejects files must not drop the fix commit."""
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"BUILD.gn": "a\n"})
      for key, val in (("user.name", "t"), ("user.email", "t@t")):
        subprocess.run(["git", "config", key, val], cwd=tmpdir, check=True)
      hook = os.path.join(tmpdir, ".git", "hooks", "pre-commit")
      with open(hook, "w", encoding="utf-8") as f:
        f.write("#!/bin/sh\necho formatted > BUILD.gn\nexit 1\n")
      os.chmod(hook, 0o755)
      with open(os.path.join(tmpdir, "BUILD.gn"), "w", encoding="utf-8") as f:
        f.write("b\n")

      sha = commit_files(tmpdir, ["BUILD.gn"], "Fix BUILD.gn")

      self.assertTrue(sha)
      self.assertEqual(_git_log(tmpdir)[-1][0], "Fix BUILD.gn")
      status = subprocess.run(["git", "status", "--porcelain"],
                              cwd=tmpdir,
                              capture_output=True,
                              text=True,
                              check=True).stdout
      self.assertEqual(status, "")

  def test_get_working_diff_preserves_diff_across_commits(self):
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {"cobalt/a.cc": "int a = 1;\n"})
      cls = self._resolver_class(
          ["cobalt/a.cc: error: bad a", "cobalt/a.cc: error: bad b", None], [
              self._patch("int a = 1;", "int a = 2;"),
              self._patch("int a = 2;", "int a = 3;"),
          ])
      resolver = cls(tmpdir, max_iterations=3, commit_per_fix=True)
      self.assertTrue(resolver.run_resolution_loop())
      diff = resolver.get_working_diff()
      self.assertIn("-int a = 1;", diff)
      self.assertIn("+int a = 3;", diff)

  def test_nested_on_patch_applied_changes_collected_into_pending_fix(self):
    from base_resolver import _LoopState  # pylint: disable=import-outside-toplevel
    with tempfile.TemporaryDirectory() as tmpdir:
      _git_repo_with(tmpdir, {
          "cobalt/a.cc": "int a = 1;\n",
          "cobalt/b.cc": "int b = 1;\n"
      })
      changes = []

      def nested_hook(modified):
        if "cobalt/a.cc" in modified[0]:
          with open(
              os.path.join(tmpdir, "cobalt/b.cc"), "w", encoding="utf-8") as f:
            f.write("int b = 2;\n")
          nested_rec = AgentChangeRecord(
              phase="P3",
              iteration=1,
              target_file="cobalt/b.cc",
              file_changes={"cobalt/b.cc": "int b = 2;"},
              applied_cleanly=True,
              rationale="Nested fix for b",
          )
          changes.append(nested_rec)

      cls = self._resolver_class([], [])
      resolver = cls(
          tmpdir,
          session_changes=changes,
          on_patch_applied_fn=nested_hook,
          commit_per_fix=True)
      resolver.committer = FixCommitter(tmpdir, "P4")
      state = _LoopState(iteration=1, last_error="bad a")
      patch = self._patch(
          "int a = 1;", "int a = 2;", rel="cobalt/a.cc", why="Fix a")
      resolver._apply_and_record(  # pylint: disable=protected-access
          patch, "model", "cobalt/a.cc", state)

      self.assertEqual([r.target_file for r in resolver.committer.pending],
                       ["cobalt/a.cc", "cobalt/b.cc"])
      self.assertEqual(resolver.committer.trigger, "bad a")

  def test_fallback_message_names_changed_file_and_skips_markdown(self):
    """Seen in the M148.7752 run: wrong file in the subject, '###' as Why."""
    rec = AgentChangeRecord(
        "P3",
        1,
        "components/optimization_guide/core/inference/BUILD.gn",
        file_changes={"media/webrtc/BUILD.gn": "x"},
        applied_cleanly=True,
        trigger=("/repo/components/optimization_guide/core/inference/BUILD.gn"
                 ":8: ERROR at //components/optimization_guide/core/inference/"
                 "BUILD.gn:8:1: Assertion failed. | assert(x)"),
        rationale=("### Root Cause Analysis\n1. **HEAD**: upstream added a "
                   "dep on `tflite_support`.\n```gn\ndeps += [ \"x\" ]\n```\n"
                   "- Cobalt drops it. So the assert must be relaxed."),
        model="m")
    subject, body = fix_commit_message([rec], "P3", repo_path="/repo")
    self.assertEqual(subject,
                     "media/webrtc/BUILD.gn: Assertion failed. | assert(x)")
    self.assertIn(
        "Why: HEAD: upstream added a dep on `tflite_support`. "
        "Cobalt drops it. So the assert must be relaxed.", body)
    self.assertNotIn("###", body)
    self.assertNotIn("deps +=", body)
    self.assertIn("Trigger: components/optimization_guide", body)
    self.assertNotIn("/repo/", body)

  def test_pipeline_flag_defaults_off(self):
    import run_rebase_pipeline  # pylint: disable=import-outside-toplevel
    parser = run_rebase_pipeline.build_arg_parser()
    base = ["--model", "m", "--expert-model", "e"]
    self.assertFalse(parser.parse_args(base).commit_per_fix)
    self.assertTrue(
        parser.parse_args(base + ["--commit-per-fix"]).commit_per_fix)


class UpstreamCLLookupTest(unittest.TestCase):
  """Tests for upstream_cl (Gerrit/Gitiles lookup, DEPS subrepos, trailers)."""

  _SAMPLE_DEPS = """
vars = {
  'chromium_git': 'https://chromium.googlesource.com',
  'webrtc_git': 'https://webrtc.googlesource.com',
  'v8_revision': 'f3cde89bb08c0000000000000000000000000002',
}
deps = {
  'src/third_party/icu':
    Var('chromium_git') + '/chromium/deps/icu.git' + '@' + 'b4aae6832c06df9d538d41b249403cf0678f16b4',
  'src/v8':
    Var('chromium_git') + '/v8/v8.git' + '@' + Var('v8_revision'),
  'src/third_party/webrtc':
    Var('webrtc_git') + '/src.git' + '@' + 'e68d24dcebce0000000000000000000000000002',
}
"""

  def setUp(self):
    super().setUp()
    upstream_cl.clear_caches()

  def tearDown(self):
    upstream_cl.clear_caches()
    super().tearDown()

  def test_parse_deps_subrepos_maps_vars_and_literals(self):
    subrepos = upstream_cl.parse_deps_subrepos(self._SAMPLE_DEPS)
    self.assertEqual(
        subrepos["third_party/icu"],
        ("chromium", "chromium/deps/icu",
         "b4aae6832c06df9d538d41b249403cf0678f16b4"),
    )
    self.assertEqual(
        subrepos["v8"],
        ("chromium", "v8/v8", "f3cde89bb08c0000000000000000000000000002"),
    )
    self.assertEqual(
        subrepos["third_party/webrtc"],
        ("webrtc", "src", "e68d24dcebce0000000000000000000000000002"),
    )

  def _make_roll_repo(self, tmpdir: str) -> None:
    """Creates a minimal git repo with a parent commit and 'Update to 148.'."""
    old_deps = self._SAMPLE_DEPS.replace(
        "f3cde89bb08c0000000000000000000000000002",
        "8941ded106430000000000000000000000000001",
    )
    _git_repo_with(
        tmpdir,
        {
            "chrome/VERSION": "MAJOR=148\nMINOR=0\nBUILD=7739\nPATCH=0\n",
            "DEPS": old_deps,
        },
    )
    with open(
        os.path.join(tmpdir, "chrome/VERSION"), "w", encoding="utf-8") as f:
      f.write("MAJOR=148\nMINOR=0\nBUILD=7752\nPATCH=0\n")
    with open(os.path.join(tmpdir, "DEPS"), "w", encoding="utf-8") as f:
      f.write(self._SAMPLE_DEPS)
    subprocess.run(
        [
            "git",
            "-c",
            "user.name=t",
            "-c",
            "user.email=t@t",
            "commit",
            "-qam",
            "Update to 148.7752.",
        ],
        cwd=tmpdir,
        check=True,
    )

  def test_lookup_and_tool_upstream_log_for_chromium_and_subrepo(self):

    requests = []

    def fake_gerrit_json(host, path):
      requests.append((host, path))
      self.assertEqual(host, "chromium.googlesource.com")
      if path.startswith("chromium/src/+log/148.0.7739.0..148.0.7752.0/"
                         "media/webrtc/BUILD.gn?"):
        return {
            "log": [{
                "commit":
                    "16777dd522270000000000000000000000000000",
                "message":
                    ("Remove build_with_tflite_lib\n\n"
                     "Unused since M146.\n\n"
                     "Bug: 400000\n"
                     "Change-Id: I123\n"
                     "Reviewed-on: https://chromium-review.googlesource.com"
                     "/c/chromium/src/+/7695593\n"),
            }]
        }
      if path.startswith("v8/v8/+log/8941ded106430000000000000000000000000001"
                         "..f3cde89bb08c0000000000000000000000000002/"
                         "src/base/cpu/cpu.h?"):
        return {
            "log": [{
                "commit":
                    "89ad1535c3430000000000000000000000000000",
                "message": ("[base] Refactor CPU detection\n\n"
                            "Reviewed-on: https://chromium-review"
                            ".googlesource.com/c/v8/v8/+/7683553\n"),
            }]
        }
      return {"log": []}

    with tempfile.TemporaryDirectory() as tmpdir:
      self._make_roll_repo(tmpdir)
      with mock.patch.object(
          upstream_cl, "_gerrit_json", side_effect=fake_gerrit_json):
        # ICU's pin is the same on both sides of the roll: no request at all.
        self.assertEqual(
            upstream_cl.lookup_upstream_cls(tmpdir, "third_party/icu/BUILD.gn"),
            [])
        self.assertEqual(requests, [])

        out = execute_local_tool("TOOL_UPSTREAM_LOG: media/webrtc/BUILD.gn",
                                 tmpdir)
        self.assertIn(
            "- 16777dd52227 | https://crrev.com/c/7695593 | "
            "Remove build_with_tflite_lib", out)
        self.assertIn("Unused since M146.", out)
        self.assertNotIn("Change-Id:", out)

        v8_out = execute_local_tool("TOOL_UPSTREAM_LOG: v8/src/base/cpu/cpu.h",
                                    tmpdir)
        self.assertIn("v8/v8:src/base/cpu/cpu.h", v8_out)
        self.assertIn(
            "- 89ad1535c343 | https://crrev.com/c/7683553 | "
            "[base] Refactor CPU detection", v8_out)

        # Transcript CLs from TOOL_UPSTREAM_LOG on a related file are picked up,
        # while DEPS and Cobalt-only paths are skipped.
        refs = upstream_cl.upstream_ref_lines(
            tmpdir,
            ["DEPS", "cobalt/foo.cc", "media/webrtc/BUILD.gn"],
            tool_transcript=v8_out,
        )
        self.assertEqual(refs, [
            "https://crrev.com/c/7695593 "
            "(16777dd52227 Remove build_with_tflite_lib)",
            "https://crrev.com/c/7683553 "
            "(89ad1535c343 [base] Refactor CPU detection)",
        ])

  def test_gerrit_json_uses_gerrit_util_default_auth(self):
    gu = upstream_cl.gerrit_util
    with mock.patch.object(gu, "CreateHttpConn") as create, mock.patch.object(
        gu, "ReadHttpJsonResponse", return_value={"ok": 1}) as read:
      res = upstream_cl._gerrit_json(  # pylint: disable=protected-access
          "chromium.googlesource.com", "chromium/src/+/HEAD?format=JSON")
    self.assertEqual(res, {"ok": 1})
    # No authenticator passed: gerrit_util chooses it like `git cl` does.
    create.assert_called_once_with(
        "chromium.googlesource.com",
        "chromium/src/+/HEAD?format=JSON",
        timeout=30)
    read.assert_called_once_with(create.return_value)

    with mock.patch.object(
        gu, "CreateHttpConn",
        side_effect=gu.GerritError(429, "quota")), self.assertLogs(
            upstream_cl.log, "WARNING"):
      self.assertIsNone(
          upstream_cl._gerrit_json(  # pylint: disable=protected-access
              "chromium.googlesource.com", "x"))

  def test_commit_messages_and_report_include_upstream_refs(self):
    ref = "https://crrev.com/c/7695593 (16777dd52227 Remove tflite option)"
    c_rec = AgentChangeRecord(
        "resolve_conflicts",
        1,
        "media/webrtc/BUILD.gn",
        file_changes={"media/webrtc/BUILD.gn": "x"},
        applied_cleanly=True,
        trigger="Conflict block #1 in media/webrtc/BUILD.gn (lines 1-5)",
        rationale="Keep Cobalt guard.",
        model="m",
        upstream_refs=[ref],
    )
    _, c_body = conflict_commit_message("media/webrtc/BUILD.gn", [c_rec])
    self.assertIn(f"Upstream: {ref}", c_body)
    self.assertIn(f"Upstream CL: {ref}", conflict_rationales([c_rec]))

    f_rec = AgentChangeRecord(
        "P3",
        1,
        "media/webrtc/BUILD.gn",
        file_changes={"media/webrtc/BUILD.gn": "x"},
        applied_cleanly=True,
        trigger="media/webrtc/BUILD.gn:1: Assertion failed",
        rationale="Drop removed option.",
        model="m",
        upstream_refs=[ref],
    )
    _, f_body = fix_commit_message([f_rec], "P3")
    self.assertIn(f"Upstream: {ref}", f_body)
    self.assertIn(f"Upstream CL: {ref}", fix_rationales([f_rec]))
    self.assertIn(f"  - upstream: {ref}", format_change_log([f_rec]))


if __name__ == "__main__":
  unittest.main()
