/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/exportcplusplus.h>
#include <sys/prx.h>

SYS_MODULE_INFO(javascriptcore, 0, 1, 0);

SYS_LIB_DECLARE(javascriptcore, SYS_LIB_AUTO_EXPORT | SYS_LIB_WEAK_IMPORT);

SYS_MODULE_START(prx_start);

// C exports
SYS_LIB_EXPORT(WTFReportBacktrace, javascriptcore);
SYS_LIB_EXPORT(WTFInvokeCrashHook, javascriptcore);
SYS_LIB_EXPORT(WTFReportAssertionFailure, javascriptcore);

// C++ classes
// Note: virtual functions must be inlined for class export to work, otherwise
// the linker will fail with "unable to export data to ELF".
SYS_LIB_EXPORTPICKUP_CLASS("JSC::WeakHandleOwner", javascriptcore);
SYS_LIB_EXPORTPICKUP_CLASS("JSC::Debugger", javascriptcore);

// C++ functions

SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::call(JSC::ExecState*, JSC::JSValue, JSC::CallType, JSC::CallData const&, JSC::JSValue, JSC::ArgList const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::callHostFunctionAsConstructor(JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createNotEnoughArgumentsError(JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createRangeError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createReferenceError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createSyntaxError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createTypeError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::createURIError(JSC::JSGlobalObject*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::DebuggerCallFrame::functionName() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::DebuggerCallFrame::thisObject() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::DynamicGlobalObjectScope::DynamicGlobalObjectScope(JSC::JSGlobalData&, JSC::JSGlobalObject*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::ErrorInstance::ErrorInstance(JSC::JSGlobalData&, JSC::Structure*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::ErrorInstance::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::evaluate(JSC::ExecState*, JSC::SourceCode const&, JSC::JSValue, JSC::JSValue*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::ExecutableBase::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::FunctionExecutable::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::getCalculatedDisplayName(JSC::ExecState*, JSC::JSObject*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::getCallableObjectSlow(JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::GetStrictModeReadonlyPropertyWriteError()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::HandleSet::grow()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::HandleSet::writeBarrier(JSC::JSValue*, JSC::JSValue const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::HashTable::createTable(JSC::JSGlobalData*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::HashTable::deleteTable() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Heap::addFinalizer(JSC::JSCell*, void (*)(JSC::JSCell*))", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Heap::collectAllGarbage()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Heap::isValidAllocation(unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Heap::reportExtraMemoryCostSlowCase(unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Identifier::add(JSC::ExecState*, char const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Identifier::addSlowCase(JSC::ExecState*, WTF::StringImpl*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Identifier::checkCurrentIdentifierTable(JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Identifier::from(JSC::ExecState*, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::initializeThreading()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::InternalFunction::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Interpreter::getStackTrace(JSC::JSGlobalData*, WTF::Vector<JSC::StackFrame, 0>&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Interpreter::retrieveLastCaller(JSC::ExecState*, int&, int&, WTF::String&, JSC::JSValue&) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::classInfo() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::className()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::customHasInstanceInternal(JSC::JSObject*, JSC::ExecState*, JSC::JSValue)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::destroyInternal(JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::getCallDataInternal(JSC::JSCell*, JSC::CallData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::getConstructDataInternal(JSC::JSCell*, JSC::ConstructData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::getObject()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSCell::toObject(JSC::ExecState*, JSC::JSGlobalObject*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFinalObject::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFinalObject::create(JSC::ExecState*, JSC::Structure*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::create(JSC::ExecState*, JSC::JSGlobalObject*, int, WTF::String const&, long long (*)(JSC::ExecState*), JSC::Intrinsic, long long (*)(JSC::ExecState*))", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::defineOwnPropertyInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&, bool)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::deletePropertyInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::destroyInternal(JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::finishCreation(JSC::ExecState*, JSC::NativeExecutable*, int, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::getCallDataInternal(JSC::JSCell*, JSC::CallData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::getConstructDataInternal(JSC::JSCell*, JSC::ConstructData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::getOwnNonIndexPropertyNamesInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::getOwnPropertyDescriptorInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::isHostFunctionNonInline() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::JSFunction(JSC::ExecState*, JSC::JSGlobalObject*, JSC::Structure*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::putInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, JSC::PutPropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSFunction::visitChildrenInternal(JSC::JSCell*, JSC::SlotVisitor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalData::create(JSC::HeapType)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalData::getHostFunction(long long (*)(JSC::ExecState*), long long (*)(JSC::ExecState*))", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalData::~JSGlobalData()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::defineOwnPropertyInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&, bool)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::getOwnPropertyDescriptorInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::getOwnPropertySlotInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::globalExec()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::init(JSC::JSObject*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::JSGlobalObject(JSC::JSGlobalData&, JSC::Structure*, JSC::GlobalObjectMethodTable const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::putDirectVirtualInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::putInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, JSC::PutPropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::toThisObjectInternal(JSC::JSCell*, JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::visitChildrenInternal(JSC::JSCell*, JSC::SlotVisitor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSGlobalObject::~JSGlobalObject()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSLockHolder::JSLockHolder(JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSLockHolder::JSLockHolder(JSC::JSGlobalData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSLockHolder::JSLockHolder(JSC::JSGlobalData*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSLockHolder::~JSLockHolder()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::classNameInternal(JSC::JSObject const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::copyBackingStoreInternal(JSC::JSCell*, JSC::CopyVisitor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::defaultValueInternal(JSC::JSObject const*, JSC::ExecState*, JSC::PreferredPrimitiveType)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::defineOwnPropertyInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&, bool)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::deletePropertyByIndexInternal(JSC::JSCell*, JSC::ExecState*, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::deletePropertyInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::fillGetterPropertySlot(JSC::PropertySlot&, int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getOwnNonIndexPropertyNamesInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getOwnPropertyDescriptorInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getOwnPropertyNamesInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getOwnPropertySlotByIndexInternal(JSC::JSCell*, JSC::ExecState*, unsigned int, JSC::PropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getOwnPropertySlotSlow(JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::getPropertyNamesInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::growOutOfLineStorage(JSC::JSGlobalData&, unsigned int, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::hasOwnProperty(JSC::ExecState*, JSC::PropertyName) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::hasProperty(JSC::ExecState*, JSC::PropertyName) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::putByIndexInternal(JSC::JSCell*, JSC::ExecState*, unsigned int, JSC::JSValue, bool)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::putDirectVirtual(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::putInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, JSC::PutPropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::setPrototype(JSC::JSGlobalData&, JSC::JSValue)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::toThisObjectInternal(JSC::JSCell*, JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSObject::visitChildrenInternal(JSC::JSCell*, JSC::SlotVisitor&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSRopeString::resolveRope(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSScope::objectAtScope(JSC::JSScope*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSString::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSString::toBoolean() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSSymbolTableObject::deletePropertyInternal(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSSymbolTableObject::getOwnNonIndexPropertyNamesInternal(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::isValidCallee()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::toNumberSlowCase(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::toObjectSlowCase(JSC::ExecState*, JSC::JSGlobalObject*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::toStringSlowCase(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::toThisObjectSlowCase(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::JSValue::toWTFStringSlowCase(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::MarkedAllocator::allocateSlowCase(unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::MarkedArgumentBuffer::slowAppend(JSC::JSValue)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::MarkedSpace::canonicalizeCellLivenessData()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::MarkStackArray::expand()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::PropertyDescriptor::setDescriptor(JSC::JSValue, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::PropertyNameArray::add(WTF::StringImpl*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::PropertySlot::functionGetter(JSC::ExecState*) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::setUpStaticFunctionSlot(JSC::ExecState*, JSC::HashEntry const*, JSC::JSObject*, JSC::PropertyName, JSC::PropertySlot&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::SharedSymbolTable::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::SlotVisitor::validate(JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::slowValidateCell(JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::slowValidateCell(JSC::JSGlobalObject*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::SmallStrings::createEmptyString(JSC::JSGlobalData*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::SmallStrings::createSingleCharacterString(JSC::JSGlobalData*, unsigned char)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::SourceProviderCache::~SourceProviderCache()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::addPropertyTransition(JSC::JSGlobalData&, JSC::Structure*, JSC::PropertyName, unsigned int, JSC::JSCell*, int&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::addPropertyTransitionToExistingStructure(JSC::Structure*, JSC::PropertyName, unsigned int, JSC::JSCell*, int&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::addPropertyWithoutTransition(JSC::JSGlobalData&, JSC::PropertyName, unsigned int, JSC::JSCell*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::despecifyDictionaryFunction(JSC::JSGlobalData&, JSC::PropertyName)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::despecifyFunctionTransition(JSC::JSGlobalData&, JSC::Structure*, JSC::PropertyName)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::get(JSC::JSGlobalData&, JSC::PropertyName, unsigned int&, JSC::JSCell*&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::materializePropertyMap(JSC::JSGlobalData&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::s_classinfo()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::Structure(JSC::JSGlobalData&, JSC::JSGlobalObject*, JSC::JSValue, JSC::TypeInfo const&, JSC::ClassInfo const*, unsigned char, int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::Structure::suggestedNewOutOfLineStorageCapacity()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::throwError(JSC::ExecState*, JSC::JSObject*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::throwError(JSC::ExecState*, JSC::JSValue)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::throwTypeError(JSC::ExecState*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::throwTypeError(JSC::ExecState*, WTF::String const&)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::toInt32(double)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("JSC::WeakSet::findAllocator()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("OpaqueJSString::string", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::emptyString()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::equal(WTF::StringImpl const*, unsigned char const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::fastFree(void*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::fastMalloc(unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::fastZeroedMalloc(unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::OSAllocator::getCurrentBytesAllocated()", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::String::fromUTF8(unsigned char const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::String::latin1() const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::String::String(char const*)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::String::String(char const*, unsigned int)", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::String::utf8(WTF::String::ConversionMode) const", javascriptcore);
SYS_LIB_EXPORTPICKUP_CPP_FUNC("WTF::StringImpl::~StringImpl()", javascriptcore);

extern "C" int prx_start(size_t args, void* argp);
int prx_start(size_t args, void* argp) {
  return SYS_PRX_RESIDENT;
}

