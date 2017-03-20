/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * JS function support.
 */
#include "jsfun.h"

#include <string.h>

#include "mozilla/PodOperations.h"
#include "mozilla/Util.h"

#include "jstypes.h"
#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"
#include "jsproxy.h"
#include "jsscript.h"
#include "jsstr.h"

#include "builtin/Eval.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#include "vm/Interpreter.h"
#include "vm/Shape.h"
#include "vm/StringBuffer.h"
#include "vm/Xdr.h"

#include "jsfuninlines.h"
#include "jsinferinlines.h"
#include "jsscriptinlines.h"

#include "vm/Interpreter-inl.h"
#include "vm/Stack-inl.h"

#ifdef JS_ION
#include "jit/Ion.h"
#include "jit/IonFrameIterator.h"
#include "jit/IonFrameIterator-inl.h"
#endif

#include "nb/memory_scope.h"

using namespace js;
using namespace js::gc;
using namespace js::types;
using namespace js::frontend;

using mozilla::ArrayLength;
using mozilla::PodCopy;

static JSBool
fun_getProperty(JSContext *cx, HandleObject obj_, HandleId id, MutableHandleValue vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    RootedObject obj(cx, obj_);
    while (!obj->is<JSFunction>()) {
        if (!JSObject::getProto(cx, obj, &obj))
            return false;
        if (!obj)
            return true;
    }
    RootedFunction fun(cx, &obj->as<JSFunction>());

    /* Set to early to null in case of error */
    vp.setNull();

    /* Find fun's top-most activation record. */
    NonBuiltinScriptFrameIter iter(cx);
    for (; !iter.done(); ++iter) {
        if (!iter.isFunctionFrame() || iter.isEvalFrame())
            continue;
        if (iter.callee() == fun)
            break;
    }
    if (iter.done())
        return true;

    if (JSID_IS_ATOM(id, cx->names().arguments)) {
        if (fun->hasRest()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_FUNCTION_ARGUMENTS_AND_REST);
            return false;
        }
        /* Warn if strict about f.arguments or equivalent unqualified uses. */
        if (!JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING | JSREPORT_STRICT, js_GetErrorMessage,
                                          NULL, JSMSG_DEPRECATED_USAGE, js_arguments_str)) {
            return false;
        }

        ArgumentsObject *argsobj = ArgumentsObject::createUnexpected(cx, iter);
        if (!argsobj)
            return false;

#ifdef JS_ION
        // Disabling compiling of this script in IonMonkey.
        // IonMonkey does not guarantee |f.arguments| can be
        // fully recovered, so we try to mitigate observing this behavior by
        // detecting its use early.
        JSScript *script = iter.script();
        jit::ForbidCompilation(cx, script);
#endif

        vp.setObject(*argsobj);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().caller)) {
        ++iter;
        if (iter.done() || !iter.isFunctionFrame()) {
            JS_ASSERT(vp.isNull());
            return true;
        }

        /* Callsite clones should never escape to script. */
        JSObject &maybeClone = iter.calleev().toObject();
        if (maybeClone.is<JSFunction>() && maybeClone.as<JSFunction>().nonLazyScript()->isCallsiteClone)
            vp.setObject(*maybeClone.as<JSFunction>().nonLazyScript()->originalFunction());
        else
            vp.set(iter.calleev());

        if (!cx->compartment()->wrap(cx, vp))
            return false;

        /*
         * Censor the caller if we don't have full access to it.
         */
        RootedObject caller(cx, &vp.toObject());
        if (caller->isWrapper() && !Wrapper::wrapperHandler(caller)->isSafeToUnwrap()) {
            vp.setNull();
        } else if (caller->is<JSFunction>()) {
            JSFunction *callerFun = &caller->as<JSFunction>();
            if (callerFun->isInterpreted() && callerFun->strict()) {
                JS_ReportErrorFlagsAndNumber(cx, JSREPORT_ERROR, js_GetErrorMessage, NULL,
                                             JSMSG_CALLER_IS_STRICT);
                return false;
            }
        }

        return true;
    }

    JS_NOT_REACHED("fun_getProperty");
    return false;
}



/* NB: no sentinels at ends -- use ArrayLength to bound loops.
 * Properties censored into [[ThrowTypeError]] in strict mode. */
static const uint16_t poisonPillProps[] = {
    NAME_OFFSET(arguments),
    NAME_OFFSET(caller),
};

static JSBool
fun_enumerate(JSContext *cx, HandleObject obj)
{
    TRACK_MEMORY_SCOPE("Javascript");
    JS_ASSERT(obj->is<JSFunction>());

    RootedId id(cx);
    bool found;

    if (!obj->isBoundFunction()) {
        id = NameToId(cx->names().classPrototype);
        if (!JSObject::hasProperty(cx, obj, id, &found, 0))
            return false;
    }

    id = NameToId(cx->names().length);
    if (!JSObject::hasProperty(cx, obj, id, &found, 0))
        return false;

    id = NameToId(cx->names().name);
    if (!JSObject::hasProperty(cx, obj, id, &found, 0))
        return false;

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];
        id = NameToId(OFFSET_TO_NAME(cx->runtime(), offset));
        if (!JSObject::hasProperty(cx, obj, id, &found, 0))
            return false;
    }

    return true;
}

static JSObject *
ResolveInterpretedFunctionPrototype(JSContext *cx, HandleObject obj)
{
    TRACK_MEMORY_SCOPE("Javascript");
#ifdef DEBUG
    JSFunction *fun = &obj->as<JSFunction>();
    JS_ASSERT(fun->isInterpreted());
    JS_ASSERT(!fun->isFunctionPrototype());
#endif

    /*
     * Assert that fun is not a compiler-created function object, which
     * must never leak to script or embedding code and then be mutated.
     * Also assert that obj is not bound, per the ES5 15.3.4.5 ref above.
     */
    JS_ASSERT(!IsInternalFunctionObject(obj));
    JS_ASSERT(!obj->isBoundFunction());

    /*
     * Make the prototype object an instance of Object with the same parent
     * as the function object itself.
     */
    JSObject *objProto = obj->global().getOrCreateObjectPrototype(cx);
    if (!objProto)
        return NULL;
    RootedObject proto(cx, NewObjectWithGivenProto(cx, &ObjectClass, objProto, NULL, SingletonObject));
    if (!proto)
        return NULL;

    /*
     * Per ES5 15.3.5.2 a user-defined function's .prototype property is
     * initially non-configurable, non-enumerable, and writable.  Per ES5 13.2
     * the prototype's .constructor property is configurable, non-enumerable,
     * and writable.
     */
    RootedValue protoVal(cx, ObjectValue(*proto));
    RootedValue objVal(cx, ObjectValue(*obj));
    if (!JSObject::defineProperty(cx, obj, cx->names().classPrototype,
                                  protoVal, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT) ||
        !JSObject::defineProperty(cx, proto, cx->names().constructor,
                                  objVal, JS_PropertyStub, JS_StrictPropertyStub, 0))
    {
       return NULL;
    }

    return proto;
}

static JSBool
fun_resolve(JSContext *cx, HandleObject obj, HandleId id, unsigned flags,
            MutableHandleObject objp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    if (!JSID_IS_ATOM(id))
        return true;

    RootedFunction fun(cx, &obj->as<JSFunction>());

    if (JSID_IS_ATOM(id, cx->names().classPrototype)) {
        /*
         * Built-in functions do not have a .prototype property per ECMA-262,
         * or (Object.prototype, Function.prototype, etc.) have that property
         * created eagerly.
         *
         * ES5 15.3.4: the non-native function object named Function.prototype
         * does not have a .prototype property.
         *
         * ES5 15.3.4.5: bound functions don't have a prototype property. The
         * isBuiltin() test covers this case because bound functions are native
         * (and thus built-in) functions by definition/construction.
         */
        if (fun->isBuiltin() || fun->isFunctionPrototype())
            return true;

        if (!ResolveInterpretedFunctionPrototype(cx, fun))
            return false;
        objp.set(fun);
        return true;
    }

    if (JSID_IS_ATOM(id, cx->names().length) || JSID_IS_ATOM(id, cx->names().name)) {
        JS_ASSERT(!IsInternalFunctionObject(obj));

        RootedValue v(cx);
        if (JSID_IS_ATOM(id, cx->names().length)) {
            if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
                return false;
            uint16_t ndefaults = fun->hasScript() ? fun->nonLazyScript()->ndefaults : 0;
            v.setInt32(fun->nargs - ndefaults - fun->hasRest());
        } else {
            v.setString(fun->atom() == NULL ?  cx->runtime()->emptyString : fun->atom());
        }

        if (!DefineNativeProperty(cx, fun, id, v, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_PERMANENT | JSPROP_READONLY, 0, 0)) {
            return false;
        }
        objp.set(fun);
        return true;
    }

    for (unsigned i = 0; i < ArrayLength(poisonPillProps); i++) {
        const uint16_t offset = poisonPillProps[i];

        if (JSID_IS_ATOM(id, OFFSET_TO_NAME(cx->runtime(), offset))) {
            JS_ASSERT(!IsInternalFunctionObject(fun));

            PropertyOp getter;
            StrictPropertyOp setter;
            unsigned attrs = JSPROP_PERMANENT;
            if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
                return false;
            if (fun->isInterpreted() ? fun->strict() : fun->isBoundFunction()) {
                JSObject *throwTypeError = fun->global().getThrowTypeError();

                getter = CastAsPropertyOp(throwTypeError);
                setter = CastAsStrictPropertyOp(throwTypeError);
                attrs |= JSPROP_GETTER | JSPROP_SETTER;
            } else {
                getter = fun_getProperty;
                setter = JS_StrictPropertyStub;
            }

            RootedValue value(cx, UndefinedValue());
            if (!DefineNativeProperty(cx, fun, id, value, getter, setter,
                                      attrs, 0, 0)) {
                return false;
            }
            objp.set(fun);
            return true;
        }
    }

    return true;
}

template<XDRMode mode>
bool
js::XDRInterpretedFunction(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
                           MutableHandleObject objp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    /* NB: Keep this in sync with CloneFunctionAndScript. */
    RootedAtom atom(xdr->cx());
    uint32_t firstword;           /* flag telling whether fun->atom is non-null,
                                   plus for fun->u.i.skipmin, fun->u.i.wrapper,
                                   and 14 bits reserved for future use */
    uint32_t flagsword;           /* word for argument count and fun->flags */

    JSContext *cx = xdr->cx();
    RootedFunction fun(cx);
    RootedScript script(cx);
    if (mode == XDR_ENCODE) {
        fun = &objp->as<JSFunction>();
        if (!fun->isInterpreted()) {
            JSAutoByteString funNameBytes;
            if (const char *name = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
                JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_NOT_SCRIPTED_FUNCTION,
                                     name);
            }
            return false;
        }
        firstword = !!fun->atom();
        script = fun->getOrCreateScript(cx);
        if (!script)
            return false;
        atom = fun->atom();
        flagsword = (fun->nargs << 16) | fun->flags;
    } else {
        fun = NewFunction(cx, NullPtr(), NULL, 0, JSFunction::INTERPRETED, NullPtr(), NullPtr(),
                          JSFunction::FinalizeKind, TenuredObject);
        if (!fun)
            return false;
        atom = NULL;
        script = NULL;
    }

    if (!xdr->codeUint32(&firstword))
        return false;
    if ((firstword & 1U) && !XDRAtom(xdr, &atom))
        return false;
    if (!xdr->codeUint32(&flagsword))
        return false;

    if (!XDRScript(xdr, enclosingScope, enclosingScript, fun, &script))
        return false;

    if (mode == XDR_DECODE) {
        fun->nargs = flagsword >> 16;
        fun->flags = uint16_t(flagsword);
        fun->initAtom(atom);
        fun->initScript(script);
        script->setFunction(fun);
        if (!JSFunction::setTypeForScriptedFunction(cx, fun))
            return false;
        JS_ASSERT(fun->nargs == fun->nonLazyScript()->bindings.numArgs());
        RootedScript script(cx, fun->nonLazyScript());
        CallNewScriptHook(cx, script, fun);
        objp.set(fun);
    }

    return true;
}

template bool
js::XDRInterpretedFunction(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, MutableHandleObject);

template bool
js::XDRInterpretedFunction(XDRState<XDR_DECODE> *, HandleObject, HandleScript, MutableHandleObject);

JSObject *
js::CloneFunctionAndScript(JSContext *cx, HandleObject enclosingScope, HandleFunction srcFun)
{
    /* NB: Keep this in sync with XDRInterpretedFunction. */

    RootedFunction clone(cx, NewFunction(cx, NullPtr(), NULL, 0,
                                         JSFunction::INTERPRETED, NullPtr(), NullPtr(),
                                         JSFunction::FinalizeKind, TenuredObject));
    if (!clone)
        return NULL;

    RootedScript srcScript(cx, srcFun->nonLazyScript());
    RootedScript clonedScript(cx, CloneScript(cx, enclosingScope, clone, srcScript));
    if (!clonedScript)
        return NULL;

    clone->nargs = srcFun->nargs;
    clone->flags = srcFun->flags;
    clone->initAtom(srcFun->displayAtom());
    clone->initScript(clonedScript);
    clonedScript->setFunction(clone);
    if (!JSFunction::setTypeForScriptedFunction(cx, clone))
        return NULL;

    RootedScript cloneScript(cx, clone->nonLazyScript());
    CallNewScriptHook(cx, cloneScript, clone);
    return clone;
}

/*
 * [[HasInstance]] internal method for Function objects: fetch the .prototype
 * property of its 'this' parameter, and walks the prototype chain of v (only
 * if v is an object) returning true if .prototype is found.
 */
static JSBool
fun_hasInstance(JSContext *cx, HandleObject objArg, MutableHandleValue v, JSBool *bp)
{
    RootedObject obj(cx, objArg);

    while (obj->is<JSFunction>() && obj->isBoundFunction())
        obj = obj->as<JSFunction>().getBoundFunctionTarget();

    RootedValue pval(cx);
    if (!JSObject::getProperty(cx, obj, obj, cx->names().classPrototype, &pval))
        return false;

    if (pval.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        RootedValue val(cx, ObjectValue(*obj));
        js_ReportValueError(cx, JSMSG_BAD_PROTOTYPE, -1, val, NullPtr());
        return false;
    }

    RootedObject pobj(cx, &pval.toObject());
    bool isDelegate;
    if (!IsDelegate(cx, pobj, v, &isDelegate))
        return false;
    *bp = isDelegate;
    return true;
}

inline void
JSFunction::trace(JSTracer *trc)
{
    if (isExtended()) {
        MarkValueRange(trc, ArrayLength(toExtended()->extendedSlots),
                       toExtended()->extendedSlots, "nativeReserved");
    }

    if (atom_)
        MarkString(trc, &atom_, "atom");

    if (isInterpreted()) {
        // Functions can be be marked as interpreted despite having no script
        // yet at some points when parsing, and can be lazy with no lazy script
        // for self hosted code.
        if (hasScript() && u.i.s.script_)
            MarkScriptUnbarriered(trc, &u.i.s.script_, "script");
        else if (isInterpretedLazy() && u.i.s.lazy_)
            MarkLazyScriptUnbarriered(trc, &u.i.s.lazy_, "lazyScript");
        if (u.i.env_)
            MarkObjectUnbarriered(trc, &u.i.env_, "fun_callscope");
    }
}

static void
fun_trace(JSTracer *trc, JSObject *obj)
{
    obj->as<JSFunction>().trace(trc);
}

Class JSFunction::class_ = {
    js_Function_str,
    JSCLASS_NEW_RESOLVE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Function),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    fun_enumerate,
    (JSResolveOp)fun_resolve,
    JS_ConvertStub,
    NULL,                    /* finalize    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    fun_hasInstance,
    NULL,                    /* construct   */
    fun_trace
};

JS_FRIEND_DATA(Class*) js::FunctionClassPtr = &JSFunction::class_;

/* Find the body of a function (not including braces). */
static bool
FindBody(JSContext *cx, HandleFunction fun, StableCharPtr chars, size_t length,
         size_t *bodyStart, size_t *bodyEnd)
{
    // We don't need principals, since those are only used for error reporting.
    CompileOptions options(cx);
    options.setFileAndLine("internal-findBody", 0)
           .setVersion(fun->nonLazyScript()->getVersion());
    AutoKeepAtoms keepAtoms(cx->runtime());
    TokenStream ts(cx, options, chars.get(), length, NULL, keepAtoms);
    int nest = 0;
    bool onward = true;
    // Skip arguments list.
    do {
        switch (ts.getToken()) {
          case TOK_NAME:
            if (nest == 0)
                onward = false;
            break;
          case TOK_LP:
            nest++;
            break;
          case TOK_RP:
            if (--nest == 0)
                onward = false;
            break;
          case TOK_ERROR:
            // Must be memory.
            return false;
          default:
            break;
        }
    } while (onward);
    TokenKind tt = ts.getToken();
    if (tt == TOK_ARROW)
        tt = ts.getToken();
    if (tt == TOK_ERROR)
        return false;
    bool braced = tt == TOK_LC;
    JS_ASSERT_IF(fun->isExprClosure(), !braced);
    *bodyStart = ts.currentToken().pos.begin;
    if (braced)
        *bodyStart += 1;
    StableCharPtr end(chars.get() + length, chars.get(), length);
    if (end[-1] == '}') {
        end--;
    } else {
        JS_ASSERT(!braced);
        for (; unicode::IsSpaceOrBOM2(end[-1]); end--)
            ;
    }
    *bodyEnd = end - chars;
    JS_ASSERT(*bodyStart <= *bodyEnd);
    return true;
}

JSString *
js::FunctionToString(JSContext *cx, HandleFunction fun, bool bodyOnly, bool lambdaParen)
{
    TRACK_MEMORY_SCOPE("Javascript");
    if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return NULL;

    // If the object is an automatically-bound arrow function, get the source
    // of the pre-binding target.
    if (fun->isArrow() && fun->isBoundFunction()) {
        JSObject *target = fun->getBoundFunctionTarget();
        RootedFunction targetFun(cx, &target->as<JSFunction>());
        JS_ASSERT(targetFun->isArrow());
        return FunctionToString(cx, targetFun, bodyOnly, lambdaParen);
    }

    StringBuffer out(cx);
    RootedScript script(cx);

    if (fun->hasScript()) {
        script = fun->nonLazyScript();
        if (script->isGeneratorExp) {
            if ((!bodyOnly && !out.append("function genexp() {")) ||
                !out.append("\n    [generator expression]\n") ||
                (!bodyOnly && !out.append("}")))
            {
                return NULL;
            }
            return out.finishString();
        }
    }
    if (!bodyOnly) {
        // If we're not in pretty mode, put parentheses around lambda functions.
        if (fun->isInterpreted() && !lambdaParen && fun->isLambda() && !fun->isArrow()) {
            if (!out.append("("))
                return NULL;
        }
        if (!fun->isArrow()) {
            if (!out.append("function "))
                return NULL;
        }
        if (fun->atom()) {
            if (!out.append(fun->atom()))
                return NULL;
        }
    }
    bool haveSource = fun->isInterpreted() && !fun->isSelfHostedBuiltin();
    if (haveSource && !script->scriptSource()->hasSourceData() &&
        !JSScript::loadSource(cx, script, &haveSource))
    {
        return NULL;
    }
    if (haveSource) {
        RootedString srcStr(cx, script->sourceData(cx));
        if (!srcStr)
            return NULL;
        Rooted<JSStableString *> src(cx, srcStr->ensureStable(cx));
        if (!src)
            return NULL;

        StableCharPtr chars = src->chars();
        bool exprBody = fun->isExprClosure();
        // The source data for functions created by calling the Function
        // constructor is only the function's body.
        bool funCon = script->sourceStart == 0 && script->scriptSource()->argumentsNotIncluded();

        // Functions created with the constructor should not be using the
        // expression body extension.
        JS_ASSERT_IF(funCon, !exprBody);
        JS_ASSERT_IF(!funCon && !fun->isArrow(), src->length() > 0 && chars[0] == '(');

        // If a function inherits strict mode by having scopes above it that
        // have "use strict", we insert "use strict" into the body of the
        // function. This ensures that if the result of toString is evaled, the
        // resulting function will have the same semantics.
        bool addUseStrict = script->strict && !script->explicitUseStrict && !fun->isArrow();

        bool buildBody = funCon && !bodyOnly;
        if (buildBody) {
            // This function was created with the Function constructor. We don't
            // have source for the arguments, so we have to generate that. Part
            // of bug 755821 should be cobbling the arguments passed into the
            // Function constructor into the source string.
            if (!out.append("("))
                return NULL;

            // Fish out the argument names.
            BindingVector *localNames = cx->new_<BindingVector>(cx);
            ScopedJSDeletePtr<BindingVector> freeNames(localNames);
            if (!FillBindingVector(script, localNames))
                return NULL;
            for (unsigned i = 0; i < fun->nargs; i++) {
                if ((i && !out.append(", ")) ||
                    (i == unsigned(fun->nargs - 1) && fun->hasRest() && !out.append("...")) ||
                    !out.append((*localNames)[i].name())) {
                    return NULL;
                }
            }
            if (!out.append(") {\n"))
                return NULL;
        }
        if ((bodyOnly && !funCon) || addUseStrict) {
            // We need to get at the body either because we're only supposed to
            // return the body or we need to insert "use strict" into the body.
            size_t bodyStart = 0, bodyEnd;

            // If the function is defined in the Function constructor, we
            // already have a body.
            if (!funCon) {
                JS_ASSERT(!buildBody);
                if (!FindBody(cx, fun, chars, src->length(), &bodyStart, &bodyEnd))
                    return NULL;
            } else {
                bodyEnd = src->length();
            }

            if (addUseStrict) {
                // Output source up to beginning of body.
                if (!out.append(chars, bodyStart))
                    return NULL;
                if (exprBody) {
                    // We can't insert a statement into a function with an
                    // expression body. Do what the decompiler did, and insert a
                    // comment.
                    if (!out.append("/* use strict */ "))
                        return NULL;
                } else {
                    if (!out.append("\n\"use strict\";\n"))
                        return NULL;
                }
            }

            // Output just the body (for bodyOnly) or the body and possibly
            // closing braces (for addUseStrict).
            size_t dependentEnd = bodyOnly ? bodyEnd : src->length();
            if (!out.append(chars + bodyStart, dependentEnd - bodyStart))
                return NULL;
        } else {
            if (!out.append(src))
                return NULL;
        }
        if (buildBody) {
            if (!out.append("\n}"))
                return NULL;
        }
        if (bodyOnly) {
            // Slap a semicolon on the end of functions with an expression body.
            if (exprBody && !out.append(";"))
                return NULL;
        } else if (!lambdaParen && fun->isLambda() && !fun->isArrow()) {
            if (!out.append(")"))
                return NULL;
        }
    } else if (fun->isInterpreted() && !fun->isSelfHostedBuiltin()) {
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[sourceless code]") ||
            (!bodyOnly && !out.append("\n}")))
            return NULL;
        if (!lambdaParen && fun->isLambda() && !fun->isArrow() && !out.append(")"))
            return NULL;
    } else {
        JS_ASSERT(!fun->isExprClosure());
        if ((!bodyOnly && !out.append("() {\n    ")) ||
            !out.append("[native code]") ||
            (!bodyOnly && !out.append("\n}")))
            return NULL;
    }
    return out.finishString();
}

JSString *
fun_toStringHelper(JSContext *cx, HandleObject obj, unsigned indent)
{
    TRACK_MEMORY_SCOPE("Javascript");
    if (!obj->is<JSFunction>()) {
        if (IsFunctionProxy(obj))
            return Proxy::fun_toString(cx, obj, indent);
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                             JSMSG_INCOMPATIBLE_PROTO,
                             js_Function_str, js_toString_str,
                             "object");
        return NULL;
    }

    RootedFunction fun(cx, &obj->as<JSFunction>());
    return FunctionToString(cx, fun, false, indent != JS_DONT_PRETTY_PRINT);
}

static JSBool
fun_toString(JSContext *cx, unsigned argc, Value *vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(IsFunctionObject(args.calleev()));

    uint32_t indent = 0;

    if (args.length() != 0 && !ToUint32(cx, args[0], &indent))
        return false;

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedString str(cx, fun_toStringHelper(cx, obj, indent));
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

#if JS_HAS_TOSOURCE
static JSBool
fun_toSource(JSContext *cx, unsigned argc, Value *vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    CallArgs args = CallArgsFromVp(argc, vp);
    JS_ASSERT(IsFunctionObject(args.calleev()));

    RootedObject obj(cx, ToObject(cx, args.thisv()));
    if (!obj)
        return false;

    RootedString str(cx, fun_toStringHelper(cx, obj, JS_DONT_PRETTY_PRINT));
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}
#endif

JSBool
js_fun_call(JSContext *cx, unsigned argc, Value *vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    RootedValue fval(cx, vp[1]);

    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, CallReceiverFromVp(vp), &JSFunction::class_);
        return false;
    }

    Value *argv = vp + 2;
    RootedValue thisv(cx, UndefinedValue());
    if (argc != 0) {
        thisv = argv[0];

        argc--;
        argv++;
    }

    /* Allocate stack space for fval, obj, and the args. */
    InvokeArgs args(cx);
    if (!args.init(argc))
        return JS_FALSE;

    /* Push fval, thisv, and the args. */
    args.setCallee(fval);
    args.setThis(thisv);
    PodCopy(args.array(), argv, argc);

    bool ok = Invoke(cx, args);
    *vp = args.rval();
    return ok;
}

#ifdef JS_ION
static bool
PushBaselineFunApplyArguments(JSContext *cx, jit::IonFrameIterator &frame, InvokeArgs &args,
                              Value *vp)
{
    unsigned length = frame.numActualArgs();
    JS_ASSERT(length <= ARGS_LENGTH_MAX);

    if (!args.init(length))
        return false;

    /* Push fval, obj, and aobj's elements as args. */
    args.setCallee(vp[1]);
    args.setThis(vp[2]);

    /* Steps 7-8. */
    frame.forEachCanonicalActualArg(CopyTo(args.array()), 0, -1);
    return true;
}
#endif

/* ES5 15.3.4.3 */
JSBool
js_fun_apply(JSContext *cx, unsigned argc, Value *vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    /* Step 1. */
    RootedValue fval(cx, vp[1]);
    if (!js_IsCallable(fval)) {
        ReportIncompatibleMethod(cx, CallReceiverFromVp(vp), &JSFunction::class_);
        return false;
    }

    /* Step 2. */
    if (argc < 2 || vp[3].isNullOrUndefined())
        return js_fun_call(cx, (argc > 0) ? 1 : 0, vp);

    InvokeArgs args(cx);

    /*
     * GuardFunApplyArgumentsOptimization already called IsOptimizedArguments,
     * so we don't need to here. This is not an optimization: we can't rely on
     * cx->fp (since natives can be called directly from JSAPI).
     */
    if (vp[3].isMagic(JS_OPTIMIZED_ARGUMENTS)) {
        /*
         * Pretend we have been passed the 'arguments' object for the current
         * function and read actuals out of the frame.
         */
        /* Steps 4-6. */

#ifdef JS_ION
        // We do not want to use ScriptFrameIter to abstract here because this
        // is supposed to be a fast path as opposed to ScriptFrameIter which is
        // doing complex logic to settle on the next frame twice.
        if (cx->currentlyRunningInJit()) {
            jit::JitActivationIterator activations(cx->runtime());
            jit::IonFrameIterator frame(activations);
            if (frame.isNative()) {
                // Stop on the next Ion JS Frame.
                ++frame;
                if (frame.isOptimizedJS()) {
                    jit::InlineFrameIterator iter(cx, &frame);

                    unsigned length = iter.numActualArgs();
                    JS_ASSERT(length <= ARGS_LENGTH_MAX);

                    if (!args.init(length))
                        return false;

                    /* Push fval, obj, and aobj's elements as args. */
                    args.setCallee(fval);
                    args.setThis(vp[2]);

                    /* Steps 7-8. */
                    iter.forEachCanonicalActualArg(cx, CopyTo(args.array()), 0, -1);
                } else {
                    JS_ASSERT(frame.isBaselineStub());

                    ++frame;
                    JS_ASSERT(frame.isBaselineJS());

                    if (!PushBaselineFunApplyArguments(cx, frame, args, vp))
                        return false;
                }
            } else {
                JS_ASSERT(frame.type() == jit::IonFrame_Exit);

                ++frame;
                JS_ASSERT(frame.isBaselineStub());

                ++frame;
                JS_ASSERT(frame.isBaselineJS());

                if (!PushBaselineFunApplyArguments(cx, frame, args, vp))
                    return false;
            }
        } else
#endif
        {
            StackFrame *fp = cx->interpreterFrame();
            unsigned length = fp->numActualArgs();
            JS_ASSERT(length <= ARGS_LENGTH_MAX);

            if (!args.init(length))
                return false;

            /* Push fval, obj, and aobj's elements as args. */
            args.setCallee(fval);
            args.setThis(vp[2]);

            /* Steps 7-8. */
            fp->forEachUnaliasedActual(CopyTo(args.array()));
        }
    } else {
        /* Step 3. */
        if (!vp[3].isObject()) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_APPLY_ARGS, js_apply_str);
            return false;
        }

        /*
         * Steps 4-5 (note erratum removing steps originally numbered 5 and 7 in
         * original version of ES5).
         */
        RootedObject aobj(cx, &vp[3].toObject());
        uint32_t length;
        if (!GetLengthProperty(cx, aobj, &length))
            return false;

        /* Step 6. */
        if (length > ARGS_LENGTH_MAX) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_TOO_MANY_FUN_APPLY_ARGS);
            return false;
        }

        if (!args.init(length))
            return false;

        /* Push fval, obj, and aobj's elements as args. */
        args.setCallee(fval);
        args.setThis(vp[2]);

        /* Steps 7-8. */
        if (!GetElements(cx, aobj, length, args.array()))
            return false;
    }

    /* Step 9. */
    if (!Invoke(cx, args))
        return false;

    *vp = args.rval();
    return true;
}

static const uint32_t JSSLOT_BOUND_FUNCTION_THIS       = 0;
static const uint32_t JSSLOT_BOUND_FUNCTION_ARGS_COUNT = 1;

static const uint32_t BOUND_FUNCTION_RESERVED_SLOTS = 2;

inline bool
JSFunction::initBoundFunction(JSContext *cx, HandleValue thisArg,
                              const Value *args, unsigned argslen)
{
    RootedFunction self(cx, this);

    /*
     * Convert to a dictionary to set the BOUND_FUNCTION flag and increase
     * the slot span to cover the arguments and additional slots for the 'this'
     * value and arguments count.
     */
    if (!self->toDictionaryMode(cx))
        return false;

    if (!self->setFlag(cx, BaseShape::BOUND_FUNCTION))
        return false;

    if (!JSObject::setSlotSpan(cx, self, BOUND_FUNCTION_RESERVED_SLOTS + argslen))
        return false;

    self->setSlot(JSSLOT_BOUND_FUNCTION_THIS, thisArg);
    self->setSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT, PrivateUint32Value(argslen));

    self->initSlotRange(BOUND_FUNCTION_RESERVED_SLOTS, args, argslen);

    return true;
}

inline const js::Value &
JSFunction::getBoundFunctionThis() const
{
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_THIS);
}

inline const js::Value &
JSFunction::getBoundFunctionArgument(unsigned which) const
{
    JS_ASSERT(isBoundFunction());
    JS_ASSERT(which < getBoundFunctionArgumentCount());

    return getSlot(BOUND_FUNCTION_RESERVED_SLOTS + which);
}

inline size_t
JSFunction::getBoundFunctionArgumentCount() const
{
    JS_ASSERT(isBoundFunction());

    return getSlot(JSSLOT_BOUND_FUNCTION_ARGS_COUNT).toPrivateUint32();
}

/* static */ bool
JSFunction::createScriptForLazilyInterpretedFunction(JSContext *cx, HandleFunction fun)
{
    TRACK_MEMORY_SCOPE("Javascript");
    JS_ASSERT(fun->isInterpretedLazy());

    LazyScript *lazy = fun->lazyScriptOrNull();
    if (lazy) {
        // Trigger a pre barrier on the lazy script being overwritten.
        if (cx->zone()->needsBarrier())
            LazyScript::writeBarrierPre(lazy);

        // Suppress GC when lazily compiling functions, to preserve source
        // character buffers.
        AutoSuppressGC suppressGC(cx);

        fun->flags &= ~INTERPRETED_LAZY;
        fun->flags |= INTERPRETED;

        JSScript *script = lazy->maybeScript();

        if (script) {
            fun->initScript(script);
            return true;
        }

        fun->initScript(NULL);

        if (fun != lazy->function()) {
            script = lazy->function()->getOrCreateScript(cx);
            if (!script) {
                fun->initLazyScript(lazy);
                return false;
            }
            fun->initScript(script);
            return true;
        }

        // Lazy script caching is only supported for leaf functions. If a
        // script with inner functions was returned by the cache, those inner
        // functions would be delazified when deep cloning the script, even if
        // they have never executed.
        //
        // Additionally, the lazy script cache is not used during incremental
        // GCs, to avoid resurrecting dead scripts after incremental sweeping
        // has started.
        if (!lazy->numInnerFunctions() && !JS::IsIncrementalGCInProgress(cx->runtime())) {
            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->runtime()->lazyScriptCache.lookup(lookup, &script);
        }

        if (script) {
            RootedObject enclosingScope(cx, lazy->enclosingScope());
            RootedScript scriptRoot(cx, script);
            RootedScript clonedScript(cx, CloneScript(cx, enclosingScope, fun, scriptRoot));
            if (!clonedScript) {
                fun->initLazyScript(lazy);
                return false;
            }

            // The cloned script will have reused the origin principals and
            // filename from the original script, which may differ.
            clonedScript->originPrincipals = lazy->originPrincipals();
            clonedScript->setSourceObject(lazy->sourceObject());

            fun->initAtom(script->function()->displayAtom());
            fun->initScript(clonedScript);
            clonedScript->setFunction(fun);

            CallNewScriptHook(cx, clonedScript, fun);

            lazy->initScript(clonedScript);
            return true;
        }

        JS_ASSERT(lazy->source()->hasSourceData());

        // Parse and compile the script from source.
        const jschar *chars = lazy->source()->chars(cx);
        if (!chars) {
            fun->initLazyScript(lazy);
            return false;
        }

        const jschar *lazyStart = chars + lazy->begin();
        size_t lazyLength = lazy->end() - lazy->begin();

        if (!frontend::CompileLazyFunction(cx, lazy, lazyStart, lazyLength)) {
            fun->initLazyScript(lazy);
            return false;
        }

        script = fun->nonLazyScript();

        // Try to insert the newly compiled script into the lazy script cache.
        if (!lazy->numInnerFunctions()) {
            // A script's starting column isn't set by the bytecode emitter, so
            // specify this from the lazy script so that if an identical lazy
            // script is encountered later a match can be determined.
            script->column = lazy->column();

            LazyScriptCache::Lookup lookup(cx, lazy);
            cx->runtime()->lazyScriptCache.insert(lookup, script);
        }

        // Remember the compiled script on the lazy script itself, in case
        // there are clones of the function still pointing to the lazy script.
        lazy->initScript(script);
        return true;
    }

    /* Lazily cloned self hosted script. */
    JSFunctionSpec *fs = static_cast<JSFunctionSpec *>(fun->getExtendedSlot(0).toPrivate());
    RootedAtom funAtom(cx, Atomize(cx, fs->selfHostedName, strlen(fs->selfHostedName)));
    if (!funAtom)
        return false;
    Rooted<PropertyName *> funName(cx, funAtom->asPropertyName());
    return cx->runtime()->cloneSelfHostedFunctionScript(cx, funName, fun);
}

/* ES5 15.3.4.5.1 and 15.3.4.5.2. */
JSBool
js::CallOrConstructBoundFunction(JSContext *cx, unsigned argc, Value *vp)
{
    RootedFunction fun(cx, &vp[0].toObject().as<JSFunction>());
    JS_ASSERT(fun->isBoundFunction());

    bool constructing = IsConstructing(vp);
    if (constructing && fun->isArrow()) {
        /*
         * Per spec, arrow functions do not even have a [[Construct]] method.
         * So before anything else, if we are an arrow function, make sure we
         * don't even get here. You never saw me. Burn this comment.
         */
        return ReportIsNotFunction(cx, ObjectValue(*fun), -1, CONSTRUCT);
    }

    /* 15.3.4.5.1 step 1, 15.3.4.5.2 step 3. */
    unsigned argslen = fun->getBoundFunctionArgumentCount();

    if (argc + argslen > ARGS_LENGTH_MAX) {
        js_ReportAllocationOverflow(cx);
        return false;
    }

    /* 15.3.4.5.1 step 3, 15.3.4.5.2 step 1. */
    RootedObject target(cx, fun->getBoundFunctionTarget());

    /* 15.3.4.5.1 step 2. */
    const Value &boundThis = fun->getBoundFunctionThis();

    InvokeArgs args(cx);
    if (!args.init(argc + argslen))
        return false;

    /* 15.3.4.5.1, 15.3.4.5.2 step 4. */
    for (unsigned i = 0; i < argslen; i++)
        args[i] = fun->getBoundFunctionArgument(i);
    PodCopy(args.array() + argslen, vp + 2, argc);

    /* 15.3.4.5.1, 15.3.4.5.2 step 5. */
    args.setCallee(ObjectValue(*target));

    if (!constructing)
        args.setThis(boundThis);

    if (constructing ? !InvokeConstructor(cx, args) : !Invoke(cx, args))
        return false;

    *vp = args.rval();
    return true;
}

#if JS_HAS_GENERATORS
static JSBool
fun_isGenerator(JSContext *cx, unsigned argc, Value *vp)
{
    JSFunction *fun;
    if (!IsFunctionObject(vp[1], &fun)) {
        JS_SET_RVAL(cx, vp, BooleanValue(false));
        return true;
    }

    bool result = false;
    if (fun->hasScript()) {
        JSScript *script = fun->nonLazyScript();
        JS_ASSERT(script->length != 0);
        result = script->isGenerator;
    }

    JS_SET_RVAL(cx, vp, BooleanValue(result));
    return true;
}
#endif

/* ES5 15.3.4.5. */
static JSBool
fun_bind(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    Value thisv = args.thisv();

    /* Step 2. */
    if (!js_IsCallable(thisv)) {
        ReportIncompatibleMethod(cx, args, &JSFunction::class_);
        return false;
    }

    /* Step 3. */
    Value *boundArgs = NULL;
    unsigned argslen = 0;
    if (args.length() > 1) {
        boundArgs = args.array() + 1;
        argslen = args.length() - 1;
    }

    /* Steps 7-9. */
    RootedValue thisArg(cx, args.length() >= 1 ? args[0] : UndefinedValue());
    RootedObject target(cx, &thisv.toObject());
    JSObject *boundFunction = js_fun_bind(cx, target, thisArg, boundArgs, argslen);
    if (!boundFunction)
        return false;

    /* Step 22. */
    args.rval().setObject(*boundFunction);
    return true;
}

JSObject*
js_fun_bind(JSContext *cx, HandleObject target, HandleValue thisArg,
            Value *boundArgs, unsigned argslen)
{
    /* Steps 15-16. */
    unsigned length = 0;
    if (target->is<JSFunction>()) {
        unsigned nargs = target->as<JSFunction>().nargs;
        if (nargs > argslen)
            length = nargs - argslen;
    }

    /* Step 4-6, 10-11. */
    RootedAtom name(cx, target->is<JSFunction>() ? target->as<JSFunction>().atom() : NULL);

    RootedObject funobj(cx, NewFunction(cx, NullPtr(), CallOrConstructBoundFunction, length,
                                        JSFunction::NATIVE_CTOR, target, name));
    if (!funobj)
        return NULL;

    /* NB: Bound functions abuse |parent| to store their target. */
    if (!JSObject::setParent(cx, funobj, target))
        return NULL;

    if (!funobj->as<JSFunction>().initBoundFunction(cx, thisArg, boundArgs, argslen))
        return NULL;

    /* Steps 17, 19-21 are handled by fun_resolve. */
    /* Step 18 is the default for new functions. */
    return funobj;
}

/*
 * Report "malformed formal parameter" iff no illegal char or similar scanner
 * error was already reported.
 */
static bool
OnBadFormal(JSContext *cx, TokenKind tt)
{
    if (tt != TOK_ERROR)
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_BAD_FORMAL);
    else
        JS_ASSERT(cx->isExceptionPending());
    return false;
}

const JSFunctionSpec js::function_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,   fun_toSource,   0,0),
#endif
    JS_FN(js_toString_str,   fun_toString,   0,0),
    JS_FN(js_apply_str,      js_fun_apply,   2,0),
    JS_FN(js_call_str,       js_fun_call,    1,0),
    JS_FN("bind",            fun_bind,       1,0),
#if JS_HAS_GENERATORS
    JS_FN("isGenerator",     fun_isGenerator,0,0),
#endif
    JS_FS_END
};

JSBool
js::Function(JSContext *cx, unsigned argc, Value *vp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedString arg(cx);   // used multiple times below

    /* Block this call if security callbacks forbid it. */
    Rooted<GlobalObject*> global(cx, &args.callee().global());
    if (!GlobalObject::isRuntimeCodeGenEnabled(cx, global)) {
        JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CSP_BLOCKED_FUNCTION);
        return false;
    }

    AutoKeepAtoms keepAtoms(cx->runtime());
    AutoNameVector formals(cx);

    bool hasRest = false;

    const char *filename;
    unsigned lineno;
    JSPrincipals *originPrincipals;
    CurrentScriptFileLineOrigin(cx, &filename, &lineno, &originPrincipals);
    JSPrincipals *principals = PrincipalsForCompiledCode(args, cx);

    CompileOptions options(cx);
    options.setPrincipals(principals)
           .setOriginPrincipals(originPrincipals)
           .setFileAndLine(filename, lineno)
           .setCompileAndGo(true);

    unsigned n = args.length() ? args.length() - 1 : 0;
    if (n > 0) {
        /*
         * Collect the function-argument arguments into one string, separated
         * by commas, then make a tokenstream from that string, and scan it to
         * get the arguments.  We need to throw the full scanner at the
         * problem, because the argument string can legitimately contain
         * comments and linefeeds.  XXX It might be better to concatenate
         * everything up into a function definition and pass it to the
         * compiler, but doing it this way is less of a delta from the old
         * code.  See ECMA 15.3.2.1.
         */
        size_t args_length = 0;
        for (unsigned i = 0; i < n; i++) {
            /* Collect the lengths for all the function-argument arguments. */
            arg = ToString<CanGC>(cx, args.handleAt(i));
            if (!arg)
                return false;
            args[i].setString(arg);

            /*
             * Check for overflow.  The < test works because the maximum
             * JSString length fits in 2 fewer bits than size_t has.
             */
            size_t old_args_length = args_length;
            args_length = old_args_length + arg->length();
            if (args_length < old_args_length) {
                js_ReportAllocationOverflow(cx);
                return false;
            }
        }

        /* Add 1 for each joining comma and check for overflow (two ways). */
        size_t old_args_length = args_length;
        args_length = old_args_length + n - 1;
        if (args_length < old_args_length ||
            args_length >= ~(size_t)0 / sizeof(jschar)) {
            js_ReportAllocationOverflow(cx);
            return false;
        }

        /*
         * Allocate a string to hold the concatenated arguments, including room
         * for a terminating 0. Mark cx->tempLifeAlloc for later release, to
         * free collected_args and its tokenstream in one swoop.
         */
        LifoAllocScope las(&cx->tempLifoAlloc());
        jschar *cp = cx->tempLifoAlloc().newArray<jschar>(args_length + 1);
        if (!cp) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        StableCharPtr collected_args(cp, args_length + 1);

        /*
         * Concatenate the arguments into the new string, separated by commas.
         */
        for (unsigned i = 0; i < n; i++) {
            arg = args[i].toString();
            size_t arg_length = arg->length();
            const jschar *arg_chars = arg->getChars(cx);
            if (!arg_chars)
                return false;
            (void) js_strncpy(cp, arg_chars, arg_length);
            cp += arg_length;

            /* Add separating comma or terminating 0. */
            *cp++ = (i + 1 < n) ? ',' : 0;
        }

        /*
         * Initialize a tokenstream that reads from the given string.  No
         * StrictModeGetter is needed because this TokenStream won't report any
         * strict mode errors.  Any strict mode errors which might be reported
         * here (duplicate argument names, etc.) will be detected when we
         * compile the function body.
         */
        TokenStream ts(cx, options, collected_args.get(), args_length,
                       /* strictModeGetter = */ NULL, keepAtoms);

        /* The argument string may be empty or contain no tokens. */
        TokenKind tt = ts.getToken();
        if (tt != TOK_EOF) {
            for (;;) {
                /*
                 * Check that it's a name.  This also implicitly guards against
                 * TOK_ERROR, which was already reported.
                 */
                if (hasRest) {
                    ts.reportError(JSMSG_PARAMETER_AFTER_REST);
                    return false;
                }

                if (tt != TOK_NAME) {
                    if (tt == TOK_TRIPLEDOT) {
                        hasRest = true;
                        tt = ts.getToken();
                        if (tt != TOK_NAME) {
                            if (tt != TOK_ERROR)
                                ts.reportError(JSMSG_NO_REST_NAME);
                            return false;
                        }
                    } else {
                        return OnBadFormal(cx, tt);
                    }
                }

                if (!formals.append(ts.currentToken().name()))
                    return false;

                /*
                 * Get the next token.  Stop on end of stream.  Otherwise
                 * insist on a comma, get another name, and iterate.
                 */
                tt = ts.getToken();
                if (tt == TOK_EOF)
                    break;
                if (tt != TOK_COMMA)
                    return OnBadFormal(cx, tt);
                tt = ts.getToken();
            }
        }
    }

#ifdef DEBUG
    for (unsigned i = 0; i < formals.length(); ++i) {
        JSString *str = formals[i];
        JS_ASSERT(str->asAtom().asPropertyName() == formals[i]);
    }
#endif

    RootedString str(cx);
    if (!args.length())
        str = cx->runtime()->emptyString;
    else
        str = ToString<CanGC>(cx, args.handleAt(args.length() - 1));
    if (!str)
        return false;
    JSLinearString *linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    JS::Anchor<JSString *> strAnchor(str);
    const jschar *chars = linear->chars();
    size_t length = linear->length();

    /*
     * NB: (new Function) is not lexically closed by its caller, it's just an
     * anonymous function in the top-level scope that its constructor inhabits.
     * Thus 'var x = 42; f = new Function("return x"); print(f())' prints 42,
     * and so would a call to f from another top-level's script or function.
     */
    RootedAtom anonymousAtom(cx, cx->names().anonymous);
    RootedFunction fun(cx, NewFunction(cx, NullPtr(), NULL, 0, JSFunction::INTERPRETED_LAMBDA,
                                       global, anonymousAtom, JSFunction::FinalizeKind,
                                       TenuredObject));
    if (!fun)
        return false;

    if (hasRest)
        fun->setHasRest();

    bool ok = frontend::CompileFunctionBody(cx, &fun, options, formals, chars, length);
    args.rval().setObject(*fun);
    return ok;
}

bool
js::IsBuiltinFunctionConstructor(JSFunction *fun)
{
    return fun->maybeNative() == Function;
}

JSFunction *
js::NewFunction(JSContext *cx, HandleObject funobjArg, Native native, unsigned nargs,
                JSFunction::Flags flags, HandleObject parent, HandleAtom atom,
                gc::AllocKind allocKind /* = JSFunction::FinalizeKind */,
                NewObjectKind newKind /* = GenericObject */)
{
    JS_ASSERT(allocKind == JSFunction::FinalizeKind || allocKind == JSFunction::ExtendedFinalizeKind);
    JS_ASSERT(sizeof(JSFunction) <= gc::Arena::thingSize(JSFunction::FinalizeKind));
    JS_ASSERT(sizeof(FunctionExtended) <= gc::Arena::thingSize(JSFunction::ExtendedFinalizeKind));

    RootedObject funobj(cx, funobjArg);
    if (funobj) {
        JS_ASSERT(funobj->is<JSFunction>());
        JS_ASSERT(funobj->getParent() == parent);
        JS_ASSERT_IF(native && cx->typeInferenceEnabled(), funobj->hasSingletonType());
    } else {
        // Don't give asm.js module functions a singleton type since they
        // are cloned (via CloneFunctionObjectIfNotSingleton) which assumes
        // that hasSingletonType implies isInterpreted.
        if (native && !IsAsmJSModuleNative(native))
            newKind = SingletonObject;
        funobj = NewObjectWithClassProto(cx, &JSFunction::class_, NULL,
                                         SkipScopeParent(parent), allocKind, newKind);
        if (!funobj)
            return NULL;
    }
    RootedFunction fun(cx, &funobj->as<JSFunction>());

    /* Initialize all function members. */
    fun->nargs = uint16_t(nargs);
    fun->flags = flags;
    if (fun->isInterpreted()) {
        JS_ASSERT(!native);
        fun->mutableScript().init(NULL);
        fun->initEnvironment(parent);
    } else {
        JS_ASSERT(fun->isNative());
        JS_ASSERT(native);
        fun->initNative(native, NULL);
    }
    if (allocKind == JSFunction::ExtendedFinalizeKind) {
        fun->flags |= JSFunction::EXTENDED;
        fun->initializeExtended();
    }
    fun->initAtom(atom);

    return fun;
}

JSFunction *
js::CloneFunctionObject(JSContext *cx, HandleFunction fun, HandleObject parent, gc::AllocKind allocKind,
                        NewObjectKind newKindArg /* = GenericObject */)
{
    TRACK_MEMORY_SCOPE("Javascript");
    JS_ASSERT(parent);
    JS_ASSERT(!fun->isBoundFunction());

    bool useSameScript = cx->compartment() == fun->compartment() &&
                         !fun->hasSingletonType() &&
                         !types::UseNewTypeForClone(fun);

    if (!useSameScript && fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return NULL;

    NewObjectKind newKind = useSameScript ? newKindArg : SingletonObject;
    JSObject *cloneobj = NewObjectWithClassProto(cx, &JSFunction::class_, NULL,
                                                 SkipScopeParent(parent), allocKind, newKind);
    if (!cloneobj)
        return NULL;
    RootedFunction clone(cx, &cloneobj->as<JSFunction>());

    clone->nargs = fun->nargs;
    clone->flags = fun->flags & ~JSFunction::EXTENDED;
    if (fun->hasScript()) {
        clone->initScript(fun->nonLazyScript());
        clone->initEnvironment(parent);
    } else if (fun->isInterpretedLazy()) {
        LazyScript *lazy = fun->lazyScriptOrNull();
        clone->initLazyScript(lazy);
        clone->initEnvironment(parent);
    } else {
        clone->initNative(fun->native(), fun->jitInfo());
    }
    clone->initAtom(fun->displayAtom());

    if (allocKind == JSFunction::ExtendedFinalizeKind) {
        clone->flags |= JSFunction::EXTENDED;
        if (fun->isExtended() && fun->compartment() == cx->compartment()) {
            for (unsigned i = 0; i < FunctionExtended::NUM_EXTENDED_SLOTS; i++)
                clone->initExtendedSlot(i, fun->getExtendedSlot(i));
        } else {
            clone->initializeExtended();
        }
    }

    if (useSameScript) {
        /*
         * Clone the function, reusing its script. We can use the same type as
         * the original function provided that its prototype is correct.
         */
        if (fun->getProto() == clone->getProto())
            clone->setType(fun->type());
        return clone;
    }

    RootedFunction cloneRoot(cx, clone);

    /*
     * Across compartments we have to clone the script for interpreted
     * functions. Cross-compartment cloning only happens via JSAPI
     * (JS_CloneFunctionObject) which dynamically ensures that 'script' has
     * no enclosing lexical scope (only the global scope).
     */
    if (cloneRoot->isInterpreted() && !CloneFunctionScript(cx, fun, cloneRoot, newKindArg))
        return NULL;

    return cloneRoot;
}

JSFunction *
js::DefineFunction(JSContext *cx, HandleObject obj, HandleId id, Native native,
                   unsigned nargs, unsigned flags, AllocKind allocKind /* = FinalizeKind */,
                   NewObjectKind newKind /* = GenericObject */)
{
    TRACK_MEMORY_SCOPE("Javascript");
    PropertyOp gop;
    StrictPropertyOp sop;

    RootedFunction fun(cx);

    if (flags & JSFUN_STUB_GSOPS) {
        /*
         * JSFUN_STUB_GSOPS is a request flag only, not stored in fun->flags or
         * the defined property's attributes. This allows us to encode another,
         * internal flag using the same bit, JSFUN_EXPR_CLOSURE -- see jsfun.h
         * for more on this.
         */
        flags &= ~JSFUN_STUB_GSOPS;
        gop = JS_PropertyStub;
        sop = JS_StrictPropertyStub;
    } else {
        gop = NULL;
        sop = NULL;
    }

    JSFunction::Flags funFlags;
    if (!native)
        funFlags = JSFunction::INTERPRETED_LAZY;
    else
        funFlags = JSAPIToJSFunctionFlags(flags);
    RootedAtom atom(cx, JSID_IS_ATOM(id) ? JSID_TO_ATOM(id) : NULL);
    fun = NewFunction(cx, NullPtr(), native, nargs, funFlags, obj, atom, allocKind, newKind);
    if (!fun)
        return NULL;

    RootedValue funVal(cx, ObjectValue(*fun));
    if (!JSObject::defineGeneric(cx, obj, id, funVal, gop, sop, flags & ~JSFUN_FLAGS_MASK))
        return NULL;

    return fun;
}

void
js::ReportIncompatibleMethod(JSContext *cx, CallReceiver call, Class *clasp)
{
    RootedValue thisv(cx, call.thisv());

#ifdef DEBUG
    if (thisv.isObject()) {
        JS_ASSERT(thisv.toObject().getClass() != clasp ||
                  !thisv.toObject().isNative() ||
                  !thisv.toObject().getProto() ||
                  thisv.toObject().getProto()->getClass() != clasp);
    } else if (thisv.isString()) {
        JS_ASSERT(clasp != &StringObject::class_);
    } else if (thisv.isNumber()) {
        JS_ASSERT(clasp != &NumberObject::class_);
    } else if (thisv.isBoolean()) {
        JS_ASSERT(clasp != &BooleanObject::class_);
    } else {
        JS_ASSERT(thisv.isUndefined() || thisv.isNull());
    }
#endif

    if (JSFunction *fun = ReportIfNotFunction(cx, call.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                                 clasp->name, funName, InformalValueTypeName(thisv));
        }
    }
}

void
js::ReportIncompatible(JSContext *cx, CallReceiver call)
{
    if (JSFunction *fun = ReportIfNotFunction(cx, call.calleev())) {
        JSAutoByteString funNameBytes;
        if (const char *funName = GetFunctionNameBytes(cx, fun, &funNameBytes)) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_METHOD,
                                 funName, "method", InformalValueTypeName(call.thisv()));
        }
    }
}

bool
JSObject::hasIdempotentProtoChain() const
{
    // Return false if obj (or an object on its proto chain) is non-native or
    // has a resolve or lookup hook.
    JSObject *obj = const_cast<JSObject *>(this);
    while (true) {
        if (!obj->isNative())
            return false;

        JSResolveOp resolve = obj->getClass()->resolve;
        if (resolve != JS_ResolveStub && resolve != (JSResolveOp) fun_resolve)
            return false;

        if (obj->getOps()->lookupProperty || obj->getOps()->lookupGeneric || obj->getOps()->lookupElement)
            return false;

        obj = obj->getProto();
        if (!obj)
            return true;
    }

    JS_NOT_REACHED("Should not get here");
    return false;
}

