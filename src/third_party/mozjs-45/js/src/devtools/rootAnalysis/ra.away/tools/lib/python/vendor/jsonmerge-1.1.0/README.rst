Merge a series of JSON documents
================================

This Python module allows you to merge a series of JSON documents into a
single one.

This problem often occurs for example when different authors fill in
different parts of a common document and you need to construct a document
that includes contributions from all the authors. It also helps when
dealing with consecutive versions of a document where different fields get
updated over time.

Consider a trivial example with two documents::

    >>> base = {
    ...         "foo": 1,
    ...         "bar": [ "one" ],
    ...      }

    >>> head = {
    ...         "bar": [ "two" ],
    ...         "baz": "Hello, world!"
    ...     }

We call the document we are merging changes into *base* and the changed
document *head*. To merge these two documents using *jsonmerge*::

    >>> from pprint import pprint

    >>> from jsonmerge import merge
    >>> result = merge(base, head)

    >>> pprint(result, width=40)
    {'bar': ['two'],
     'baz': 'Hello, world!',
     'foo': 1}

As you can see, when encountering an JSON object, *jsonmerge* by default
returns fields that appear in either *base* or *head* document. For other
JSON types, it simply replaces the older value. These principles are also
applied in case of multiple nested JSON objects.

In a more realistic use case however, you might want to apply different
*merge strategies* to different parts of the document. You can tell
*jsonmerge* how to do that using a syntax based on `JSON schema`_.

If you already have schemas for your document, you can simply expand them
with additional keywords recognized by *jsonmerge*.

You use the *mergeStrategy* schema keyword to specify the strategy. The
default two strategies mentioned above are called *objectMerge* for objects
and *overwrite* for all other types.

Let's say you want to specify that the merged *bar* field in the example
document above should contain elements from all documents, not just the
latest one. You can do this with a schema like this::

    >>> schema = {
    ...             "properties": {
    ...                 "bar": {
    ...                     "mergeStrategy": "append"
    ...                 }
    ...             }
    ...         }

    >>> from jsonmerge import Merger
    >>> merger = Merger(schema)
    >>> result = merger.merge(base, head)

    >>> pprint(result, width=40)
    {'bar': ['one', 'two'],
     'baz': 'Hello, world!',
     'foo': 1}

Another common example is when you need to keep a versioned list of values
that appeared in the series of documents::

    >>> schema = {
    ...             "properties": {
    ...                 "foo": {
    ...                     "type": "object",
    ...                     "mergeStrategy": "version",
    ...                     "mergeOptions": { "limit": 5 }
    ...                 }
    ...             }
    ...         }
    >>> from jsonmerge import Merger
    >>> merger = Merger(schema)

    >>> v1 = {
    ...     'foo': {
    ...         'greeting': 'Hello, World!'
    ...     }
    ... }

    >>> v2 = {
    ...     'foo': {
    ...         'greeting': 'Howdy, World!'
    ...     }
    ... }

    >>> base = None
    >>> base = merger.merge(base, v1, meta={'version': 1})
    >>> base = merger.merge(base, v2, meta={'version': 2})

    >>> pprint(base, width=55)
    {'foo': [{'value': {'greeting': 'Hello, World!'},
              'version': 1},
             {'value': {'greeting': 'Howdy, World!'},
              'version': 2}]}

Note that we use the *mergeOptions* keyword to supply additional options to
the merge strategy. In this case, we tell the *version* strategy to retain
only 5 most recent versions of this field. We also used the *meta* argument
to supply some document meta-data that is included for each version of the
field. *meta* can contain an arbitrary JSON object.

Example above also demonstrates how *jsonmerge* is typically used when
merging more than two documents. Typically you start with an empty *base*
and then consecutively merge different *heads* into it.

If you care about well-formedness of your documents, you might also want to
obtain a schema for the documents that the *merge* method creates.
*jsonmerge* provides a way to automatically generate it from a schema for
the input document::

    >>> result_schema = merger.get_schema()

    >>> pprint(result_schema, width=80)
    {'properties': {'foo': {'items': {'properties': {'value': {'type': 'object'}}},
                            'maxItems': 5,
                            'type': 'array'}}}

Note that because of the *version* strategy, the type of the *foo* field
changed from *object* to *array*.


Merge strategies
----------------

These are the currently implemented merge strategies.

overwrite
  Overwrite with the value in *base* with value in *head*. Works with any
  type.

append
  Append arrays. Works only with arrays.

arrayMergeById
  Merge arrays, identifying items to be merged by an ID field. Resulting
  arrays have items from both *base* and *head* arrays.  Any items that
  have identical an ID are merged based on the strategy specified further
  down in the hierarchy.

  By default, array items are expected to be objects and ID of the item is
  obtained from the *id* property of the object.

  You can specify an arbitrary *JSON pointer* to point to the ID of the
  item using the *idRef* merge option. When resolving the pointer, document
  root is placed at the root of the array item (e.g. by default, *idRef* is
  '/id')

  Array items in *head* for which the ID cannot be identified (e.g. *idRef*
  pointer is invalid) are ignored.

  You can specify an additional item ID to be ignored using the *ignoreId*
  merge option.

objectMerge
  Merge objects. Resulting objects have properties from both *base* and
  *head*. Any properties that are present both in *base* and *head* are
  merged based on the strategy specified further down in the hierarchy
  (e.g. in *properties*, *patternProperties* or *additionalProperties*
  schema keywords).

version
  Changes the type of the value to an array. New values are appended to the
  array in the form of an object with a *value* property. This way all
  values seen during the merge are preserved.

  You can limit the length of the list using the *limit* option in the
  *mergeOptions* keyword.

  By default, if a *head* document contains the same value as the *base*,
  document, no new version will be appended. You can change this by setting
  *ignoreDups* option to *false*.

If a merge strategy is not specified in the schema, *objectMerge* is used
to objects and *overwrite* for all other values.

You can implement your own strategies by making subclasses of
jsonmerge.strategies.Strategy and passing them to Merger() constructor.


Limitations
-----------

Merging of documents with schemas that do not have a well-defined type
(e.g. schemas using *allOf*, *anyOf* and *oneOf*) will likely fail. Such
documents could require merging of two values of different types. For
example, *jsonmerge* does not know how to merge a string to an object.

You can work around this limitation by defining for your own strategy that
defines what to do in such cases. See docstring documentation for the
*Strategy* class on how to do that. get_schema() however currently provides
no support for ambiguous schemas like that.


Requirements
------------

You need *jsonschema* (https://pypi.python.org/pypi/jsonschema) module
installed.


Installation
------------

You install *jsonmerge*, as you would install any Python module, by running
these commands::

    python setup.py install
    python setup.py test


Source
------

The latest version is available on GitHub: https://github.com/avian2/jsonmerge


License
-------

Copyright 2014, Tomaz Solc <tomaz.solc@tablix.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

.. _JSON schema: http://json-schema.org

..
    vim: tw=75 ts=4 sw=4 expandtab softtabstop=4
