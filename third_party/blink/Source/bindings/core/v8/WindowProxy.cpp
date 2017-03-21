/*
 * Copyright (C) 2008, 2009, 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/core/v8/WindowProxy.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMActivityLogger.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8GCForContextDispose.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8HTMLDocument.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8Initializer.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8Window.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/DocumentNameCollection.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLIFrameElement.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/TraceEvent.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/Platform.h"
#include "wtf/Assertions.h"
#include "wtf/OwnPtr.h"
#include "wtf/StringExtras.h"
#include "wtf/text/CString.h"
#include <algorithm>
#include <utility>
#include <v8-debug.h>
#include <v8.h>

namespace blink {

static void checkDocumentWrapper(v8::Handle<v8::Object> wrapper, Document* document)
{
    ASSERT(V8Document::toImpl(wrapper) == document);
    ASSERT(!document->isHTMLDocument() || (V8Document::toImpl(v8::Handle<v8::Object>::Cast(wrapper->GetPrototype())) == document));
}

PassOwnPtrWillBeRawPtr<WindowProxy> WindowProxy::create(LocalFrame* frame, DOMWrapperWorld& world, v8::Isolate* isolate)
{
    return adoptPtrWillBeNoop(new WindowProxy(frame, &world, isolate));
}

WindowProxy::WindowProxy(LocalFrame* frame, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
    : m_frame(frame)
    , m_isolate(isolate)
    , m_world(world)
{
}

WindowProxy::~WindowProxy()
{
    // clearForClose() or clearForNavigation() must be invoked before destruction starts.
    ASSERT(!isContextInitialized());
}

void WindowProxy::trace(Visitor* visitor)
{
    visitor->trace(m_frame);
}

void WindowProxy::disposeContext(GlobalDetachmentBehavior behavior)
{
    if (!isContextInitialized())
        return;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_scriptState->context();
    m_frame->loader().client()->willReleaseScriptContext(context, m_world->worldId());

    if (behavior == DetachGlobal)
        m_scriptState->detachGlobalObject();

    m_scriptState->disposePerContextData();

    // It's likely that disposing the context has created a lot of
    // garbage. Notify V8 about this so it'll have a chance of cleaning
    // it up when idle.
    V8GCForContextDispose::instance().notifyContextDisposed(m_frame->isMainFrame());
}

void WindowProxy::clearForClose()
{
    if (!isContextInitialized())
        return;

    m_document.clear();
    disposeContext(DoNotDetachGlobal);
}

void WindowProxy::clearForNavigation()
{
    if (!isContextInitialized())
        return;

    ScriptState::Scope scope(m_scriptState.get());

    m_document.clear();

    // Clear the document wrapper cache before turning on access checks on
    // the old LocalDOMWindow wrapper. This way, access to the document wrapper
    // will be protected by the security checks on the LocalDOMWindow wrapper.
    clearDocumentProperty();

    v8::Handle<v8::Object> windowWrapper = V8Window::findInstanceInPrototypeChain(m_global.newLocal(m_isolate), m_isolate);
    ASSERT(!windowWrapper.IsEmpty());
    windowWrapper->TurnOnAccessCheck();
    disposeContext(DetachGlobal);
}

// Create a new environment and setup the global object.
//
// The global object corresponds to a LocalDOMWindow instance. However, to
// allow properties of the JS LocalDOMWindow instance to be shadowed, we
// use a shadow object as the global object and use the JS LocalDOMWindow
// instance as the prototype for that shadow object. The JS LocalDOMWindow
// instance is undetectable from JavaScript code because the __proto__
// accessors skip that object.
//
// The shadow object and the LocalDOMWindow instance are seen as one object
// from JavaScript. The JavaScript object that corresponds to a
// LocalDOMWindow instance is the shadow object. When mapping a LocalDOMWindow
// instance to a V8 object, we return the shadow object.
//
// To implement split-window, see
//   1) https://bugs.webkit.org/show_bug.cgi?id=17249
//   2) https://wiki.mozilla.org/Gecko:SplitWindow
//   3) https://bugzilla.mozilla.org/show_bug.cgi?id=296639
// we need to split the shadow object further into two objects:
// an outer window and an inner window. The inner window is the hidden
// prototype of the outer window. The inner window is the default
// global object of the context. A variable declared in the global
// scope is a property of the inner window.
//
// The outer window sticks to a LocalFrame, it is exposed to JavaScript
// via window.window, window.self, window.parent, etc. The outer window
// has a security token which is the domain. The outer window cannot
// have its own properties. window.foo = 'x' is delegated to the
// inner window.
//
// When a frame navigates to a new page, the inner window is cut off
// the outer window, and the outer window identify is preserved for
// the frame. However, a new inner window is created for the new page.
// If there are JS code holds a closure to the old inner window,
// it won't be able to reach the outer window via its global object.
bool WindowProxy::initializeIfNeeded()
{
    if (isContextInitialized())
        return true;

    DOMWrapperWorld::setWorldOfInitializingWindow(m_world.get());
    bool result = initialize();
    DOMWrapperWorld::setWorldOfInitializingWindow(0);
    return result;
}

bool WindowProxy::initialize()
{
    TRACE_EVENT0("v8", "WindowProxy::initialize");
    TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "InitializeWindow");

    ScriptForbiddenScope::AllowUserAgentScript allowScript;

    v8::HandleScope handleScope(m_isolate);

    createContext();

    if (!isContextInitialized())
        return false;

    ScriptState::Scope scope(m_scriptState.get());
    v8::Handle<v8::Context> context = m_scriptState->context();
    if (m_global.isEmpty()) {
        m_global.set(m_isolate, context->Global());
        if (m_global.isEmpty()) {
            disposeContext(DoNotDetachGlobal);
            return false;
        }
    }

    if (!installDOMWindow()) {
        disposeContext(DoNotDetachGlobal);
        return false;
    }

    if (m_world->isMainWorld()) {
        // ActivityLogger for main world is updated within updateDocument().
        updateDocument();
        if (m_frame->document()) {
            setSecurityToken(m_frame->document()->securityOrigin());
            ContentSecurityPolicy* csp = m_frame->document()->contentSecurityPolicy();
            context->AllowCodeGenerationFromStrings(csp->allowEval(0, ContentSecurityPolicy::SuppressReport));
            context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_isolate, csp->evalDisabledErrorMessage()));
        }
    } else {
        updateActivityLogger();
        SecurityOrigin* origin = m_world->isolatedWorldSecurityOrigin();
        setSecurityToken(origin);
        if (origin && InspectorInstrumentation::hasFrontends()) {
            InspectorInstrumentation::didCreateIsolatedContext(m_frame, m_scriptState.get(), origin);
        }
    }
    m_frame->loader().client()->didCreateScriptContext(context, m_world->extensionGroup(), m_world->worldId());
    return true;
}

void WindowProxy::createContext()
{
    // The documentLoader pointer could be 0 during frame shutdown.
    // FIXME: Can we remove this check?
    if (!m_frame->loader().documentLoader())
        return;

    // Create a new environment using an empty template for the shadow
    // object. Reuse the global object if one has been created earlier.
    v8::Handle<v8::ObjectTemplate> globalTemplate = V8Window::getShadowObjectTemplate(m_isolate);
    if (globalTemplate.IsEmpty())
        return;

    double contextCreationStartInSeconds = currentTime();

    // Dynamically tell v8 about our extensions now.
    const V8Extensions& extensions = ScriptController::registeredExtensions();
    OwnPtr<const char*[]> extensionNames = adoptArrayPtr(new const char*[extensions.size()]);
    int index = 0;
    int extensionGroup = m_world->extensionGroup();
    int worldId = m_world->worldId();
    for (size_t i = 0; i < extensions.size(); ++i) {
        if (!m_frame->loader().client()->allowScriptExtension(extensions[i]->name(), extensionGroup, worldId))
            continue;

        extensionNames[index++] = extensions[i]->name();
    }
    v8::ExtensionConfiguration extensionConfiguration(index, extensionNames.get());

    v8::Handle<v8::Context> context = v8::Context::New(m_isolate, &extensionConfiguration, globalTemplate, m_global.newLocal(m_isolate));
    if (context.IsEmpty())
        return;
    m_scriptState = ScriptState::create(context, m_world);

    double contextCreationDurationInMilliseconds = (currentTime() - contextCreationStartInSeconds) * 1000;
    const char* histogramName = "WebCore.WindowProxy.createContext.MainWorld";
    if (!m_world->isMainWorld())
        histogramName = "WebCore.WindowProxy.createContext.IsolatedWorld";
    blink::Platform::current()->histogramCustomCounts(histogramName, contextCreationDurationInMilliseconds, 0, 10000, 50);
}

static v8::Handle<v8::Object> toInnerGlobalObject(v8::Handle<v8::Context> context)
{
    return v8::Handle<v8::Object>::Cast(context->Global()->GetPrototype());
}

bool WindowProxy::installDOMWindow()
{
    DOMWindow* window = m_frame->domWindow();
    const WrapperTypeInfo* wrapperTypeInfo = window->wrapperTypeInfo();
    v8::Local<v8::Object> windowWrapper = V8ObjectConstructor::newInstance(m_isolate, m_scriptState->perContextData()->constructorForType(wrapperTypeInfo));
    if (windowWrapper.IsEmpty())
        return false;

    V8DOMWrapper::setNativeInfo(v8::Handle<v8::Object>::Cast(windowWrapper->GetPrototype()), wrapperTypeInfo, window->toScriptWrappableBase());

    // Install the windowWrapper as the prototype of the innerGlobalObject.
    // The full structure of the global object is as follows:
    //
    // outerGlobalObject (Empty object, remains after navigation)
    //   -- has prototype --> innerGlobalObject (Holds global variables, changes during navigation)
    //   -- has prototype --> LocalDOMWindow instance
    //   -- has prototype --> Window.prototype
    //   -- has prototype --> Object.prototype
    //
    // Note: Much of this prototype structure is hidden from web content. The
    //       outer, inner, and LocalDOMWindow instance all appear to be the same
    //       JavaScript object.
    v8::Handle<v8::Object> innerGlobalObject = toInnerGlobalObject(m_scriptState->context());
    V8DOMWrapper::setNativeInfo(innerGlobalObject, wrapperTypeInfo, window->toScriptWrappableBase());
    innerGlobalObject->SetPrototype(windowWrapper);
    V8DOMWrapper::associateObjectWithWrapperNonTemplate(window, wrapperTypeInfo, windowWrapper, m_isolate);
    wrapperTypeInfo->installConditionallyEnabledProperties(windowWrapper, m_isolate);
    return true;
}

void WindowProxy::updateDocumentWrapper(v8::Handle<v8::Object> wrapper)
{
    ASSERT(m_world->isMainWorld());
    m_document.set(m_isolate, wrapper);
}

void WindowProxy::updateDocumentProperty()
{
    if (!m_world->isMainWorld())
        return;

    ScriptState::Scope scope(m_scriptState.get());
    v8::Handle<v8::Context> context = m_scriptState->context();
    v8::Handle<v8::Value> documentWrapper = toV8(m_frame->document(), context->Global(), context->GetIsolate());
    ASSERT(documentWrapper == m_document.newLocal(m_isolate) || m_document.isEmpty());
    if (m_document.isEmpty())
        updateDocumentWrapper(v8::Handle<v8::Object>::Cast(documentWrapper));
    checkDocumentWrapper(m_document.newLocal(m_isolate), m_frame->document());

    // If instantiation of the document wrapper fails, clear the cache
    // and let the LocalDOMWindow accessor handle access to the document.
    if (documentWrapper.IsEmpty()) {
        clearDocumentProperty();
        return;
    }
    ASSERT(documentWrapper->IsObject());
    context->Global()->ForceSet(v8AtomicString(m_isolate, "document"), documentWrapper, static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontDelete));

    // We also stash a reference to the document on the inner global object so that
    // LocalDOMWindow objects we obtain from JavaScript references are guaranteed to have
    // live Document objects.
    V8HiddenValue::setHiddenValue(m_isolate, toInnerGlobalObject(context), V8HiddenValue::document(m_isolate), documentWrapper);
}

void WindowProxy::clearDocumentProperty()
{
    ASSERT(isContextInitialized());
    if (!m_world->isMainWorld())
        return;
    v8::HandleScope handleScope(m_isolate);
    m_scriptState->context()->Global()->ForceDelete(v8AtomicString(m_isolate, "document"));
}

void WindowProxy::updateActivityLogger()
{
    m_scriptState->perContextData()->setActivityLogger(V8DOMActivityLogger::activityLogger(
        m_world->worldId(), m_frame->document() ? m_frame->document()->baseURI() : KURL()));
}

void WindowProxy::setSecurityToken(SecurityOrigin* origin)
{
    // If two tokens are equal, then the SecurityOrigins canAccess each other.
    // If two tokens are not equal, then we have to call canAccess.
    // Note: we can't use the HTTPOrigin if it was set from the DOM.
    String token;
    // We stick with an empty token if document.domain was modified or if we
    // are in the initial empty document, so that we can do a full canAccess
    // check in those cases.
    bool delaySet = m_world->isMainWorld()
        && (origin->domainWasSetInDOM()
            || m_frame->loader().stateMachine()->isDisplayingInitialEmptyDocument());
    if (origin && !delaySet)
        token = origin->toString();

    // An empty or "null" token means we always have to call
    // canAccess. The toString method on securityOrigins returns the
    // string "null" for empty security origins and for security
    // origins that should only allow access to themselves. In this
    // case, we use the global object as the security token to avoid
    // calling canAccess when a script accesses its own objects.
    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = m_scriptState->context();
    if (token.isEmpty() || token == "null") {
        context->UseDefaultSecurityToken();
        return;
    }

    if (m_world->isPrivateScriptIsolatedWorld())
        token = "private-script://" + token;

    CString utf8Token = token.utf8();
    // NOTE: V8 does identity comparison in fast path, must use a symbol
    // as the security token.
    context->SetSecurityToken(v8AtomicString(m_isolate, utf8Token.data(), utf8Token.length()));
}

void WindowProxy::updateDocument()
{
    ASSERT(m_world->isMainWorld());
    if (!isGlobalInitialized())
        return;
    if (!isContextInitialized())
        return;
    updateActivityLogger();
    updateDocumentProperty();
    updateSecurityOrigin(m_frame->document()->securityOrigin());
}

static v8::Handle<v8::Value> getNamedProperty(HTMLDocument* htmlDocument, const AtomicString& key, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    if (!htmlDocument->hasNamedItem(key) && !htmlDocument->hasExtraNamedItem(key))
        return v8Undefined();

    RefPtrWillBeRawPtr<DocumentNameCollection> items = htmlDocument->documentNamedItems(key);
    if (items->isEmpty())
        return v8Undefined();

    if (items->hasExactlyOneItem()) {
        HTMLElement* element = items->item(0);
        ASSERT(element);
        Frame* frame = isHTMLIFrameElement(*element) ? toHTMLIFrameElement(*element).contentFrame() : 0;
        if (frame)
            return toV8(frame->domWindow(), creationContext, isolate);
        return toV8(element, creationContext, isolate);
    }
    return toV8(PassRefPtrWillBeRawPtr<HTMLCollection>(items.release()), creationContext, isolate);
}

static void getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    // FIXME: Consider passing StringImpl directly.
    AtomicString name = toCoreAtomicString(property);
    HTMLDocument* htmlDocument = V8HTMLDocument::toImpl(info.Holder());
    ASSERT(htmlDocument);
    v8::Handle<v8::Value> result = getNamedProperty(htmlDocument, name, info.Holder(), info.GetIsolate());
    if (!result.IsEmpty()) {
        v8SetReturnValue(info, result);
        return;
    }
    v8::Handle<v8::Value> prototype = info.Holder()->GetPrototype();
    if (prototype->IsObject()) {
        v8SetReturnValue(info, prototype.As<v8::Object>()->Get(property));
        return;
    }
}

void WindowProxy::namedItemAdded(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!isContextInitialized())
        return;

    ScriptState::Scope scope(m_scriptState.get());
    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->SetAccessor(v8String(m_isolate, name), getter);
}

void WindowProxy::namedItemRemoved(HTMLDocument* document, const AtomicString& name)
{
    ASSERT(m_world->isMainWorld());

    if (!isContextInitialized())
        return;

    if (document->hasNamedItem(name) || document->hasExtraNamedItem(name))
        return;

    ScriptState::Scope scope(m_scriptState.get());
    ASSERT(!m_document.isEmpty());
    v8::Handle<v8::Object> documentHandle = m_document.newLocal(m_isolate);
    checkDocumentWrapper(documentHandle, document);
    documentHandle->Delete(v8String(m_isolate, name));
}

void WindowProxy::updateSecurityOrigin(SecurityOrigin* origin)
{
    ASSERT(m_world->isMainWorld());
    if (!isContextInitialized())
        return;
    setSecurityToken(origin);
}

} // namespace blink
