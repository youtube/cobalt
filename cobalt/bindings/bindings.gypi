# Copyright 2014 Google Inc. All Rights Reserved.
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

# This file is intended to be included into another .gyp file that will have
# a number of variables defined, which will drive the bindings generation
# targets below. A .gyp file that includes this should depend on the 'bindings'
# target which will be defined in this gyp file.
#
# Input variables:
#   source_idl_files: All IDL files for which a bindings wrapper should be
#       created.
#   generated_header_idl_files: IDL files for IDL dictionaries and IDL enums. A
#       header will be generated for these that can be #included in
#   dependency_idl_files: IDL files that are dependencies of other IDLs, but no
#       bindings wrapper will be created. For example, partial interfaces and
#       the right-hand-side of implements statements.
#
# Other variables, such as bindings_defines, can be set in the including .gyp
# file to affect how the bindings are built. See comments in the variables
# section below.
#
# Output:
#   generated_bindings: A gyp target that will generate all bindings source
#       files. At most one target can declare generated_bindings as a
#       dependency. The target that depends on generated_bindings will compile
#       the generated sources, since generated_bindings will export the required
#       gyp variables to its dependent.
#
# Basic flow of data:
#   [All Static files] -> GlobalObjectsPickle
#     - GlobalObjectsPickle is a python pickle of all interfaces with the Global
#       or PrimaryGlobal extended attribute
#   GlobalObjectsPickle -> GlobalConstructors.idl
#     - GlobalConstructors.idl defines a partial interface that exposes all of
#       the interfaces Interface Objects, also known as constructors.
#   [All Static and Generated IDL files] -> InterfacesInfoComponent.pickle
#     - Preprocessed interface data from the IDLs
#     - In Blink, this is per-component, but we don't have multiple components
#       in Cobalt at this point.
#   [InterfacesInfoComponent.pickle] -> InterfacesInfoOverall.pickle
#     - Pickles for all components are combined into a global pickle that covers
#       all interfaces across all modules.
#   InterfacesInfoOverall.pickle -> [Bindings for all interfaces]
#
# This is built and based on the flow for Blink's IDL parsing and code
# generation pipeline:
# http://www.chromium.org/developers/design-documents/idl-build
{
  'variables': {
    # Get the engine variables and blink's variables and pull them out to this
    # 'variables' scope so we can use them in this file.
    'includes': [
      '../script/engine_variables.gypi',
      '../../third_party/blink/Source/bindings/scripts/scripts.gypi',
    ],
    'variables': {
      # Specify a default component for generated window IDL. This should be
      # removed when the concept of 'components' in the blink IDL parsing scripts
      # is refactored out, since it doesn't really apply to Cobalt.
      'window_component%': 'dom',
    },

    # Define these variables in the including .gyp file and their lists will get
    # merged in, giving some control to how the bindings are built.
    'bindings_defines': ['<@(engine_defines)'],
    'bindings_dependencies': ['<@(engine_dependencies)'],
    'bindings_include_dirs': ['<@(engine_include_dirs)'],
    'bindings_templates_dir': '<(engine_templates_dir)',
    'idl_compiler_script': '<(engine_idl_compiler)',
    'conversion_header_generator_script': '<(engine_conversion_header_generator_script)',

    # Templates that are shared by the code generation for multiple engines.
    'shared_template_files': [
      'templates/dictionary.h.template',
      'templates/interface-base.cc.template',
      'templates/interface-base.h.template',
      'templates/callback-interface-base.cc.template',
      'templates/callback-interface-base.h.template',
    ],

    # The following lists of IDL files should be set in the including .gyp file.

    # Bindings for the interfaces in this list will be generated, and there must
    # be an implementation declared in a header that lives in the same
    # directory of each IDL.
    'source_idl_files': [],

    # For each IDL file in this list, a header file that can be #included in
    # Cobalt will be generated, as well as a .cc file that implements the
    # conversion to/from a JS value.
    'generated_header_idl_files': [],

    # Partial interfaces and the right-side of "implements"
    # Code will not get generated for these interfaces; they are used to add
    # functionality to other interfaces.
    'dependency_idl_files': [],

    # Bindings for these will not be generated, but attempts to access their
    # Constructors will be an event in the unsupported property mechanism.
    'unsupported_interface_idl_files': [],

    'window_component%': '<(window_component)',

    'prefix': '<(generated_bindings_prefix)',

    # The location of blink's bindings scripts, such as idl_compiler.py
    'bindings_scripts_dir': '<(bindings_scripts_dir)',

    # PLY, Web IDL parser, and Blink IDL parser sources
    'idl_lexer_parser_files': ['<@(idl_lexer_parser_files)'],

    # Jinja bytecode cache, PLY lex/parse table cache
    'idl_cache_files': ['<@(idl_cache_files)'],

    # Jinja sources
    'jinja_module_files': ['<@(jinja_module_files)'],

    # Cobalt's Jinja templates
    'code_generator_template_files': [
      '<@(engine_template_files)',
      '<@(shared_template_files)'
    ],

    # Dependencies of the bindings generation that are not captured as inputs
    'bindings_extra_inputs': [
      '<@(idl_lexer_parser_files)',
      '<@(idl_cache_files)',
      '<@(idl_compiler_files)',
      '<(DEPTH)/cobalt/bindings/code_generator_cobalt.py',
      '<(DEPTH)/cobalt/bindings/expression_generator.py',
      '<(DEPTH)/cobalt/bindings/contexts.py',
      '<(DEPTH)/cobalt/bindings/idl_compiler_cobalt.py',
      '<(DEPTH)/cobalt/bindings/generate_conversion_header.py',
      '<(DEPTH)/cobalt/bindings/name_conversion.py',
      '<(DEPTH)/cobalt/bindings/overload_context.py',
      '<@(code_generator_template_files)',
      '<@(engine_bindings_scripts)',
    ],

    # Caches and intermediate structures.
    'bindings_scripts_output_dir': '<(bindings_output_dir)/scripts',

    # Directory containing generated IDL files.
    'generated_idls_output_dir': '<(bindings_output_dir)/idl',

    # Base directory into which generated bindings source files will be
    # generated. Directory structure will mirror the directory structure
    # that the .idl files came from.
    'generated_source_output_dir': '<(bindings_output_dir)/source',

    # File containing a whitelist of Extended Attributes that the code generation
    # pipeline understands
    'extended_attributes_file': 'IDLExtendedAttributes.txt',

    # Blink interface info is calculated in two stages. First at a per-component level
    # (in Blink this is core or modules) and then these are combined. While Cobalt
    # currently does not and may not need to distinguish between components, we adhere to
    # Blink's process for simplicity.
    'component_info_pickle':
        '<(bindings_scripts_output_dir)/component_info.pickle',
    'interfaces_info_individual_pickle':
        '<(bindings_scripts_output_dir)/interfaces_info_individual.pickle',
    'interfaces_info_combined_pickle':
        '<(bindings_scripts_output_dir)/interfaces_info_overall.pickle',

    # All source files that will be generated from the source IDL files.
    'generated_sources':
        ['<!@pymod_do_main(cobalt.build.path_conversion -s'
         ' --output_directory <(generated_source_output_dir)'
         ' --output_extension cc --output_prefix <(prefix)_'
         ' --base_directory <(DEPTH)'
         ' <@(source_idl_files))'],

    # .cc files that implement conversion functions to/from a JS value for
    # generated types (enums and dictionaries).
    'generated_type_conversions':
        ['<!@pymod_do_main(cobalt.build.path_conversion -s'
         ' --output_directory <(generated_source_output_dir)'
         ' --output_extension cc --output_prefix <(prefix)_'
         ' --base_directory <(DEPTH)'
         ' <@(generated_header_idl_files))'],

    # Generated IDL file that will define all the constructors that should be
    # on the Window object
    'global_constructors_generated_idl_file':
        '<(generated_idls_output_dir)/<(window_component)/window_constructors.idl',

    # Dummy header file which is generated because the idl compiler assumes
    # there is a header for each IDL.
    'global_constructors_generated_header_file':
        '<(generated_idls_output_dir)/<(window_component)/window_constructors.h',

    # Generated header file that contains forward declarations for converting
    # to/from JS value and generated types.
    'generated_type_conversion_header_file':
        '<(generated_source_output_dir)/<(prefix)_gen_type_conversion.h',
  },

  'targets': [
    {
      # Create a target that depends on this target to compile the generated
      # source files. There should be only one such target.
      # Based on blink/Source/bindings/core/v8/generated.gyp:bindings_core_v8_generated_individual
      'target_name': 'generated_bindings',
      'type': 'none',
      # The 'binding' rule generates .h files, so mark as hard_dependency, per:
      # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
      'hard_dependency': 1,
      'dependencies': [
        'cached_jinja_templates',
        'cached_lex_yacc_tables',
        'generated_types',
        'generated_type_conversion',
        'global_constructors_idls',
        'interfaces_info_overall',
        '<@(bindings_dependencies)',
      ],
      'export_dependent_settings': [
        '<@(bindings_dependencies)',
      ],
      'sources': [
        '<@(source_idl_files)',
      ],
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          # Script source files, etc.
          '<@(bindings_extra_inputs)',

          # This file is global, so if it changes all files are rebuilt.
          # However, this file will only change when dependency structure
          # changes, so shouldn't change too much.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<(interfaces_info_combined_pickle)',

          # Similarly, all files are rebuilt if a partial interface or
          # right side of 'implements' changes.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<@(dependency_idl_files)',

          # Also add as a dependency the set of unsupported IDL files.
          '<@(unsupported_interface_idl_files)',

          # The generated constructors IDL is also a partial interface, and
          # we need to rebuild if it is modified.
          '<(global_constructors_generated_idl_file)',

          # The whitelist of what extended attributes we support. If an attribute
          # not in this list is encountered, it will cause an error in the
          # pipeline.
          '<(extended_attributes_file)',

          # Forward declarations of conversion functions for generated types.
          '<(generated_type_conversion_header_file)',
        ],
        'outputs': [
          '<!@pymod_do_main(cobalt.build.path_conversion -s -p <(prefix)_ '
              '-e cc -d <(generated_source_output_dir) -b <(DEPTH) <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT))',
          '<!@pymod_do_main(cobalt.build.path_conversion -s -p <(prefix)_ '
              '-e h -d <(generated_source_output_dir) -b <(DEPTH) <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT))'
        ],
        'action': [
          'python',
          '<(idl_compiler_script)',
          '--cache-dir',
          '<(bindings_scripts_output_dir)',
          '--output-dir',
          '<(generated_source_output_dir)',
          '--interfaces-info',
          '<(interfaces_info_combined_pickle)',
          '--component-info',
          '<(component_info_pickle)',
          '--extended-attributes',
          '<(extended_attributes_file)',
          '<(RULE_INPUT_PATH)',
        ],
        'message': 'Generating binding from <(RULE_INPUT_PATH)',
      }],
      # The target that depends on this will build the bindings source.
      'direct_dependent_settings': {
        'defines': [ '<@(bindings_defines)' ],
        'include_dirs': [
          '<@(bindings_include_dirs)',
          '<(generated_source_output_dir)',
        ],
        'sources': [
          '<@(generated_sources)',
          '<@(generated_type_conversions)',
        ],
        'defines': [ '<@(bindings_defines)'],
      }
    },

    {
      # Based on the generated_bindings target above. Main difference is that
      # this produces two .h files, and takes the dictionary idl files as input.
      'target_name': 'generated_types',
      'type': 'none',
      'hard_dependency': 1,
      'dependencies': [
        'cached_jinja_templates',
        'cached_lex_yacc_tables',
        'global_constructors_idls',
        'interfaces_info_overall',
      ],
      'sources': [
        '<@(generated_header_idl_files)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(generated_source_output_dir)',
        ]
      },
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          # Script source files, etc.
          '<@(bindings_extra_inputs)',

          # This file is global, so if it changes all files are rebuilt.
          # However, this file will only change when dependency structure
          # changes, so shouldn't change too much.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<(interfaces_info_combined_pickle)',

          # Similarly, all files are rebuilt if a partial interface or
          # right side of 'implements' changes.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<@(dependency_idl_files)',

          # Also add as a dependency the set of unsupported IDL files.
          '<@(unsupported_interface_idl_files)',

          # The generated constructors IDL is also a partial interface, and
          # we need to rebuild if it is modified.
          '<(global_constructors_generated_idl_file)',

          # The whitelist of what extended attributes we support. If an attribute
          # not in this list is encountered, it will cause an error in the
          # pipeline.
          '<(extended_attributes_file)',
        ],
        'outputs': [
          '<!@pymod_do_main(cobalt.build.path_conversion -s '
              '-e h -d <(generated_source_output_dir) -b <(DEPTH) <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT))',
          '<!@pymod_do_main(cobalt.build.path_conversion -s -p <(prefix)_ '
              '-e cc -d <(generated_source_output_dir) -b <(DEPTH) <(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT))'
        ],
        'action': [
          'python',
          '<(idl_compiler_script)',
          '--cache-dir',
          '<(bindings_scripts_output_dir)',
          '--output-dir',
          '<(generated_source_output_dir)',
          '--interfaces-info',
          '<(interfaces_info_combined_pickle)',
          '--component-info',
          '<(component_info_pickle)',
          '--extended-attributes',
          '<(extended_attributes_file)',
          '<(RULE_INPUT_PATH)',
        ],
        'message': 'Generating dictionary from <(RULE_INPUT_PATH)',
      }],
    },
    {
      # Create a target that depends on this target to compile the generated
      # source files. There should be only one such target.
      # Based on blink/Source/bindings/core/v8/generated.gyp:bindings_core_v8_generated_individual
      'target_name': 'generated_type_conversion',
      'type': 'none',
      # The 'binding' rule generates .h files, so mark as hard_dependency, per:
      # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
      'hard_dependency': 1,
      'dependencies': [
        'cached_jinja_templates',
        'cached_lex_yacc_tables',
        'global_constructors_idls',
        'interfaces_info_overall',
        '<@(bindings_dependencies)',
      ],
      'export_dependent_settings': [
        '<@(bindings_dependencies)',
      ],
      'sources': [
        '<@(source_idl_files)',
        '<@(generated_header_idl_files)'
      ],
      'actions': [{
        'action_name': 'generate_type_conversion_header',
        'inputs': [
          # Script source files, etc.
          '<@(bindings_extra_inputs)',

          # This file is global, so if it changes all files are rebuilt.
          # However, this file will only change when dependency structure
          # changes, so shouldn't change too much.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<(interfaces_info_combined_pickle)',

          # Similarly, all files are rebuilt if a partial interface or
          # right side of 'implements' changes.
          # See blink/Source/bindings/core/v8/generated.gyp for more info
          '<@(dependency_idl_files)',

          # Also add as a dependency the set of unsupported IDL files.
          '<@(unsupported_interface_idl_files)',

          # The generated constructors IDL is also a partial interface, and
          # we need to rebuild if it is modified.
          '<(global_constructors_generated_idl_file)',

          # The whitelist of what extended attributes we support. If an attribute
          # not in this list is encountered, it will cause an error in the
          # pipeline.
          '<(extended_attributes_file)',
        ],
        'outputs': [
          '<(generated_type_conversion_header_file)',
        ],
        'action': [
          'python',
          '<(conversion_header_generator_script)',
          '--cache-dir',
          '<(bindings_scripts_output_dir)',
          '--output-dir',
          '<(generated_source_output_dir)',
          '--interfaces-info',
          '<(interfaces_info_combined_pickle)',
          '--component-info',
          '<(component_info_pickle)',
        ],
        'message': 'Generating generated type conversion header',
      }],
    },

    {
      # Based on blink/Source/bindings/core/generated.gyp:core_global_objects
      'target_name': 'global_objects',
      'variables': {
        'idl_files': [
          '<@(source_idl_files)', '<@(generated_header_idl_files)'
        ],
        'output_file': '<(bindings_scripts_output_dir)/GlobalObjects.pickle',
      },
      'includes': ['../../third_party/blink/Source/bindings/scripts/global_objects.gypi'],
    },

    {
      # Based on blink/Source/bindings/core/generated.gyp:core_global_constructors_idls
      'target_name': 'global_constructors_idls',
      'dependencies': [
        'global_objects',
      ],
      'variables': {
        'idl_files': [
          '<@(source_idl_files)',
          '<@(unsupported_interface_idl_files)',
        ],
        'global_objects_file':
          '<(bindings_scripts_output_dir)/GlobalObjects.pickle',
        'global_names_idl_files': [
          'Window',
          '<(global_constructors_generated_idl_file)',
        ],
        'outputs': [
          '<(global_constructors_generated_idl_file)',
          '<(global_constructors_generated_header_file)',
        ],
      },
      'includes': ['../../third_party/blink/Source/bindings/scripts/global_constructors.gypi'],
    },

    {
      # Based on blink/Source/bindings/core/generated.gyp:interfaces_info_individual_core
      'target_name': 'interfaces_info_individual',
      'dependencies': [
        'cached_lex_yacc_tables',
        'global_constructors_idls',
      ],
      'variables': {
        'static_idl_files': [
          '<@(source_idl_files)',
          '<@(generated_header_idl_files)',
          '<@(dependency_idl_files)',
          '<@(unsupported_interface_idl_files)'],
        'generated_idl_files': ['<(global_constructors_generated_idl_file)'],
        'component_info_file':'<(component_info_pickle)',
        'interfaces_info_file': '<(interfaces_info_individual_pickle)',
        'output_file': '<(interfaces_info_file)',
        'cache_directory': '<(bindings_scripts_output_dir)',
        'root_directory': '../..',
      },
      'includes': ['../../third_party/blink/Source/bindings/scripts/interfaces_info_individual.gypi'],
    },

    {
      # Based on blink/Source/bindings/modules/generated.gyp:interfaces_info_overall
      'target_name': 'interfaces_info_overall',
      'dependencies': [
          'interfaces_info_individual',
      ],
      'variables': {
        'input_files': ['<(interfaces_info_individual_pickle)'],
        'output_file': '<(interfaces_info_combined_pickle)',
      },
      'includes': ['../../third_party/blink/Source/bindings/scripts/interfaces_info_overall.gypi'],
    },

    {
      # Borrowed from blink/Source/bindings/scripts/scripts.gyp:cached_lex_yacc_tables
      'target_name': 'cached_lex_yacc_tables',
      'type': 'none',
      'actions': [{
        'action_name': 'cache_lex_yacc_tables',
        'inputs': [
          '<@(idl_lexer_parser_files)',
        ],
        'outputs': [
          '<(bindings_scripts_output_dir)/lextab.py',
          '<(bindings_scripts_output_dir)/parsetab.pickle',
        ],
        'action': [
          'python',
          '<(bindings_scripts_dir)/blink_idl_parser.py',
          '<(bindings_scripts_output_dir)',
        ],
        'message': 'Caching PLY lex & yacc lex/parse tables',
      }],
    },
    {
      # Borrowed from blink/Source/bindings/scripts/scripts.gyp:cached_jinja_templates
      'target_name': 'cached_jinja_templates',
      'type': 'none',
      'actions': [{
        'action_name': 'cache_jinja_templates',
        'inputs': [
          '<@(jinja_module_files)',
          '<(DEPTH)/cobalt/bindings/code_generator_cobalt.py',
          '<(DEPTH)/cobalt/bindings/path_generator.py',
          '<@(code_generator_template_files)',
        ],
        'outputs': [
          '<(bindings_scripts_output_dir)/cached_jinja_templates.stamp',  # Dummy to track dependency
        ],
        'action': [
          'python',
          '<(DEPTH)/cobalt/bindings/code_generator_cobalt.py',
          '<(bindings_scripts_output_dir)',
          '<(bindings_templates_dir)',
          '<(bindings_scripts_output_dir)/cached_jinja_templates.stamp',
        ],
        'message': 'Caching bytecode of Jinja templates',
      }],
    },
  ],
}
