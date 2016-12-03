"""License copied from https://bitbucket.org/ericsnowcurrently/apache_conf_parser/src/c4ed197beddf/LICENSE
Copyright (c) 2012, Eric Snow
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import re as re_mod
import abc as abc_mod
import collections as col_mod

INDENT = "    "
_NODES = []


class ListAdapter(col_mod.MutableSequence):
    def __init__(self, *args):
        self.items = []
        for arg in args:
            self.append(arg)
    def __len__(self): 
        return len(self.items)
    def __contains__(self, val): 
        return val in self.items
    def __iter__(self): 
        return iter(self.items)
    def __getitem__(self, index):
        return self.items[index]
    def __setitem__(self, index, val):
        self.items[index] = val
    def __delitem__(self, index):
        del self.items[index]
    def insert(self, index, val):
        self.items.insert(index, val)
    def __eq__(self, val):
        return self.items == val
    def __ne__(self, val):
        return self.items != val
    def __lt__(self, val):
        return self.items < val
    def __le__(self, val):
        return self.items <= val
    def __gt__(self, val):
        return self.items > val
    def __ge__(self, val):
        return self.items >= val
    def __repr__(self):
        return repr(self.items)


class ParserError(Exception): pass
class InvalidLineError(ParserError): pass
class NodeCompleteError(InvalidLineError): pass
class DirectiveError(ParserError): pass
class NestingLimitError(ParserError): pass
class NodeMatchError(ParserError): pass


class Node(object):
    """
    The apache parser node.

    """
    __metaclass__ = abc_mod.ABCMeta

    MATCH_RE = ".*"
    def __init__(self):
        self.lines = []
        self._content = None
        self._complete = False
        self.changed = False
    @property
    def complete(self):
        return self._complete
    @complete.setter
    def complete(self, val):
        self._complete = val
    @classmethod
    def match(cls, line):
        if line is None:
            return False
        return bool(re_mod.match(cls.MATCH_RE, line))
    @property
    def content(self):
        return "\n".join(self.lines)
    @property
    def stable(self):
        return True
    def add_line(self, line):
        if "\n" in line:
            raise InvalidLineError("Lines cannot contain newlines.")
        if self.complete:
            raise NodeCompleteError(line)
        self.lines.append(line)
        if not line.endswith("\\"):
            self.complete = True
    def __str__(self):
        if self.changed:
            return self.content or ""
        return "\n".join(self.lines)
    def pprint(self, depth=0):
        return "%s%s" % (INDENT*depth, str(self).lstrip())
    

class CommentNode(Node):
    """A comment."""
    MATCH_RE = r"\s*#(.*[^\\])?$"
    def add_line(self, line):
        if line.endswith("\\"):
            raise InvalidLineError("Comments cannot have line continuations.")
        super(CommentNode, self).add_line(line)
        self._content = line.split("#", 1)[1]
    def __str__(self):
        if not self.lines:
            raise NodeCompleteError("Can't turn an uninitialized comment node into a string.")
        if self.changed:
            return "#"+self._content
        return super(CommentNode, self).__str__()
_NODES.append(CommentNode)


class BlankNode(Node):
    """A blank line."""
    MATCH_RE = "\s*$"
    def add_line(self, line):
        if line.endswith("\\"):
            raise InvalidLineError("Blank lines cannot have line continuations.")
        super(BlankNode, self).add_line(line)
        self._content = ""
    def pprint(self, depth=0):
        return ""
_NODES.append(BlankNode)


class Directive(Node):
    """A configuration directive."""
    # MOZILLA MODIFICATION:
    # Original NAME_RE was "[a-zA-Z]\w*", but patcher configs use non-word
    # characters in node names, so we needed to be more liberal about what
    # is acceptable here.
    NAME_RE = "[a-zA-Z0-9][-.\w]*"

    def __init__(self):
        super(Directive, self).__init__()
        self._stable = False

    class Name(object):
        # cannot be changed once set
        def __get__(self, obj, cls):
            if not hasattr(obj, "_name"):
                raise NodeCompleteError("Name has not been set yet.")
            return obj._name
        def __set__(self, obj, value):
            if hasattr(obj, "_name") and obj._name is not None:
                raise DirectiveError("Name is already set.  Cannot set to %s" % value)
            if not re_mod.match(obj.NAME_RE, value):
                raise DirectiveError("Invalid name: %s" % value)
            obj._name = value
            obj._stable = True # name is the first thing in the line
    name = Name()

    class Arguments(object):
        def __get__(self, obj, cls):
            if not hasattr(obj, "_arguments"):
                class ValidatedArgs(ListAdapter):
                    """
                    Validate the addition, change, or removal of arguments.

                    """
                    def __setitem__(self, index, val):
                        #self.validate_general(val)
                        self.items[index] = val
                        if obj.complete:
                            obj.changed = True
                    def __delitem__(self, index):
                        del self.items[index]
                        if obj.complete:
                            obj.changed = True
                    def insert(self, index, val):
                        # append calls this
                        #self.validate_general(val)
                        self.items.insert(index, val)
                        if obj.complete:
                            obj.changed = True
                    def validate_general(self, val):
                        #if "<" in line or ">" in line:
                        #if re_mod.match("([^\"'<>]*('.*?'|\".*?\"))*[^<>]*$", line):
                        if not re_mod.match("'.*'$|\".*\"$", val) and ("<" in val or ">" in val):
                            raise DirectiveError("Angle brackets not allowed in directive args.  Received: %s" % val)
                obj._arguments = ValidatedArgs()
            return obj._arguments
    arguments = Arguments()

    def add_to_header(self, line):
        if self.complete:
            raise NodeCompleteError("Cannot add to the header of a complete directive.")
        if not line:
            raise DirectiveError("An empty line is not a valid header line.")
        stable = True
        if line[-1] == "\\":
            line = line[:-1]
            stable = False
        parts = line.strip().split()
        try:
            self.name
        except NodeCompleteError:
            self.name = parts[0]
            parts = parts[1:]
        for part in parts:
            self.arguments.append(part)
        self._stable = stable

    @abc_mod.abstractmethod
    def add_line(self, line):
        super(Directive, self).add_line(line)

    @property
    def stable(self):
        return self._stable

    @property
    def complete(self):
        return super(Directive, Directive).complete.fget(self)
    @complete.setter
    def complete(self, val):
        if val and not self.stable:
            raise NodeCompleteError("Can't set an unstable Directive to complete.")
        super(Directive, Directive).complete.fset(self, val)

    @property
    def content(self):
        name = self.name if not self.arguments else self.name + " "
        return "%s%s" % (name, " ".join(arg for arg in self.arguments))

    def __repr__(self):
        name = self.name or ""
        if name:
            name += " "
        return "<%sDirective at %s>" % (
                name,
                id(self),
                )
    def pprint(self, depth=0):
        return "%s%s %s" % (
                INDENT*depth, 
                self.name, 
                " ".join([arg for arg in self.arguments]),
                )


class SimpleDirective(Directive):
    MATCH_RE = r"\s*%s(\s+.*)*\s*[\\]?$" % Directive.NAME_RE
    def add_line(self, line):
        try:
            self.add_to_header(line)
        except DirectiveError as e:
            raise InvalidLineError(str(e))
        super(SimpleDirective, self).add_line(line)
    def __str__(self):
        if not self.lines:
            raise NodeCompleteError("Can't turn an uninitialized simple directive into a string.")
        return super(SimpleDirective, self).__str__()
_NODES.append(SimpleDirective)


class ComplexNode(Node):
    """
    A node that is composed of a list of other nodes.

    """
    NESTING_LIMIT = 10
    def __init__(self, candidates):
        super(ComplexNode, self).__init__()
        self.candidates = candidates

    class Nodes(object):
        def __get__(self, obj, cls):
            if not hasattr(obj, "_nodes"):
                class ChangeAwareNodes(ListAdapter):
                    # disallow changing completion status of nodes once added?
                    def __setitem__(self, index, val):
                        self.items[index] = val
                        if obj.complete:
                            obj.changed = True
                    def __delitem__(self, index):
                        del self.items[index]
                        if obj.complete:
                            obj.changed = True
                    def insert(self, index, val):
                        self.items.insert(index, val)
                        if obj.complete:
                            obj.changed = True
                    @property
                    def stable(self):
                        #if not self.items or self.items[-1].complete:
                        #    return True
                        #return False
                        return all(item.stable for item in self.items)
                    def __repr__(self):
                        return repr(self.items)
                    @property
                    def directives(self):
                        if not obj.complete or obj.changed or not hasattr(self, "_names"):
                            self._directives = {}
                            for item in self.items:
                                if isinstance(item, Directive):
                                    if item.name not in self._directives:
                                        self._directives[item.name] = []
                                    if item not in self._directives[item.name]:
                                        self._directives[item.name].append(item)
                        return self._directives       
                        #result = {}
                        #for item in self.items:
                        #    if isinstance(item, Directive):
                        #        if item.name not in result:
                        #            result[item.name] = []
                        #        if item not in result[item.name]:
                        #            result[item.name].append(item)
                        #return result
                obj._nodes = ChangeAwareNodes()
            return obj._nodes
    nodes = Nodes()

    @property
    def complete(self):
        return self._complete
    @complete.setter
    def complete(self, val):
        if val and not self.nodes.stable:
            raise NodeCompleteError("The node list is not stable.  Likely the last node is still waiting for additional lines.")
        super(ComplexNode, ComplexNode).complete.fset(self, val)
    
    @property
    def stable(self):
        return self.nodes.stable

    def get_node(self, line):
        for node_cls in self.candidates:
            if node_cls.match(line):
                return node_cls()
        raise NodeMatchError("No matching node: %s" % line)

    def add_line(self, line, depth=0):
        if self.complete:
            raise NodeCompleteError("Can't add lines to a complete Node.")
        if depth > self.NESTING_LIMIT:
            raise NestingLimitError("Cannot nest directives more than %s levels." % self.NESTING_LIMIT)
        
        if not self.nodes.stable:
            node = self.nodes[-1]
            if isinstance(node, ComplexDirective):
                node.add_line(line, depth=depth+1)
            else:
                node.add_line(line)
        else:
            newnode = self.get_node(line)
            newnode.add_line(line)
            self.nodes.append(newnode)
        if not self.nodes.stable:
            self.complete = False

    def __str__(self):
        if not self.complete:
            raise NodeCompleteError("Can't turn an incomplete complex node into a string.")
        return "\n".join(str(node) for node in self.nodes)
    def pprint(self, depth=0):
        if not self.complete:
            raise NodeCompleteError("Can't print an incomplete complex node.")
        return "\n".join(node.pprint(depth) for node in self.nodes)


class ComplexDirective(Directive):
    MATCH_RE = r"\s*<\s*%s(\s+[^>]*)*\s*(>\s*|[\\])$" % Directive.NAME_RE
    BODY_CLASS = ComplexNode
    CANDIDATES = _NODES

    def __init__(self):
        super(ComplexDirective, self).__init__()
        self.header = SimpleDirective()
        self.body = self.BODY_CLASS(self.CANDIDATES)
        self.tail = ""
        self.tailmatch = False

    @property
    def name(self):
        return self.header.name
    @property
    def arguments(self):
        return self.header.arguments
    @property
    def stable(self):
        #return self.header.stable and self.body.stable
        return self.complete

    @property
    def complete(self):
        if self.body.complete and not self.header.complete:
            raise NodeCompleteError("Body is complete but header isn't.")
        if self.tailmatch and not self.body.complete:
            raise NodeCompleteError("Tail is matched but body is not complete.")
        return self.header.complete and self.body.complete and self.tailmatch
    @complete.setter
    def complete(self, val):
        if val and not (self.body.complete and self.header.complete and self.tailmatch):
            raise NodeCompleteError("Cannot set a complex directive to complete if its parts aren't complete")
        if not val and self.body.complete and self.header.complete and self.tailmatch:
            raise NodeCompleteError("Cannot set a complex directive to not complete if its parts are all complete")
        super(ComplexDirective, ComplexDirective).complete.fset(self, val)

    def add_line(self, line, depth=0):
        if self.complete:
            raise NodeCompleteError("Can't add lines to a complete Node.")
        if not self.header.complete:
            try:
                super(ComplexDirective, self).add_line(line)
            except NodeCompleteError:
                pass
            if ">" in line:
                header, remainder = line.split(">", 1)
                if remainder.strip() != "":
                    raise InvalidLineError("Directive header has an extraneous tail: %s" % line) 
            else:
                header = line
            header = header.lstrip()
            if header and header.startswith("<"):
                try:
                    self.header.name
                except NodeCompleteError:
                    header = header[1:]
            if header and "<" in header:
                raise InvalidLineError("Angle brackets not allowed in complex directive header.  Received: %s" % line)
            if header:
                try:
                    self.header.add_to_header(header)
                except (NodeCompleteError, DirectiveError) as e:
                    raise InvalidLineError(str(e))
            if ">" in line:
                self.header.complete = True
        elif not self.body.stable:
            self.body.add_line(line, depth+1)
        elif re_mod.match("\s*</%s>\s*$" % self.name, line):
            self.body.complete = True
            self.tail = line
            self.tailmatch = True
        elif not self.body.complete:
            self.body.add_line(line, depth+1)
        else:
            raise InvalidLineError("Expecting closing tag.  Got: %s" % line)
                
    def __str__(self):
        if not self.lines:
            raise NodeCompleteError("Can't turn an uninitialized complex directive into a string.")
        if not self.complete:
            raise NodeCompleteError("Can't turn an incomplete complex directive into a string.")
        return "%s\n%s%s%s" % (
                "\n".join(self.lines), 
                self.body, 
                "\n" if self.body.nodes else "",
                self.tail,
                )
    def pprint(self, depth=0):
        if not self.complete:
            raise NodeCompleteError("Can't print an incomplete complex directive.")
        return "%s<%s>\n%s%s%s</%s>" % (
                INDENT*depth, 
                self.header.pprint(), 
                self.body.pprint(depth=depth+1), 
                "\n" if self.body.nodes else "",
                INDENT*depth,
                self.name,
                ) 
_NODES.append(ComplexDirective)


class ApacheConfParser(ComplexNode):
    def __init__(self, source, infile=True, delay=False, count=None):
        """Count is the starting number for line counting..."""
        super(ApacheConfParser, self).__init__(_NODES)
	self.source = source.splitlines()
	if infile:
            self.source = (line.strip("\n") for line in open(source))
        self.count = count
        if not delay:
            self.parse()

    def parse(self):
        if self.complete:
            return
        for line in self.source:
            if self.count is not None:
                print(self.count)
                self.count += 1
            self.add_line(line)
        self.complete = True
                

