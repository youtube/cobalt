# vim:ts=4 sw=4 expandtab softtabstop=4
from jsonmerge.exceptions import HeadInstanceError, \
                                 BaseInstanceError, \
                                 SchemaError
import jsonschema
import re

class Strategy(object):
    """Base class for merge strategies.
    """

    def merge(self, walk, base, head, schema, meta, **kwargs):
        """Merge head instance into base.

        walk -- WalkInstance object for the current context.
        base -- Value being merged into.
        head -- Value being merged.
        schema -- Schema used for merging.
        meta -- Meta data, as passed to the Merger.merge() method.
        kwargs -- Dict with any extra options given in the 'mergeOptions'
        keyword

        Specific merge strategies should override this method to implement
        their behavior.

        The function should return the object resulting from the merge.

        Recursion into the next level, if necessary, is achieved by calling
        walk.descend() method.
        """
        raise NotImplemented

    def get_schema(self, walk, schema, meta, **kwargs):
        """Return the schema for the merged document.

        walk -- WalkSchema object for the current context.
        schema -- Original document schema.
        meta -- Schema for the meta data, as passed to the Merger.get_schema()
        method.
        kwargs -- Dict with any extra options given in the 'mergeOptions'
        keyword.

        Specific merge strategies should override this method to modify the
        document schema depending on the behavior of the merge() method.

        The function should return the schema for the object resulting from the
        merge.

        Recursion into the next level, if necessary, is achieved by calling
        walk.descend() method.

        Implementations should take care that all external schema references
        are resolved in the returned schema. This can be achieved by calling
        walk.resolve_refs() method.
        """
        raise NotImplemented

class Overwrite(Strategy):
    def merge(self, walk, base, head, schema, meta, **kwargs):
        return head

    def get_schema(self, walk, schema, meta, **kwargs):
        return walk.resolve_refs(schema)

class Version(Strategy):
    def merge(self, walk, base, head, schema, meta, limit=None, unique=None, ignoreDups=True, **kwargs):

        # backwards compatibility
        if unique is False:
            ignoreDups = False

        if base is None:
            base = []
        else:
            base = list(base)

        if not ignoreDups or not base or base[-1]['value'] != head:
            base.append(walk.add_meta(head, meta))
            if limit is not None:
                base = base[-limit:]

        return base

    def get_schema(self, walk, schema, meta, limit=None, **kwargs):

        if meta is not None:
            item = dict(meta)
        else:
            item = {}

        if 'properties' not in item:
            item['properties'] = {}

        item['properties']['value'] = walk.resolve_refs(schema)

        rv = {  "type": "array",
                "items": item }

        if limit is not None:
            rv['maxItems'] = limit

        return rv

class Append(Strategy):
    def merge(self, walk, base, head, schema, meta, **kwargs):
        if not walk.is_type(head, "array"):
            raise HeadInstanceError("Head for an 'append' merge strategy is not an array")

        if base is None:
            base = []
        else:
            if not walk.is_type(base, "array"):
                raise BaseInstanceError("Base for an 'append' merge strategy is not an array")

            base = list(base)

        base += head
        return base

    def get_schema(self, walk, schema, meta, **kwargs):
        schema.pop('maxItems', None)
        schema.pop('uniqueItems', None)

        return walk.resolve_refs(schema)


class ArrayMergeById(Strategy):
    def merge(self, walk, base, head, schema, meta, idRef="id", ignoreId=None, **kwargs):
        if not walk.is_type(head, "array"):
            raise HeadInstanceError("Head for an 'arrayMergeById' merge strategy is not an array")  # nopep8

        if base is None:
            base = []
        else:
            if not walk.is_type(base, "array"):
                raise BaseInstanceError("Base for an 'arrayMergeById' merge strategy is not an array")  # nopep8
            base = list(base)

        subschema = None

        if schema:
            subschema = schema.get('items')

        if walk.is_type(subschema, "array"):
            raise SchemaError("'arrayMergeById' not supported when 'items' is an array")

        for head_item in head:

            try:
                head_key = walk.resolver.resolve_fragment(head_item, idRef)
            except jsonschema.RefResolutionError:
                # Do nothing if idRef field cannot be found.
                continue

            if head_key == ignoreId:
                continue

            key_count = 0
            for i, base_item in enumerate(base):
                base_key = walk.resolver.resolve_fragment(base_item, idRef)
                if base_key == head_key:
                    key_count += 1
                    # If there was a match, we replace with a merged item
                    base[i] = walk.descend(subschema, base_item, head_item, meta)
            if key_count == 0:
                # If there wasn't a match, we append a new object
                base.append(walk.descend(subschema, None, head_item, meta))
            if key_count > 1:
                raise BaseInstanceError("Id was not unique")

        return base

    def get_schema(self, walk, schema, meta, **kwargs):
        subschema = None
        if schema:
            subschema = schema.get('items')

        # Note we're discarding the walk.descend() result here. This is because
        # it would de-reference the $ref if the subschema is a reference - i.e.
        # in the result it would replace the reference with the copy of the
        # target.
        #
        # But we want to keep the $ref and do the walk.descend() only on the target of the reference.
        #
        # This seems to work, but is an ugly workaround. walk.descend() should
        # be fixed instead to not dereference $refs when not necessary.
        walk.descend(subschema, meta)
        return schema


class ObjectMerge(Strategy):
    def merge(self, walk, base, head, schema, meta, **kwargs):
        if not walk.is_type(head, "object"):
            raise HeadInstanceError("Head for an 'object' merge strategy is not an object")

        if base is None:
            base = {}
        else:
            if not walk.is_type(base, "object"):
                raise BaseInstanceError("Base for an 'object' merge strategy is not an object")

            base = dict(base)

        for k, v in head.items():

            subschema = None

            # get subschema for this element
            if schema is not None:
                p = schema.get('properties')
                if p is not None:
                    subschema = p.get(k)

                if subschema is None:
                    p = schema.get('patternProperties')
                    if p is not None:
                        for pattern, s in p.items():
                            if re.search(pattern, k):
                                subschema = s

                if subschema is None:
                    p = schema.get('additionalProperties')
                    if p is not None:
                        subschema = p.get(k)

            base[k] = walk.descend(subschema, base.get(k), v, meta)

        return base

    def get_schema(self, walk, schema, meta, **kwargs):

        for forbidden in ("oneOf", "allOf", "anyOf"):
            if forbidden in schema:
                raise SchemaError("Type ambiguous schema")

        schema2 = dict(schema)

        def descend_keyword(keyword):
            p = schema.get(keyword)
            if p is not None:
                for k, v in p.items():
                    schema2[keyword][k] = walk.descend(v, meta)

        descend_keyword("properties")
        descend_keyword("patternProperties")
        descend_keyword("additionalProperties")

        return schema2
