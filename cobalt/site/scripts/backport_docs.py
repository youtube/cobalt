#!/usr/bin/env python3
# Copyright 2026 The Cobalt Authors. All Rights Reserved.
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
"""Backports reference markdown documentation back into C header comments."""

import argparse
import os
import re


class HeaderParser:

  def __init__(self, lines):
    self.lines = lines
    self.comments = {}
    self.decl_start = {}
    self.overview_comment = None
    self.symbols = {}
    self.params = {}

  def parse(self):
    in_comment = False
    comment_start = -1
    comment_lines = []
    code_accumulator = []
    code_start = -1
    current_struct_name = None
    current_enum_name = None

    i = 0
    while i < len(self.lines):
      line = self.lines[i]
      stripped = line.strip()

      if line.startswith("// Module Overview:"):
        start_idx = i
        while i < len(self.lines) and self.lines[i].strip().startswith("//"):
          i += 1
        self.overview_comment = (start_idx, i - 1)
        continue

      if stripped.startswith("//"):
        if code_start == -1:
          if not in_comment:
            in_comment = True
            comment_start = i
          comment_lines.append(line)
        i += 1
        continue

      if in_comment:
        in_comment = False
        if not stripped:
          comment_lines = []
          comment_start = -1
          i += 1
          continue

      if not stripped:
        i += 1
        continue

      # Strip inline comments for code parsing
      code_line = re.sub(r"//.*$", "", line)
      stripped_code = code_line.strip()

      # Check if line is a preprocessor / boilerplate to skip
      if re.match(
          r"^\s*(?:#\s*(?:(?!define\b)\w+|define\s+[A-Z0-9_]+_H_)|extern\s+|[{}]\s*$)",
          code_line,
      ):
        code_accumulator = []
        code_start = -1
        i += 1
        continue

      if current_struct_name is not None:
        # Check end of struct
        struct_end_match = re.match(r"^\s*\}\s*([a-zA-Z0-9_]*)\s*;", code_line)
        if struct_end_match:
          current_struct_name = None
          comment_lines = []
          comment_start = -1
          i += 1
          continue

        # Check member
        member_match = re.match(
            r"^\s*[^;]+?\b([a-zA-Z0-9_]+)\s*(?:\[[^\]]*\])?\s*;", code_line)
        if member_match:
          member_name = member_match.group(1)
          symbol_name = f"{current_struct_name}.{member_name}"
          self.symbols[symbol_name] = "member"
          self.decl_start[symbol_name] = i
          if comment_start != -1:
            self.comments[symbol_name] = (comment_start, i - 1)
          comment_lines = []
          comment_start = -1
          i += 1
          continue

      elif current_enum_name is not None:
        # Check end of enum
        enum_end_match = re.match(r"^\s*\}\s*([a-zA-Z0-9_]*)\s*;", code_line)
        if enum_end_match:
          current_enum_name = None
          comment_lines = []
          comment_start = -1
          i += 1
          continue

        # Check enum value
        value_match = re.match(r"^\s*([a-zA-Z0-9_]+)\b", code_line)
        if value_match:
          value_name = value_match.group(1)
          symbol_name = f"{current_enum_name}.{value_name}"
          self.symbols[symbol_name] = "member"
          self.decl_start[symbol_name] = i
          if comment_start != -1:
            self.comments[symbol_name] = (comment_start, i - 1)
          comment_lines = []
          comment_start = -1
          i += 1
          continue

      else:
        # Check start of struct
        struct_start_match = re.match(
            r"^\s*(?:typedef\s+)?struct\s+([a-zA-Z0-9_]+)", code_line)
        if struct_start_match and "{" in code_line:
          struct_name = struct_start_match.group(1)
          self.symbols[struct_name] = "struct"
          self.decl_start[struct_name] = i
          if comment_start != -1:
            self.comments[struct_name] = (comment_start, i - 1)
          current_struct_name = struct_name
          comment_lines = []
          comment_start = -1
          i += 1
          continue

        # Check start of enum
        enum_start_match = re.match(
            r"^\s*(?:typedef\s+)?enum\s+([a-zA-Z0-9_]+)", code_line)
        if enum_start_match and "{" in code_line:
          enum_name = enum_start_match.group(1)
          self.symbols[enum_name] = "typedef"
          self.decl_start[enum_name] = i
          if comment_start != -1:
            self.comments[enum_name] = (comment_start, i - 1)
          current_enum_name = enum_name
          comment_lines = []
          comment_start = -1
          i += 1
          continue

      # Regular code accumulation
      if code_start == -1:
        code_start = i
      code_accumulator.append(code_line)
      is_complete = False
      code_str = " ".join(code_accumulator).strip()

      if code_str.startswith("#define"):
        is_complete = True
      elif ";" in code_str:
        is_complete = True

      if is_complete:
        comment_end = code_start - 1 if comment_start != -1 else -1
        self.process_code_block(code_str, comment_start, comment_end,
                                code_start)
        code_accumulator = []
        code_start = -1
        comment_lines = []
        comment_start = -1

      i += 1

  def process_code_block(self, code_str, comment_start, comment_end,
                         code_start):
    macro_match = re.match(r"^#\s*define\s+([a-zA-Z0-9_]+)", code_str)
    if macro_match:
      name = macro_match.group(1)
      self.symbols[name] = "macro"
      self.decl_start[name] = code_start
      if comment_start != -1:
        self.comments[name] = (comment_start, comment_end)
      return

    if code_str.startswith("typedef"):
      name = None
      func_ptr_match = re.search(r"\(\*\s*([a-zA-Z0-9_]+)\s*\)\s*\(([^)]*)\)",
                                 code_str)
      if func_ptr_match:
        name = func_ptr_match.group(1)
        self.symbols[name] = "typedef"
        self.decl_start[name] = code_start
        params_str = func_ptr_match.group(2)
        self.params[name] = self.extract_params(params_str)
      else:
        s = code_str.rstrip(";").strip()
        words = re.findall(r"[a-zA-Z0-9_]+", s)
        if words:
          name = words[-1]
          self.symbols[name] = "typedef"
          self.decl_start[name] = code_start
      if name and comment_start != -1:
        self.comments[name] = (comment_start, comment_end)
      return

    if "SB_EXPORT" in code_str:
      name_match = re.search(r"([a-zA-Z0-9_]+)\s*\(", code_str)
      if name_match:
        name = name_match.group(1)
        self.symbols[name] = "function"
        self.decl_start[name] = code_start
        params_match = re.search(r"\(([^)]*)\)", code_str)
        if params_match:
          params_str = params_match.group(1)
          self.params[name] = self.extract_params(params_str)
        else:
          self.params[name] = []
        if comment_start != -1:
          self.comments[name] = (comment_start, comment_end)
      return

  def extract_params(self, params_str):
    if not params_str.strip() or params_str.strip() == "void":
      return []
    params = []
    parts = params_str.split(",")
    for part in parts:
      part = part.strip()
      words = re.findall(r"[a-zA-Z0-9_]+", part)
      if words:
        params.append(words[-1])
    return params


class DocBlock:
  pass


class ParagraphBlock(DocBlock):

  def __init__(self, text):
    self.text = text


class VerbatimBlock(DocBlock):

  def __init__(self, text):
    self.text = text


class HeadingBlock(DocBlock):

  def __init__(self, text, level):
    self.text = text
    self.level = level


class ParameterBlock(DocBlock):

  def __init__(self, name, desc):
    self.name = name
    self.desc = desc
    self.sub_bullets = []
    self.post_text = []


def parse_paragraph_into_elements(paragraph):
  matches = list(re.finditer(r"`([a-zA-Z_][a-zA-Z0-9_]*)`:\s*", paragraph))
  if not matches:
    return [("text", paragraph)]

  elements = []
  spans = [m.span() for m in matches]

  if spans[0][0] > 0:
    text = paragraph[0:spans[0][0]].strip()
    if text:
      elements.append(("text", text))

  for i in range(len(spans)):
    start_match, end_match = spans[i]
    end_desc = spans[i + 1][0] if i + 1 < len(spans) else len(paragraph)

    param_name = matches[i].group(1)
    param_desc = paragraph[end_match:end_desc].strip()
    elements.append(("parameter", param_name, param_desc))

  return elements


def parse_markdown_docs(lines):
  blocks = []
  current_block = None

  in_verbatim = False
  for line in lines:
    stripped = line.strip()

    if in_verbatim:
      if stripped.startswith("```"):
        # End of verbatim
        blocks.append(current_block)
        current_block = None
        in_verbatim = False
      else:
        if current_block.text:
          current_block.text += "\n" + line
        else:
          current_block.text = line
      continue

    # Not in verbatim
    if stripped.startswith("```"):
      # Start of verbatim
      if current_block:
        blocks.append(current_block)
      current_block = VerbatimBlock("")
      in_verbatim = True
      continue

    # Heading: e.g. "## Heading" or "# Heading"
    heading_match = re.match(r"^(#+)\s+(.*)$", stripped)
    if heading_match:
      if current_block:
        blocks.append(current_block)
      level = len(heading_match.group(1))
      current_block = HeadingBlock(heading_match.group(2).strip(), level)
      continue

    if not line or not stripped:
      if isinstance(current_block, ParagraphBlock):
        blocks.append(current_block)
        current_block = None
      continue

    # Sub-bullet: e.g. "    *   text"
    sub_bullet_match = re.match(r"^\s{4,}\*\s+(.*)$", line)
    if sub_bullet_match:
      content = sub_bullet_match.group(1)
      if isinstance(current_block, ParameterBlock):
        current_block.sub_bullets.append(content)
      else:
        if current_block:
          blocks.append(current_block)
        current_block = ParagraphBlock(line)
      continue

    # Continuation of sub-bullet: indented by 8 or more spaces
    sub_bullet_cont_match = re.match(r"^\s{8,}(.*)$", line)
    if sub_bullet_cont_match:
      content = sub_bullet_cont_match.group(1)
      if isinstance(current_block,
                    ParameterBlock) and current_block.sub_bullets:
        current_block.sub_bullets[-1] += " " + content
      elif isinstance(current_block, ParameterBlock):
        current_block.post_text.append(content)
      else:
        if current_block:
          blocks.append(current_block)
        current_block = ParagraphBlock(line)
      continue

    # Post text: indented by 4 to 7 spaces
    post_text_match = re.match(r"^\s{4,7}(?!\*)(.*)$", line)
    if post_text_match:
      content = post_text_match.group(1)
      if isinstance(current_block, ParameterBlock):
        current_block.post_text.append(content)
      else:
        if current_block:
          blocks.append(current_block)
        current_block = ParagraphBlock(line)
      continue

    # Main bullet: "* text" or "*   text"
    main_bullet_match = re.match(r"^\*\s+(.*)$", line)
    if main_bullet_match:
      content = main_bullet_match.group(1)
      if current_block:
        blocks.append(current_block)

      param_match = re.match(r"`([a-zA-Z_][a-zA-Z0-9_]*)`:\s*(.*)$", content)
      if param_match:
        name = param_match.group(1)
        desc = param_match.group(2)
        current_block = ParameterBlock(name, desc)
      else:
        current_block = ParameterBlock("", content)
      continue

    # Normal line
    if isinstance(current_block, ParagraphBlock):
      current_block.text += "\n" + line
    else:
      if current_block:
        blocks.append(current_block)
      current_block = ParagraphBlock(line)

  if current_block:
    blocks.append(current_block)

  # Normalize post_text and process merged parameters
  final_blocks = []
  for block in blocks:
    if isinstance(block, ParagraphBlock):
      elements = parse_paragraph_into_elements(block.text)
      for elem in elements:
        if elem[0] == "text":
          final_blocks.append(ParagraphBlock(elem[1]))
        elif elem[0] == "parameter":
          final_blocks.append(ParameterBlock(elem[1], elem[2]))
    elif isinstance(block, ParameterBlock):
      if block.post_text:
        block.post_text = " ".join(block.post_text)
      final_blocks.append(block)
    else:
      final_blocks.append(block)

  return final_blocks


def format_identifiers(text, pipe_symbols, header_symbols):

  def replace_match(match):
    word = match.group(1)
    if word in pipe_symbols:
      return f"|{word}|"
    elif word in header_symbols:
      return word
    else:
      return f"`{word}`"

  return re.sub(r"`([^`]+)`", replace_match, text)


def wrap_paragraph(text, prefix, max_width=80):
  text = re.sub(r"\s+", " ", text).strip()
  if not text:
    return []

  words = text.split(" ")
  lines = []
  current_line = []
  current_len = len(prefix)

  for word in words:
    word_len = len(word)
    space_len = 1 if current_line else 0
    if current_len + space_len + word_len > max_width:
      lines.append(prefix + " ".join(current_line))
      current_line = [word]
      current_len = len(prefix) + word_len
    else:
      current_line.append(word)
      current_len += space_len + word_len

  if current_line:
    lines.append(prefix + " ".join(current_line))

  return lines


def wrap_sub_bullet(text, first_prefix, next_prefix, max_width=80):
  text = re.sub(r"\s+", " ", text).strip()
  if not text:
    return []

  words = text.split(" ")
  lines = []
  current_line = []

  current_prefix = first_prefix
  current_len = len(current_prefix)

  for word in words:
    word_len = len(word)
    space_len = 1 if current_line else 0
    if current_len + space_len + word_len > max_width:
      lines.append(current_prefix + " ".join(current_line))
      current_line = [word]
      current_prefix = next_prefix
      current_len = len(current_prefix) + word_len
    else:
      current_line.append(word)
      current_len += space_len + word_len

  if current_line:
    lines.append(current_prefix + " ".join(current_line))

  return lines


def format_doc_blocks(blocks, pipe_symbols, header_symbols, indent=""):
  lines = []
  for idx, block in enumerate(blocks):
    if isinstance(block, ParagraphBlock):
      # Add blank comment line before paragraphs except the first one
      if idx > 0:
        lines.append(indent + "//")
      formatted_text = format_identifiers(block.text, pipe_symbols,
                                          header_symbols)
      lines.extend(wrap_paragraph(formatted_text, indent + "// "))

    elif isinstance(block, VerbatimBlock):
      # Verbatim blocks should preserve formatting
      if idx > 0:
        lines.append(indent + "//")
      for line in block.text.splitlines():
        stripped_line = line.rstrip()
        if stripped_line:
          lines.append(indent + "// " + stripped_line)
        else:
          lines.append(indent + "//")

    elif isinstance(block, HeadingBlock):
      if idx > 0:
        lines.append(indent + "//")
      hashes = "#" * max(1, block.level - 1)
      lines.append(f"{indent}// {hashes} {block.text}")

    elif isinstance(block, ParameterBlock):
      # Add blank comment line if transitioning from ParagraphBlock
      if idx > 0 and isinstance(blocks[idx - 1], ParagraphBlock):
        lines.append(indent + "//")

      # Format parameter description
      name_str = f"|{block.name}|: " if block.name else ""
      full_text = f"{name_str}{block.desc}"
      formatted_text = format_identifiers(full_text, pipe_symbols,
                                          header_symbols)
      lines.extend(
          wrap_sub_bullet(formatted_text, indent + "// * ", indent + "//   "))

      # Sub-bullets
      for sub in block.sub_bullets:
        formatted_sub = format_identifiers(sub, pipe_symbols, header_symbols)
        lines.extend(
            wrap_sub_bullet(formatted_sub, indent + "//   - ",
                            indent + "//     "))

      # Post text
      if block.post_text:
        formatted_post = format_identifiers(block.post_text, pipe_symbols,
                                            header_symbols)
        lines.extend(wrap_paragraph(formatted_post, indent + "//   "))

  return lines


def parse_markdown_members(struct_name, lines):
  content = "\n".join(lines)
  # Split by bullet points starting with "*   `"
  parts = re.split(r"(?:\n|^)\*\s+`", content)

  member_docs = {}
  for part in parts:
    part = part.strip()
    if not part:
      continue
    match = re.match(r"^([^`]+)`\n*(.*)$", part, re.DOTALL)
    if not match:
      continue
    decl_str = match.group(1).strip()
    desc_str = match.group(2)
    import textwrap
    desc_str = textwrap.dedent(desc_str).strip()

    words = re.findall(r"[a-zA-Z0-9_]+", decl_str)
    if not words:
      continue
    member_name = words[-1]

    desc_lines = desc_str.splitlines()
    blocks = parse_markdown_docs(desc_lines)
    member_docs[f"{struct_name}.{member_name}"] = blocks

  return member_docs


def parse_markdown_file(markdown_file):
  with open(markdown_file, "r", encoding="utf-8") as f:
    content = f.read()

  # Split into sections based on "### " but not "#### "
  parts = re.split(r"(?<!#)###\s+", content)

  # 1. Parse overview from first part
  first_part = parts[0]
  overview_lines = []
  for line in first_part.splitlines():
    stripped = line.strip()
    if stripped.startswith("Project:") or stripped.startswith("Book:"):
      continue
    # Skip main title "# Starboard Module Reference: ..."
    if stripped.startswith("# Starboard Module Reference:"):
      continue
    if stripped in [
        "## Enums", "## Functions", "## Typedefs", "## Macros", "## Structs",
        "## Structures"
    ]:
      break
    overview_lines.append(line)

  overview_blocks = parse_markdown_docs(overview_lines)

  # 2. Parse symbol documentations
  symbol_docs = {}
  for part in parts[1:]:
    lines = part.splitlines()
    if not lines:
      continue
    symbol_name = lines[0].strip()

    # Find the documentation lines
    doc_lines = []
    has_members = False
    members_lines = []
    for line in lines[1:]:
      stripped = line.strip()
      if stripped.startswith("## "):
        break
      if stripped.startswith("#### Definition") or stripped.startswith(
          "#### Declaration"):
        break
      if stripped.startswith("#### Members") or stripped.startswith(
          "#### Values"):
        has_members = True
        continue
      if has_members:
        members_lines.append(line)
      else:
        doc_lines.append(line)

    blocks = parse_markdown_docs(doc_lines)
    symbol_docs[symbol_name] = blocks

    if has_members:
      member_docs = parse_markdown_members(symbol_name, members_lines)
      symbol_docs.update(member_docs)

  return overview_blocks, symbol_docs


def main():
  parser = argparse.ArgumentParser(
      description="Backport markdown documentation to header comments.")
  parser.add_argument("markdown_file", help="Path to input markdown file")
  parser.add_argument("header_file", help="Path to target C/C++ header file")
  args = parser.parse_args()

  # Read header file
  with open(args.header_file, "r", encoding="utf-8") as f:
    header_lines = f.readlines()

  # Parse header file
  header_parser = HeaderParser(header_lines)
  header_parser.parse()

  # Parse markdown file
  overview_blocks, symbol_docs = parse_markdown_file(args.markdown_file)

  # Construct symbol lists for formatting
  header_symbols = set(header_parser.symbols.keys())
  pipe_symbols = set()

  # Extract parameters from header parser
  for name, params in header_parser.params.items():
    pipe_symbols.update(params)

  # Extract all header symbols and unqualified member names to pipe_symbols
  for sym_name in header_symbols:
    if "." in sym_name:
      pipe_symbols.add(sym_name.split(".")[-1])
    else:
      pipe_symbols.add(sym_name)

  # Add hardcoded media types
  pipe_symbols.update(["int16", "float32"])

  replacements = []

  # 1. Update Overview description
  if header_parser.overview_comment and overview_blocks:
    start_idx, end_idx = header_parser.overview_comment
    original_first_line = header_lines[start_idx].strip()
    overview_lines = [
        original_first_line,
        "//",
    ]
    formatted_overview = format_doc_blocks(overview_blocks, pipe_symbols,
                                           header_symbols, "")
    overview_lines.extend(formatted_overview)
    replacements.append((start_idx, end_idx, overview_lines))

  # 2. Update Symbol comments
  for symbol_name, blocks in symbol_docs.items():
    if symbol_name not in header_symbols:
      continue

    # Extract indentation of the declaration line
    decl_start = header_parser.decl_start[symbol_name]
    decl_line = header_lines[decl_start]
    indent_match = re.match(r"^(\s*)", decl_line)
    indent = indent_match.group(1) if indent_match else ""

    # Format the documentation blocks preserving indentation
    formatted_comment = format_doc_blocks(blocks, pipe_symbols, header_symbols,
                                          indent)

    if symbol_name in header_parser.comments:
      start_idx, end_idx = header_parser.comments[symbol_name]
      replacements.append((start_idx, end_idx, formatted_comment))
    else:
      # If symbol had no comment, insert before declaration
      # Ensure there is a blank comment line or newline?
      # Usually just insert the comment lines.
      replacements.append((decl_start, decl_start - 1, formatted_comment))

  # Apply replacements in reverse order
  updated_lines = apply_replacements(header_lines, replacements)

  # Write back to header file
  with open(args.header_file, "w", encoding="utf-8") as f:
    f.writelines(updated_lines)

  print(f"Successfully backported documentation to {args.header_file}")


def apply_replacements(lines, replacements):
  replacements.sort(key=lambda x: x[0], reverse=True)
  new_lines = list(lines)
  for start_idx, end_idx, repl_lines in replacements:
    new_lines[start_idx:end_idx + 1] = [line + "\n" for line in repl_lines]
  return new_lines


if __name__ == "__main__":
  main()
