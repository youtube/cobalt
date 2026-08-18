#!/usr/bin/env python3
# pylint: disable=duplicate-code,wrong-import-position
"""Vertex AI Reasoning Engine package for Cobalt Chromium rebase."""

import os
import sys

# Ensure reasoning_engine package directory is in sys.path
_PKG_DIR = os.path.dirname(os.path.abspath(__file__))
if _PKG_DIR not in sys.path:
  sys.path.insert(0, _PKG_DIR)

from engine import CobaltReasoningEngine, load_skill

__all__ = ["CobaltReasoningEngine", "load_skill"]
