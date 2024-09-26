# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json5_generator
import make_runtime_features
import template_expander


# We want exactly the same parsing as RuntimeFeatureWriter
# but generate different files.
class OriginTrialsWriter(make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = 'origin_trial_feature'

    def __init__(self, json5_file_path, output_dir):
        super(OriginTrialsWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.h'): self.generate_header,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.h.tmpl')
    def generate_header(self):
        return {
            'features': self._features,
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }


if __name__ == '__main__':
    json5_generator.Maker(OriginTrialsWriter).main()
