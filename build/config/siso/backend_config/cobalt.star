# -*- bazel-starlark -*-
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso backend config for Cobalt.

This file was forked from:
//build/config/siso/backend_config/google.star with only the default
worker pool to match Cobalt's RBE instance.
"""

load("@builtin//struct.star", "module")

def __platform_properties(ctx):
    container_image = "docker://gcr.io/chops-public-images-prod/rbe/siso-chromium/linux@sha256:ef35d347f4a4a2d32b76fd908e66e96f59bf8ba7379fd5626548244c45343b2b"
    return {
        "default": {
            "OSFamily": "Linux",
            "container-image": container_image,
            "label:action_default": "1",
        },
        # TODO: b/585221756 - this is copied and customized from google.star,
        # but adjusted to always use default machines. It is NOT actually used,
        # however: all actions in .star files that require "large" in
        # "platform_ref" only run the actions remotely when "googlechrome" is
        # set in the configuration (it is done in google.star, but not here,
        # see the main config.star for an explanation).
        #
        # This is set here only because starting with the 150.7871 rebase siso
        # fails if this entry is not set.
        #
        # ---
        # From google.star:
        # Large workers are usually used for Python actions like generate bindings, mojo generators etc
        # They can run on Linux workers.
        "large": {
            "OSFamily": "Linux",
            "container-image": container_image,
            # From google.star:
            #
            # As of Jul 2023, the action_large pool uses n2-highmem-8 with 200GB of pd-ssd.
            # The pool is intended for the following actions.
            #  - slow actions that can benefit from multi-cores and/or faster disk I/O. e.g. link, mojo, generate bindings etc.
            #  - actions that fail for OOM.
            # "label:action_large": "1",

            # Cobalt customization.
            "label:action_default": "1",
        },
    }

backend = module(
    "backend",
    platform_properties = __platform_properties,
)
