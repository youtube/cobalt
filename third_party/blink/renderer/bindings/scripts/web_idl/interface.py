# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools

from .attribute import Attribute
from .composition_parts import Identifier
from .composition_parts import WithCodeGeneratorInfo
from .composition_parts import WithComponent
from .composition_parts import WithDebugInfo
from .composition_parts import WithExposure
from .composition_parts import WithExtendedAttributes
from .composition_parts import WithIdentifier
from .composition_parts import WithOwner
from .constant import Constant
from .constructor import Constructor
from .constructor import ConstructorGroup
from .idl_type import IdlType
from .ir_map import IRMap
from .make_copy import make_copy
from .operation import Operation
from .operation import OperationGroup
from .reference import RefById
from .user_defined_type import UserDefinedType


class Interface(UserDefinedType, WithExtendedAttributes, WithCodeGeneratorInfo,
                WithExposure, WithComponent, WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-interfaces"""

    class IR(IRMap.IR, WithExtendedAttributes, WithCodeGeneratorInfo,
             WithExposure, WithComponent, WithDebugInfo):
        def __init__(self,
                     identifier,
                     is_partial,
                     is_mixin,
                     inherited=None,
                     attributes=None,
                     constants=None,
                     constructors=None,
                     named_constructors=None,
                     operations=None,
                     iterable=None,
                     maplike=None,
                     setlike=None,
                     extended_attributes=None,
                     component=None,
                     debug_info=None):
            assert isinstance(is_partial, bool)
            assert isinstance(is_mixin, bool)
            assert inherited is None or isinstance(inherited, RefById)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert constants is None or isinstance(constants, (list, tuple))
            assert constructors is None or isinstance(constructors,
                                                      (list, tuple))
            assert named_constructors is None or isinstance(
                named_constructors, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            assert iterable is None or isinstance(iterable, Iterable.IR)
            assert maplike is None or isinstance(maplike, Maplike.IR)
            assert setlike is None or isinstance(setlike, Setlike.IR)

            attributes = attributes or []
            constants = constants or []
            constructors = constructors or []
            named_constructors = named_constructors or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(constant, Constant.IR) for constant in constants)
            assert all(
                isinstance(constructor, Constructor.IR)
                for constructor in constructors)
            assert all(
                isinstance(named_constructor, Constructor.IR)
                for named_constructor in named_constructors)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            kind = None
            if is_partial:
                if is_mixin:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.PARTIAL_INTERFACE
            else:
                if is_mixin:
                    kind = IRMap.IR.Kind.INTERFACE_MIXIN
                else:
                    kind = IRMap.IR.Kind.INTERFACE
            IRMap.IR.__init__(self, identifier=identifier, kind=kind)
            WithExtendedAttributes.__init__(self, extended_attributes)
            WithCodeGeneratorInfo.__init__(self)
            WithExposure.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.is_partial = is_partial
            self.is_mixin = is_mixin
            self.inherited = inherited
            self.deriveds = []
            self.attributes = list(attributes)
            self.constants = list(constants)
            self.constructors = list(constructors)
            self.constructor_groups = []
            self.named_constructors = list(named_constructors)
            self.named_constructor_groups = []
            self.operations = list(operations)
            self.operation_groups = []
            self.exposed_constructs = []
            self.legacy_window_aliases = []
            self.iterable = iterable
            self.maplike = maplike
            self.setlike = setlike
            self.sync_iterator = None

        def iter_all_members(self):
            list_of_members = [
                self.attributes,
                self.constants,
                self.constructors,
                self.named_constructors,
                self.operations,
            ]
            if self.iterable:
                list_of_members.append(self.iterable.operations)
            if self.maplike:
                list_of_members.append(self.maplike.attributes)
                list_of_members.append(self.maplike.operations)
            if self.setlike:
                list_of_members.append(self.setlike.attributes)
                list_of_members.append(self.setlike.operations)
            # self.sync_iterator.operations are not members of this interface.
            # They're members of an iterator prototype object.
            return itertools.chain(*list_of_members)

        def iter_all_overload_groups(self):
            # Returns all overload groups regardless of whether they're members
            # of this interface or not.
            list_of_groups = [
                self.constructor_groups,
                self.named_constructor_groups,
                self.operation_groups,
            ]
            if self.iterable:
                list_of_groups.append(self.iterable.operation_groups)
            if self.maplike:
                list_of_groups.append(self.maplike.operation_groups)
            if self.setlike:
                list_of_groups.append(self.setlike.operation_groups)
            # self.sync_iterator.operation_groups are not members of this
            # interface, but we'd like to let IdlCompiler work on all overload
            # groups in order to minimize the potentially-surprising
            # differences.
            #
            # This method is used by IdlCompiler to propagate IDL extended
            # attributes from operations to operation groups and also to
            # determine the exposure of operation groups from the operation's
            # exposure.
            #
            # Exactly and technically speaking, iterator object's "next" is not
            # an IDL operation (just like iterator objects are not platform
            # objects), however, we treat them in the same way as much as
            # possible just because it's implementor-friendly and no observable
            # side effect.
            if self.sync_iterator:
                list_of_groups.append(self.sync_iterator.operation_groups)
            return itertools.chain(*list_of_groups)

    def __init__(self, ir):
        assert isinstance(ir, Interface.IR)
        assert not ir.is_partial

        ir = make_copy(ir)
        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, ir, readonly=True)
        WithCodeGeneratorInfo.__init__(self, ir, readonly=True)
        WithExposure.__init__(self, ir, readonly=True)
        WithComponent.__init__(self, ir, readonly=True)
        WithDebugInfo.__init__(self, ir)

        self._is_mixin = ir.is_mixin
        self._inherited = ir.inherited
        self._deriveds = tuple(ir.deriveds)
        self._attributes = tuple([
            Attribute(attribute_ir, owner=self)
            for attribute_ir in ir.attributes
        ])
        self._constants = tuple([
            Constant(constant_ir, owner=self) for constant_ir in ir.constants
        ])
        self._constructors = tuple([
            Constructor(constructor_ir, owner=self)
            for constructor_ir in ir.constructors
        ])
        self._constructor_groups = tuple([
            ConstructorGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._constructors)),
                owner=self) for group_ir in ir.constructor_groups
        ])
        assert len(self._constructor_groups) <= 1
        self._named_constructors = tuple([
            Constructor(named_constructor_ir, owner=self)
            for named_constructor_ir in ir.named_constructors
        ])
        self._named_constructor_groups = tuple([
            ConstructorGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._named_constructors)),
                owner=self) for group_ir in ir.named_constructor_groups
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(group_ir,
                           list(
                               filter(
                                   lambda x: x.is_static == group_ir.is_static
                                   and x.identifier == group_ir.identifier,
                                   self._operations)),
                           owner=self) for group_ir in ir.operation_groups
        ])
        self._exposed_constructs = tuple(ir.exposed_constructs)
        self._legacy_window_aliases = tuple(ir.legacy_window_aliases)
        self._indexed_and_named_properties = None
        indexed_and_named_property_operations = list(
            filter(lambda x: x.is_indexed_or_named_property_operation,
                   self._operations))
        if indexed_and_named_property_operations:
            self._indexed_and_named_properties = IndexedAndNamedProperties(
                indexed_and_named_property_operations, owner=self)
        self._stringifier = None
        stringifier_operation_irs = list(
            filter(lambda x: x.is_stringifier, ir.operations))
        if stringifier_operation_irs:
            assert len(stringifier_operation_irs) == 1
            op_ir = make_copy(stringifier_operation_irs[0])
            if not op_ir.code_generator_info.property_implemented_as:
                if op_ir.identifier:
                    op_ir.code_generator_info.set_property_implemented_as(
                        str(op_ir.identifier))
                op_ir.change_identifier(Identifier('toString'))
            operation = Operation(op_ir, owner=self)
            attribute = None
            if operation.stringifier_attribute:
                attr_id = operation.stringifier_attribute
                attributes = list(
                    filter(lambda x: x.identifier == attr_id,
                           self._attributes))
                assert len(attributes) == 1
                attribute = attributes[0]
            self._stringifier = Stringifier(operation, attribute, owner=self)
        self._iterable = (Iterable(ir.iterable, owner=self)
                          if ir.iterable else None)
        self._maplike = Maplike(ir.maplike, owner=self) if ir.maplike else None
        self._setlike = Setlike(ir.setlike, owner=self) if ir.setlike else None
        self._sync_iterator = (SyncIterator(ir.sync_iterator, owner=self)
                               if ir.sync_iterator else None)

    @property
    def is_mixin(self):
        """Returns True if this is a mixin interface."""
        return self._is_mixin

    @property
    def inherited(self):
        """Returns the inherited interface or None."""
        return self._inherited.target_object if self._inherited else None

    @property
    def deriveds(self):
        """Returns the list of the derived interfaces."""
        return tuple(map(lambda ref: ref.target_object, self._deriveds))

    @property
    def inclusive_inherited_interfaces(self):
        """
        Returns the list of inclusive inherited interfaces.

        https://webidl.spec.whatwg.org/#interface-inclusive-inherited-interfaces
        """
        result = []
        interface = self
        while interface is not None:
            result.append(interface)
            interface = interface.inherited
        return result

    def does_implement(self, identifier):
        """
        Returns True if this is or inherits from the given interface.
        """
        assert isinstance(identifier, str)

        for interface in self.inclusive_inherited_interfaces:
            if interface.identifier == identifier:
                return True
        return False

    @property
    def attributes(self):
        """
        Returns attributes, including [LegacyUnforgeable] attributes in
        ancestors.
        """
        return self._attributes

    @property
    def constants(self):
        """Returns constants."""
        return self._constants

    @property
    def constructors(self):
        """Returns constructors."""
        return self._constructors

    @property
    def constructor_groups(self):
        """
        Returns groups of constructors.

        Constructors are grouped as operations are. There is 0 or 1 group.
        """
        return self._constructor_groups

    @property
    def named_constructors(self):
        """Returns named constructors."""
        return self._named_constructors

    @property
    def named_constructor_groups(self):
        """Returns groups of overloaded named constructors."""
        return self._named_constructor_groups

    @property
    def operations(self):
        """
        Returns all operations, including special operations without an
        identifier, as well as [LegacyUnforgeable] operations in ancestors.
        """
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations, including [LegacyUnforgeable]
        operations in ancestors.

        All operations that have an identifier are grouped by identifier, thus
        it's possible that there is a single operation in a certain operation
        group.  If an operation doesn't have an identifier, i.e. if it's a
        merely special operation, then the operation doesn't appear in any
        operation group.
        """
        return self._operation_groups

    @property
    def exposed_constructs(self):
        """
        Returns a list of the constructs that are exposed on this global object.
        """
        return tuple(
            map(lambda ref: ref.target_object, self._exposed_constructs))

    @property
    def legacy_window_aliases(self):
        """Returns a list of properties exposed as [LegacyWindowAlias]."""
        return self._legacy_window_aliases

    @property
    def indexed_and_named_properties(self):
        """Returns a IndexedAndNamedProperties or None."""
        return self._indexed_and_named_properties

    @property
    def stringifier(self):
        """Returns a Stringifier or None."""
        return self._stringifier

    @property
    def iterable(self):
        """Returns an Iterable or None."""
        return self._iterable

    @property
    def maplike(self):
        """Returns a Maplike or None."""
        return self._maplike

    @property
    def setlike(self):
        """Returns a Setlike or None."""
        return self._setlike

    @property
    def sync_iterator(self):
        """Returns a SyncIterator or None."""
        return self._sync_iterator

    # UserDefinedType overrides
    @property
    def is_interface(self):
        return True


class LegacyWindowAlias(WithIdentifier, WithExtendedAttributes, WithExposure):
    """
    Represents a property exposed on a Window object as [LegacyWindowAlias].
    """

    def __init__(self, identifier, original, extended_attributes, exposure):
        assert isinstance(original, RefById)

        WithIdentifier.__init__(self, identifier)
        WithExtendedAttributes.__init__(
            self, extended_attributes, readonly=True)
        WithExposure.__init__(self, exposure, readonly=True)

        self._original = original

    @property
    def original(self):
        """Returns the original object of this alias."""
        return self._original.target_object


class IndexedAndNamedProperties(WithOwner):
    """
    Represents a set of indexed/named getter/setter/deleter.

    https://webidl.spec.whatwg.org/#idl-indexed-properties
    https://webidl.spec.whatwg.org/#idl-named-properties
    """

    def __init__(self, operations, owner):
        assert isinstance(operations, (list, tuple))
        assert all(
            isinstance(operation, Operation) for operation in operations)

        WithOwner.__init__(self, owner)

        self._own_indexed_getter = None
        self._own_indexed_setter = None
        self._own_named_getter = None
        self._own_named_setter = None
        self._own_named_deleter = None

        for operation in operations:
            arg1_type = operation.arguments[0].idl_type
            if arg1_type.is_integer:
                if operation.is_getter:
                    assert self._own_indexed_getter is None
                    self._own_indexed_getter = operation
                elif operation.is_setter:
                    assert self._own_indexed_setter is None
                    self._own_indexed_setter = operation
                else:
                    assert False
            elif arg1_type.is_string:
                if operation.is_getter:
                    assert self._own_named_getter is None
                    self._own_named_getter = operation
                elif operation.is_setter:
                    assert self._own_named_setter is None
                    self._own_named_setter = operation
                elif operation.is_deleter:
                    assert self._own_named_deleter is None
                    self._own_named_deleter = operation
                else:
                    assert False
            else:
                assert False

    @property
    def has_indexed_properties(self):
        return self.indexed_getter or self.indexed_setter

    @property
    def has_named_properties(self):
        return self.named_getter or self.named_setter or self.named_deleter

    @property
    def is_named_property_enumerable(self):
        named_getter = self.named_getter
        return bool(named_getter
                    and 'NotEnumerable' not in named_getter.extended_attributes
                    and 'LegacyUnenumerableNamedProperties' not in self.owner.
                    extended_attributes)

    @property
    def indexed_getter(self):
        return self._find_accessor('own_indexed_getter')

    @property
    def indexed_setter(self):
        return self._find_accessor('own_indexed_setter')

    @property
    def named_getter(self):
        return self._find_accessor('own_named_getter')

    @property
    def named_setter(self):
        return self._find_accessor('own_named_setter')

    @property
    def named_deleter(self):
        return self._find_accessor('own_named_deleter')

    @property
    def own_indexed_getter(self):
        return self._own_indexed_getter

    @property
    def own_indexed_setter(self):
        return self._own_indexed_setter

    @property
    def own_named_getter(self):
        return self._own_named_getter

    @property
    def own_named_setter(self):
        return self._own_named_setter

    @property
    def own_named_deleter(self):
        return self._own_named_deleter

    def _find_accessor(self, attr):
        for interface in self.owner.inclusive_inherited_interfaces:
            props = interface.indexed_and_named_properties
            if props:
                accessor = getattr(props, attr)
                if accessor:
                    return accessor
        return None


class Stringifier(WithOwner):
    """https://webidl.spec.whatwg.org/#idl-stringifiers"""

    def __init__(self, operation, attribute, owner):
        assert isinstance(operation, Operation)
        assert attribute is None or isinstance(attribute, Attribute)

        WithOwner.__init__(self, owner)

        self._operation = operation
        self._attribute = attribute

    @property
    def operation(self):
        return self._operation

    @property
    def attribute(self):
        return self._attribute


class Iterable(WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-iterable"""

    class IR(WithDebugInfo):
        def __init__(self,
                     key_type=None,
                     value_type=None,
                     operations=None,
                     debug_info=None):
            assert key_type is None or isinstance(key_type, IdlType)
            assert isinstance(value_type, IdlType)
            assert operations is None or isinstance(operations, (list, tuple))
            operations = operations or []
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            WithDebugInfo.__init__(self, debug_info)

            self.key_type = key_type
            self.value_type = value_type
            self.operations = list(operations)
            self.operation_groups = []

    def __init__(self, ir, owner):
        assert isinstance(ir, Iterable.IR)
        assert isinstance(owner, Interface)

        WithDebugInfo.__init__(self, ir)

        self._key_type = ir.key_type
        self._value_type = ir.value_type
        self._operations = tuple([
            Operation(operation_ir, owner=owner)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._operations)),
                owner=owner) for group_ir in ir.operation_groups
        ])

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type

    @property
    def attributes(self):
        """Returns attributes supported by an iterable declaration."""
        return ()

    @property
    def operations(self):
        """Returns operations supported by an iterable declaration."""
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations supported by an iterable
        declaration.
        """
        return self._operation_groups


class Maplike(WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-maplike"""

    class IR(WithDebugInfo):
        def __init__(self,
                     key_type,
                     value_type,
                     is_readonly,
                     attributes=None,
                     operations=None,
                     debug_info=None):
            assert isinstance(key_type, IdlType)
            assert isinstance(value_type, IdlType)
            assert isinstance(is_readonly, bool)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            attributes = attributes or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            WithDebugInfo.__init__(self, debug_info)

            self.key_type = key_type
            self.value_type = value_type
            self.is_readonly = is_readonly
            self.attributes = list(attributes)
            self.operations = list(operations)
            self.operation_groups = []

    def __init__(self, ir, owner):
        assert isinstance(ir, Maplike.IR)
        assert isinstance(owner, Interface)

        WithDebugInfo.__init__(self, ir)

        self._key_type = ir.key_type
        self._value_type = ir.value_type
        self._is_readonly = ir.is_readonly
        self._attributes = tuple([
            Attribute(attribute_ir, owner=owner)
            for attribute_ir in ir.attributes
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=owner)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._operations)),
                owner=owner) for group_ir in ir.operation_groups
        ])

    @property
    def key_type(self):
        """Returns the key type."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type

    @property
    def is_readonly(self):
        """Returns True if this is a readonly maplike."""
        return self._is_readonly

    @property
    def attributes(self):
        """Returns attributes supported by a maplike declaration."""
        return self._attributes

    @property
    def operations(self):
        """Returns operations supported by a maplike declaration."""
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations supported by a maplike
        declaration.
        """
        return self._operation_groups


class Setlike(WithDebugInfo):
    """https://webidl.spec.whatwg.org/#idl-setlike"""

    class IR(WithDebugInfo):
        def __init__(self,
                     value_type,
                     is_readonly,
                     attributes=None,
                     operations=None,
                     debug_info=None):
            assert isinstance(value_type, IdlType)
            assert isinstance(is_readonly, bool)
            assert attributes is None or isinstance(attributes, (list, tuple))
            assert operations is None or isinstance(operations, (list, tuple))
            attributes = attributes or []
            operations = operations or []
            assert all(
                isinstance(attribute, Attribute.IR)
                for attribute in attributes)
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            WithDebugInfo.__init__(self, debug_info)

            self.key_type = None
            self.value_type = value_type
            self.is_readonly = is_readonly
            self.attributes = list(attributes)
            self.operations = list(operations)
            self.operation_groups = []

    def __init__(self, ir, owner):
        assert isinstance(ir, Setlike.IR)
        assert isinstance(owner, Interface)

        WithDebugInfo.__init__(self, ir)

        self._value_type = ir.value_type
        self._is_readonly = ir.is_readonly
        self._attributes = tuple([
            Attribute(attribute_ir, owner=owner)
            for attribute_ir in ir.attributes
        ])
        self._operations = tuple([
            Operation(operation_ir, owner=owner)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._operations)),
                owner=owner) for group_ir in ir.operation_groups
        ])

    @property
    def key_type(self):
        """Returns None rather than raising no attribute error."""
        return None

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type

    @property
    def is_readonly(self):
        """Returns True if this is a readonly setlike."""
        return self._is_readonly

    @property
    def attributes(self):
        """Returns attributes supported by a setlike declaration."""
        return self._attributes

    @property
    def operations(self):
        """Returns operations supported by a setlike declaration."""
        return self._operations

    @property
    def operation_groups(self):
        """
        Returns groups of overloaded operations supported by a setlike
        declaration.
        """
        return self._operation_groups


class SyncIterator(UserDefinedType, WithExtendedAttributes,
                   WithCodeGeneratorInfo, WithExposure, WithComponent,
                   WithDebugInfo):
    """
    Represents a sync iterator type for 'default iterator objects' [1],
    which exists for every interface that has a 'pair iterator' [2][3], or
    a map/set iterator type of a maplike/setlike interface [4][5].

    [1] https://webidl.spec.whatwg.org/#es-default-iterator-object
    [2] https://webidl.spec.whatwg.org/#es-iterable
    [3] https://webidl.spec.whatwg.org/#dfn-pair-iterator
    [4] https://webidl.spec.whatwg.org/#create-a-map-iterator
    [5] https://webidl.spec.whatwg.org/#create-a-set-iterator
    """

    class IR(WithIdentifier, WithCodeGeneratorInfo, WithComponent,
             WithDebugInfo):
        def __init__(self, interface_ir, component, debug_info, key_type,
                     value_type, operations):
            assert isinstance(interface_ir, Interface.IR)
            assert key_type is None or isinstance(key_type, IdlType)
            assert isinstance(value_type, IdlType)
            assert isinstance(operations, (list, tuple))
            assert all(
                isinstance(operation, Operation.IR)
                for operation in operations)

            identifier = Identifier('SyncIterator_{}'.format(
                interface_ir.identifier))

            WithIdentifier.__init__(self, identifier)
            WithCodeGeneratorInfo.__init__(self)
            WithComponent.__init__(self, component)
            WithDebugInfo.__init__(self, debug_info)

            self.code_generator_info.set_for_testing(
                interface_ir.code_generator_info.for_testing)

            self.key_type = key_type
            self.value_type = value_type
            self.operations = list(operations)
            self.operation_groups = []

    def __init__(self, ir, owner):
        assert isinstance(ir, SyncIterator.IR)
        assert isinstance(owner, Interface)

        UserDefinedType.__init__(self, ir.identifier)
        WithExtendedAttributes.__init__(self, readonly=True)
        WithCodeGeneratorInfo.__init__(self,
                                       ir.code_generator_info,
                                       readonly=True)
        WithExposure.__init__(self, readonly=True)
        WithComponent.__init__(self, ir.components, readonly=True)
        WithDebugInfo.__init__(self, ir.debug_info)

        self._interface = owner
        self._key_type = ir.key_type
        self._value_type = ir.value_type
        self._operations = tuple([
            Operation(operation_ir, owner=self)
            for operation_ir in ir.operations
        ])
        self._operation_groups = tuple([
            OperationGroup(
                group_ir,
                list(
                    filter(lambda x: x.identifier == group_ir.identifier,
                           self._operations)),
                owner=self) for group_ir in ir.operation_groups
        ])

    @property
    def interface(self):
        """Returns the interface that defines this sync iterator."""
        return self._interface

    @property
    def key_type(self):
        """Returns the key type or None."""
        return self._key_type

    @property
    def value_type(self):
        """Returns the value type."""
        return self._value_type

    @property
    def inherited(self):
        # Just to be compatible with web_idl.Interface.
        return None

    @property
    def deriveds(self):
        # Just to be compatible with web_idl.Interface.
        return ()

    @property
    def attributes(self):
        """Returns attributes."""
        return ()

    @property
    def constants(self):
        """Returns constants."""
        return ()

    @property
    def constructors(self):
        """Returns constructors."""
        return ()

    @property
    def constructor_groups(self):
        """Returns groups of constructors."""
        return ()

    @property
    def named_constructors(self):
        """Returns named constructors."""
        return ()

    @property
    def named_constructor_groups(self):
        """Returns groups of overloaded named constructors."""
        return ()

    @property
    def operations(self):
        """Returns operations."""
        return self._operations

    @property
    def operation_groups(self):
        """Returns a list of OperationGroups."""
        return self._operation_groups

    @property
    def exposed_constructs(self):
        """Returns exposed constructs."""
        return ()

    # UserDefinedType overrides
    @property
    def is_sync_iterator(self):
        return True
