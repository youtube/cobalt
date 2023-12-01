#!/usr/bin/env python3

import sys


def _check_title(lines):
  if not lines or not lines[0].strip():
    return ["The first line should be a title."]  # skip the rest
  suggestions = []
  if len(lines) > 1 and lines[1].strip():
    suggestions.append(
        "Separate the title from the body with a blank line.\n" +
        "https://cbea.ms/git-commit/#separate")
  if len(lines[0]) > 50:
    suggestions.append(
        "Preferably limit the title to 50 characters.\n" +
        "https://cbea.ms/git-commit/#limit-50")
  if lines[0][0] != lines[0][0].capitalize():
    suggestions.append(
        "Preferably capitalize the title.\n" +
        "https://cbea.ms/git-commit/#capitalize")
  if lines[0].strip()[-1] == ".":
    suggestions.append(
        "Optionally don't end the title with a period.\n" +
        "https://cbea.ms/git-commit/#end")
  return suggestions


def _check_line_length(lines):
  for line in lines[1:]:
    if len(line) > 72:
      return [
          "Preferably wrap lines at 72 characters.\n" +
          "https://cbea.ms/git-commit/#wrap-72"]
  return []


with open(sys.argv[1]) as f:
  lines = [l.replace("\n", "") for l in f.readlines() if not l.startswith("#")]
  suggestions = _check_title(lines) + _check_line_length(lines)
  if suggestions:
    warning_text = (
        "This hook always passes to stay out of your way, " +
        "but this time it suggests:")
    print("\n\n".join([warning_text] + suggestions))
