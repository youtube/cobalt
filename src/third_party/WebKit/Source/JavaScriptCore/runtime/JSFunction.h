/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSFunction_h
#define JSFunction_h

#include "InternalFunction.h"
#include "JSDestructibleObject.h"
#include "JSScope.h"
#include "Watchpoint.h"

namespace JSC {

    class ExecutableBase;
    class FunctionExecutable;
    class FunctionPrototype;
    class JSActivation;
    class JSGlobalObject;
    class LLIntOffsetsExtractor;
    class NativeExecutable;
    class SourceCode;
    namespace DFG {
    class SpeculativeJIT;
    class JITCompiler;
    }

    JS_EXPORT_PRIVATE EncodedJSValue JSC_HOST_CALL callHostFunctionAsConstructor(ExecState*);

    JS_EXPORT_PRIVATE String getCalculatedDisplayName(CallFrame*, JSObject*);
    
    class JSFunction : public JSDestructibleObject {
        friend class JIT;
        friend class DFG::SpeculativeJIT;
        friend class DFG::JITCompiler;
        friend class JSGlobalData;

    public:
        typedef JSDestructibleObject Base;

        JS_EXPORT_PRIVATE static JSFunction* create(ExecState*, JSGlobalObject*, int length, const String& name, NativeFunction, Intrinsic = NoIntrinsic, NativeFunction nativeConstructor = callHostFunctionAsConstructor);

        static JSFunction* create(ExecState* exec, FunctionExecutable* executable, JSScope* scope)
        {
            JSGlobalData& globalData = exec->globalData();
            JSFunction* function = new (NotNull, allocateCell<JSFunction>(globalData.heap)) JSFunction(globalData, executable, scope);
            ASSERT(function->structure()->globalObject());
            function->finishCreation(globalData);
            return function;
        }
        
        static JS_EXPORT_PRIVATE void destroyInternal(JSCell*);
        static void destroy(JSCell* cell) {
            return destroyInternal(cell);
        }
        
        JS_EXPORT_PRIVATE String name(ExecState*);
        JS_EXPORT_PRIVATE String displayName(ExecState*);
        const String calculatedDisplayName(ExecState*);

        JSScope* scope()
        {
            ASSERT(!isHostFunctionNonInline());
            return m_scope.get();
        }
        // This method may be called for host functins, in which case it
        // will return an arbitrary value. This should only be used for
        // optimized paths in which the return value does not matter for
        // host functions, and checking whether the function is a host
        // function is deemed too expensive.
        JSScope* scopeUnchecked()
        {
            return m_scope.get();
        }
        void setScope(JSGlobalData& globalData, JSScope* scope)
        {
            ASSERT(!isHostFunctionNonInline());
            m_scope.set(globalData, this, scope);
        }

        ExecutableBase* executable() const { return m_executable.get(); }

        // To call either of these methods include Executable.h
        inline bool isHostFunction() const;
        FunctionExecutable* jsExecutable() const;

        JS_EXPORT_PRIVATE const SourceCode* sourceCode() const;

        DECLARE_EXPORTED_CLASSINFO();

        static Structure* createStructure(JSGlobalData& globalData, JSGlobalObject* globalObject, JSValue prototype) 
        {
            ASSERT(globalObject);
            return Structure::create(globalData, globalObject, prototype, TypeInfo(JSFunctionType, StructureFlags), &s_info); 
        }

        NativeFunction nativeFunction();
        NativeFunction nativeConstructor();

        static JS_EXPORT_PRIVATE ConstructType getConstructDataInternal(JSCell*, ConstructData&);
        static ConstructType getConstructData(JSCell* cell, ConstructData& data) {
            return getConstructDataInternal(cell, data);
        }

        static JS_EXPORT_PRIVATE CallType getCallDataInternal(JSCell*, CallData&);
        static CallType getCallData(JSCell* cell, CallData& data) {
            return getCallDataInternal(cell, data);
        }

        static inline size_t offsetOfScopeChain()
        {
            return OBJECT_OFFSETOF(JSFunction, m_scope);
        }

        static inline size_t offsetOfExecutable()
        {
            return OBJECT_OFFSETOF(JSFunction, m_executable);
        }

        Structure* cachedInheritorID(ExecState* exec)
        {
            if (UNLIKELY(!m_cachedInheritorID))
                return cacheInheritorID(exec);
            return m_cachedInheritorID.get();
        }

        Structure* tryGetKnownInheritorID()
        {
            if (!m_cachedInheritorID)
                return 0;
            if (m_inheritorIDWatchpoint.hasBeenInvalidated())
                return 0;
            return m_cachedInheritorID.get();
        }
        
        void addInheritorIDWatchpoint(Watchpoint* watchpoint)
        {
            ASSERT(tryGetKnownInheritorID());
            m_inheritorIDWatchpoint.add(watchpoint);
        }

        static size_t offsetOfCachedInheritorID()
        {
            return OBJECT_OFFSETOF(JSFunction, m_cachedInheritorID);
        }

    protected:
        const static unsigned StructureFlags = OverridesGetOwnPropertySlot | ImplementsHasInstance | OverridesVisitChildren | OverridesGetPropertyNames | JSObject::StructureFlags;

        JS_EXPORT_PRIVATE JSFunction(ExecState*, JSGlobalObject*, Structure*);
        JSFunction(JSGlobalData&, FunctionExecutable*, JSScope*);
        
        JS_EXPORT_PRIVATE void finishCreation(ExecState*, NativeExecutable*, int length, const String& name);
        using Base::finishCreation;

        Structure* cacheInheritorID(ExecState*);

        static bool getOwnPropertySlot(JSCell*, ExecState*, PropertyName, PropertySlot&);

        static JS_EXPORT_PRIVATE bool getOwnPropertyDescriptorInternal(JSObject*, ExecState*, PropertyName, PropertyDescriptor&);
        static  bool getOwnPropertyDescriptor(JSObject* object, ExecState* exec, PropertyName name, PropertyDescriptor& descriptor) {
            return getOwnPropertyDescriptorInternal(object, exec, name, descriptor);
        }

        static JS_EXPORT_PRIVATE void getOwnNonIndexPropertyNamesInternal(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode);
        static void getOwnNonIndexPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& nameArray, EnumerationMode mode = ExcludeDontEnumProperties) {
            return getOwnNonIndexPropertyNamesInternal(object, exec, nameArray, mode);
        }

        static JS_EXPORT_PRIVATE bool defineOwnPropertyInternal(JSObject*, ExecState*, PropertyName, PropertyDescriptor&, bool shouldThrow);
        static JS_EXPORT_PRIVATE bool defineOwnProperty(JSObject* object, ExecState* exec, PropertyName name, PropertyDescriptor& descriptor, bool shouldThrow) {
            return defineOwnPropertyInternal(object, exec, name, descriptor, shouldThrow);
        }

        static JS_EXPORT_PRIVATE void putInternal(JSCell*, ExecState*, PropertyName, JSValue, PutPropertySlot&);
        static JS_EXPORT_PRIVATE void put(JSCell* cell, ExecState* exec, PropertyName name, JSValue value, PutPropertySlot& slot) {
            return putInternal(cell, exec, name, value, slot);
        }

        static JS_EXPORT_PRIVATE bool deletePropertyInternal(JSCell*, ExecState*, PropertyName);
        static bool deleteProperty(JSCell* cell, ExecState* exec, PropertyName name) {
            return deletePropertyInternal(cell, exec, name);
        }

        static JS_EXPORT_PRIVATE void visitChildrenInternal(JSCell*, SlotVisitor&);
        static void visitChildren(JSCell* cell, SlotVisitor& sv) {
            return visitChildrenInternal(cell, sv);
        }

    private:
        friend class LLIntOffsetsExtractor;
        
        JS_EXPORT_PRIVATE bool isHostFunctionNonInline() const;

        static JSValue argumentsGetter(ExecState*, JSValue, PropertyName);
        static JSValue callerGetter(ExecState*, JSValue, PropertyName);
        static JSValue lengthGetter(ExecState*, JSValue, PropertyName);
        static JSValue nameGetter(ExecState*, JSValue, PropertyName);

        WriteBarrier<ExecutableBase> m_executable;
        WriteBarrier<JSScope> m_scope;
        WriteBarrier<Structure> m_cachedInheritorID;
        InlineWatchpointSet m_inheritorIDWatchpoint;
    };

    inline bool JSValue::isFunction() const
    {
        return isCell() && (asCell()->inherits(JSFunction::s_classinfo()) || asCell()->inherits(InternalFunction::s_classinfo()));
    }

} // namespace JSC

#endif // JSFunction_h
