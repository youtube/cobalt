#!/usr/bin/env python3
"""Checks UPSTREAM.yaml files."""

import sys
import strictyaml

# A schema for specifying the source of an imported tree.
_schema = strictyaml.Map({

    # The URL of a Git repository containing the source revision.
    "git-url": strictyaml.Url(),

    # A tag (preferably) or commit ID of the fetchable source revision.
    "git-revision": strictyaml.Str(),

    # The Chromium version from which the tree was imported, or
    # a string indicating the tree is not in Chromium at all.
    # See https://chromium.org/developers/version-numbers.
    # Versions used should be public releases announced on
    # https://chromereleases.googleblog.com.
    "chromium-version": strictyaml.Enum([
        "80.0.3987.162",
        "not in chromium",
    ]),

    # The Chromium directory from which the tree is imported, or
    # the directory where gclient places it according to DEPS, or
    # a string indicating the tree is not in Chromium at all.
    "chromium-dir": strictyaml.Str(),
})

for arg in sys.argv[1:]:
  if arg.endswith("UPSTREAM.yaml"):
    with open(arg, encoding="utf-8") as f:
      try:
        strictyaml.load(f.read(), _schema, arg)
      except strictyaml.exceptions.YAMLValidationError as e:
        print(e)
        sys.exit(1)
