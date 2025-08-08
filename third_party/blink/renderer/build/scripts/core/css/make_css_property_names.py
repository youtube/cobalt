#!/usr/bin/env python

from core.css import css_properties
import gperf
import json5_generator
import template_expander


class CSSPropertyNamesWriter(json5_generator.Writer):
    class_name = "CSSPropertyNames"
    file_basename = "css_property_names"

    def __init__(self, json5_file_path, output_dir):
        super(CSSPropertyNamesWriter, self).__init__(json5_file_path,
                                                     output_dir)
        self._outputs = {
            (self.file_basename + ".h"): self.generate_header,
            (self.file_basename + ".cc"): self.generate_implementation,
        }
        self._css_properties = css_properties.CSSProperties(json5_file_path)

    def _enum_declaration(self, property_):
        return f"    {property_.enum_key} = {property_.enum_value},"

    def _array_item(self, property_):
        return f"    CSSPropertyID::{property_.enum_key},"

    @template_expander.use_jinja(
        'core/css/templates/css_property_names.h.tmpl')
    def generate_header(self):
        return {
            'alias_offset':
            self._css_properties.alias_offset,
            'class_name':
            self.class_name,
            'property_enums':
            "\n".join(
                map(self._enum_declaration,
                    self._css_properties.properties_including_aliases)),
            'property_aliases':
            "\n".join(map(self._array_item, self._css_properties.aliases)),
            'computable_properties':
            "\n".join(map(self._array_item, self._css_properties.computable)),
            'first_property_id':
            self._css_properties.first_property_id,
            'properties_count':
            len(self._css_properties.properties_including_aliases),
            'last_property_id':
            self._css_properties.last_property_id,
            'last_unresolved_property_id':
            self._css_properties.last_unresolved_property_id,
            'last_high_priority_property_id':
            self._css_properties.last_high_priority_property_id,
            'property_id_bit_length':
            self._css_properties.property_id_bit_length,
            'max_name_length':
            max(map(len, self._css_properties.properties_by_id)),
            'max_shorthand_expansion':
            self._css_properties.max_shorthand_expansion,
        }

    @gperf.use_jinja_gperf_template(
        'core/css/templates/css_property_names.cc.tmpl',
        ['-Q', 'CSSPropStringPool'])
    def generate_implementation(self):
        enum_value_to_name = {}
        for property_ in self._css_properties.properties_including_aliases:
            enum_value_to_name[property_.enum_value] = \
                property_.name.original
        property_offsets = []
        property_names = []
        current_offset = 0
        for enum_value in range(self._css_properties.first_property_id,
                                max(enum_value_to_name) + 1):
            property_offsets.append(current_offset)
            if enum_value in enum_value_to_name:
                name = enum_value_to_name[enum_value]
                property_names.append(name)
                current_offset += len(name) + 1

        # This is the input to gperf.
        css_name_and_enum_pairs = [
            (property_.name.original,
             f'static_cast<int>(CSSPropertyID::{property_.enum_key})')
            for property_ in self._css_properties.gperf_properties
        ]

        css_sample_id_pairs = [
            (property_.enum_key, property_.css_sample_id)
            for property_ in self._css_properties.properties_including_aliases
        ]

        return {
            'aliases':
            self._css_properties.aliases,
            'alias_offset':
            self._css_properties.alias_offset,
            'class_name':
            'CSSPropertyNames',
            'file_basename':
            self.file_basename,
            'properties_with_alternatives':
            self._css_properties.properties_with_alternatives,
            'css_sample_id_pairs':
            css_sample_id_pairs,
            'property_names':
            property_names,
            'property_offsets':
            property_offsets,
            'property_to_enum_map':
            '\n'.join('%s, %s' % property_
                      for property_ in css_name_and_enum_pairs),
            'gperf_path':
            self.gperf_path,
        }


if __name__ == "__main__":
    json5_generator.Maker(CSSPropertyNamesWriter).main()
