// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8Binding.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "core/testing/RefCountedScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "wtf/Vector.h"

#include <gtest/gtest.h>
#include <v8.h>

#define CHECK_TOV8VALUE(expected, value) check(expected, value, __FILE__, __LINE__)

namespace blink {

namespace {

class V8ValueTraitsTest : public ::testing::Test {
public:

    V8ValueTraitsTest()
        : m_scope(v8::Isolate::GetCurrent())
    {
    }

    virtual ~V8ValueTraitsTest()
    {
    }

    template <typename T> v8::Local<v8::Value> toV8Value(const T& value)
    {
        return V8ValueTraits<T>::toV8Value(value, m_scope.scriptState()->context()->Global(), m_scope.isolate());
    }

    template <typename T>
    void check(const char* expected, const T& value, const char* path, int lineNumber)
    {
        v8::Local<v8::Value> actual = toV8Value(value);
        if (actual.IsEmpty()) {
            ADD_FAILURE_AT(path, lineNumber) << "toV8Value returns an empty value.";
            return;
        }
        String actualString = toCoreString(actual->ToString());
        if (String(expected) != actualString) {
            ADD_FAILURE_AT(path, lineNumber) << "toV8Value returns an incorrect value.\n  Actual: " << actualString.utf8().data() << "\nExpected: " << expected;
            return;
        }
    }

    V8TestingScope m_scope;
};

class GarbageCollectedHolder : public GarbageCollectedFinalized<GarbageCollectedHolder> {
public:
    GarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    void trace(Visitor* visitor) { visitor->trace(m_scriptWrappable); }

    // This should be public in order to access a Member<X> object.
    Member<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

class OffHeapGarbageCollectedHolder {
public:
    OffHeapGarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    // This should be public in order to access a Persistent<X> object.
    Persistent<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

TEST_F(V8ValueTraitsTest, numeric)
{
    CHECK_TOV8VALUE("0", static_cast<int>(0));
    CHECK_TOV8VALUE("1", static_cast<int>(1));
    CHECK_TOV8VALUE("-1", static_cast<int>(-1));

    CHECK_TOV8VALUE("-2", static_cast<long>(-2));
    CHECK_TOV8VALUE("2", static_cast<unsigned>(2));
    CHECK_TOV8VALUE("2", static_cast<unsigned long>(2));

    CHECK_TOV8VALUE("0.5", static_cast<double>(0.5));
    CHECK_TOV8VALUE("-0.5", static_cast<float>(-0.5));
}

TEST_F(V8ValueTraitsTest, boolean)
{
    CHECK_TOV8VALUE("true", true);
    CHECK_TOV8VALUE("false", false);
}

TEST_F(V8ValueTraitsTest, string)
{
    char arrayString[] = "arrayString";
    const char constArrayString[] = "constArrayString";
    CHECK_TOV8VALUE("arrayString", arrayString);
    CHECK_TOV8VALUE("constArrayString", constArrayString);
    CHECK_TOV8VALUE("pointer", const_cast<char*>(static_cast<const char*>("pointer")));
    CHECK_TOV8VALUE("constpointer", static_cast<const char*>("constpointer"));
    CHECK_TOV8VALUE("coreString", String("coreString"));
    CHECK_TOV8VALUE("atomicString", AtomicString("atomicString"));

    // Null strings are converted to empty strings.
    CHECK_TOV8VALUE("", static_cast<char*>(nullptr));
    CHECK_TOV8VALUE("", static_cast<const char*>(nullptr));
    CHECK_TOV8VALUE("", String());
    CHECK_TOV8VALUE("", AtomicString());
}

TEST_F(V8ValueTraitsTest, nullType)
{
    CHECK_TOV8VALUE("null", V8NullType());
}

TEST_F(V8ValueTraitsTest, nullptrType)
{
    CHECK_TOV8VALUE("null", nullptr);
}

TEST_F(V8ValueTraitsTest, undefinedType)
{
    CHECK_TOV8VALUE("undefined", V8UndefinedType());
}

TEST_F(V8ValueTraitsTest, refCountedScriptWrappable)
{
    RefPtr<RefCountedScriptWrappable> object = RefCountedScriptWrappable::create("hello");

    CHECK_TOV8VALUE("hello", object);
    CHECK_TOV8VALUE("hello", object.get());
    CHECK_TOV8VALUE("hello", object.release());

    ASSERT_FALSE(object);

    CHECK_TOV8VALUE("null", object);
    CHECK_TOV8VALUE("null", object.get());
    CHECK_TOV8VALUE("null", object.release());
}

TEST_F(V8ValueTraitsTest, garbageCollectedScriptWrappable)
{
    GarbageCollectedScriptWrappable* object = new GarbageCollectedScriptWrappable("world");
    GarbageCollectedHolder holder(object);
    OffHeapGarbageCollectedHolder offHeapHolder(object);

    CHECK_TOV8VALUE("world", object);
    CHECK_TOV8VALUE("world", RawPtr<GarbageCollectedScriptWrappable>(object));
    CHECK_TOV8VALUE("world", holder.m_scriptWrappable);
    CHECK_TOV8VALUE("world", offHeapHolder.m_scriptWrappable);

    object = nullptr;
    holder.m_scriptWrappable = nullptr;
    offHeapHolder.m_scriptWrappable = nullptr;

    CHECK_TOV8VALUE("null", object);
    CHECK_TOV8VALUE("null", RawPtr<GarbageCollectedScriptWrappable>(object));
    CHECK_TOV8VALUE("null", holder.m_scriptWrappable);
    CHECK_TOV8VALUE("null", offHeapHolder.m_scriptWrappable);
}

TEST_F(V8ValueTraitsTest, vector)
{
    Vector<RefPtr<RefCountedScriptWrappable> > v;
    v.append(RefCountedScriptWrappable::create("foo"));
    v.append(RefCountedScriptWrappable::create("bar"));

    CHECK_TOV8VALUE("foo,bar", v);
}

TEST_F(V8ValueTraitsTest, stringVectors)
{
    Vector<String> stringVector;
    stringVector.append("foo");
    stringVector.append("bar");
    CHECK_TOV8VALUE("foo,bar", stringVector);

    Vector<AtomicString> atomicStringVector;
    atomicStringVector.append("quux");
    atomicStringVector.append("bar");
    CHECK_TOV8VALUE("quux,bar", atomicStringVector);
}

TEST_F(V8ValueTraitsTest, basicTypeVectors)
{
    Vector<int> intVector;
    intVector.append(42);
    intVector.append(23);
    CHECK_TOV8VALUE("42,23", intVector);

    Vector<long> longVector;
    longVector.append(31773);
    longVector.append(404);
    CHECK_TOV8VALUE("31773,404", longVector);

    Vector<unsigned> unsignedVector;
    unsignedVector.append(1);
    unsignedVector.append(2);
    CHECK_TOV8VALUE("1,2", unsignedVector);

    Vector<unsigned long> unsignedLongVector;
    unsignedLongVector.append(1001);
    unsignedLongVector.append(2002);
    CHECK_TOV8VALUE("1001,2002", unsignedLongVector);

    Vector<float> floatVector;
    floatVector.append(0.125);
    floatVector.append(1.);
    CHECK_TOV8VALUE("0.125,1", floatVector);

    Vector<double> doubleVector;
    doubleVector.append(2.3);
    doubleVector.append(4.2);
    CHECK_TOV8VALUE("2.3,4.2", doubleVector);

    Vector<bool> boolVector;
    boolVector.append(true);
    boolVector.append(true);
    boolVector.append(false);
    CHECK_TOV8VALUE("true,true,false", boolVector);
}

TEST_F(V8ValueTraitsTest, heapVector)
{
    HeapVector<Member<GarbageCollectedScriptWrappable> > v;
    v.append(new GarbageCollectedScriptWrappable("hoge"));
    v.append(new GarbageCollectedScriptWrappable("fuga"));
    v.append(nullptr);

    CHECK_TOV8VALUE("hoge,fuga,", v);
}

TEST_F(V8ValueTraitsTest, stringHeapVectors)
{
    HeapVector<String> stringVector;
    stringVector.append("foo");
    stringVector.append("bar");
    CHECK_TOV8VALUE("foo,bar", stringVector);

    HeapVector<AtomicString> atomicStringVector;
    atomicStringVector.append("quux");
    atomicStringVector.append("bar");
    CHECK_TOV8VALUE("quux,bar", atomicStringVector);
}

TEST_F(V8ValueTraitsTest, basicTypeHeapVectors)
{
    HeapVector<int> intVector;
    intVector.append(42);
    intVector.append(23);
    CHECK_TOV8VALUE("42,23", intVector);

    HeapVector<long> longVector;
    longVector.append(31773);
    longVector.append(404);
    CHECK_TOV8VALUE("31773,404", longVector);

    HeapVector<unsigned> unsignedVector;
    unsignedVector.append(1);
    unsignedVector.append(2);
    CHECK_TOV8VALUE("1,2", unsignedVector);

    HeapVector<unsigned long> unsignedLongVector;
    unsignedLongVector.append(1001);
    unsignedLongVector.append(2002);
    CHECK_TOV8VALUE("1001,2002", unsignedLongVector);

    HeapVector<float> floatVector;
    floatVector.append(0.125);
    floatVector.append(1.);
    CHECK_TOV8VALUE("0.125,1", floatVector);

    HeapVector<double> doubleVector;
    doubleVector.append(2.3);
    doubleVector.append(4.2);
    CHECK_TOV8VALUE("2.3,4.2", doubleVector);

    HeapVector<bool> boolVector;
    boolVector.append(true);
    boolVector.append(true);
    boolVector.append(false);
    CHECK_TOV8VALUE("true,true,false", boolVector);
}

TEST_F(V8ValueTraitsTest, scriptValue)
{
    ScriptValue value(m_scope.scriptState(), v8::Number::New(m_scope.isolate(), 1234));

    CHECK_TOV8VALUE("1234", value);
}

TEST_F(V8ValueTraitsTest, v8Value)
{
    v8::Local<v8::Value> localValue(v8::Number::New(m_scope.isolate(), 1234));
    v8::Handle<v8::Value> handleValue(v8::Number::New(m_scope.isolate(), 5678));

    CHECK_TOV8VALUE("1234", localValue);
    CHECK_TOV8VALUE("5678", handleValue);
}

TEST_F(V8ValueTraitsTest, toImplArray)
{
    {
        v8::Handle<v8::Array> v8StringArray = v8::Array::New(m_scope.isolate(), 2);
        v8StringArray->Set(toV8Value(0), toV8Value("Hello, World!"));
        v8StringArray->Set(toV8Value(1), toV8Value("Hi, Mom!"));

        NonThrowableExceptionState exceptionState;
        Vector<String> stringVector = toImplArray<String>(v8StringArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVector.size());
        EXPECT_EQ("Hello, World!", stringVector[0]);
        EXPECT_EQ("Hi, Mom!", stringVector[1]);
    }
    {
        v8::Handle<v8::Array> v8UnsignedArray = v8::Array::New(m_scope.isolate(), 3);
        v8UnsignedArray->Set(toV8Value(0), toV8Value(42));
        v8UnsignedArray->Set(toV8Value(1), toV8Value(1729));
        v8UnsignedArray->Set(toV8Value(2), toV8Value(31773));

        NonThrowableExceptionState exceptionState;
        Vector<unsigned> unsignedVector = toImplArray<unsigned>(v8UnsignedArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, unsignedVector.size());
        EXPECT_EQ(42U, unsignedVector[0]);
        EXPECT_EQ(1729U, unsignedVector[1]);
        EXPECT_EQ(31773U, unsignedVector[2]);
    }
    {
        const double doublePi = 3.141592653589793238;
        const float floatPi = doublePi;
        v8::Handle<v8::Array> v8RealArray = v8::Array::New(m_scope.isolate(), 1);
        v8RealArray->Set(toV8Value(0), toV8Value(doublePi));

        NonThrowableExceptionState exceptionState;
        Vector<double> doubleVector = toImplArray<double>(v8RealArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(1U, doubleVector.size());
        EXPECT_EQ(doublePi, doubleVector[0]);

        Vector<float> floatVector = toImplArray<float>(v8RealArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(1U, floatVector.size());
        EXPECT_EQ(floatPi, floatVector[0]);
    }
    {
        v8::Handle<v8::Array> v8Array = v8::Array::New(m_scope.isolate(), 3);
        v8Array->Set(toV8Value(0), toV8Value("Vini, vidi, vici."));
        v8Array->Set(toV8Value(1), toV8Value(65535));
        v8Array->Set(toV8Value(2), toV8Value(0.125));

        NonThrowableExceptionState exceptionState;
        Vector<v8::Handle<v8::Value> > v8HandleVector = toImplArray<v8::Handle<v8::Value> >(v8Array, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, v8HandleVector.size());
        EXPECT_EQ("Vini, vidi, vici.", toUSVString(v8HandleVector[0], exceptionState));
        EXPECT_EQ(65535U, toUInt32(v8HandleVector[1]));
        EXPECT_EQ(0.125, toFloat(v8HandleVector[2]));

        Vector<ScriptValue> scriptValueVector = toImplArray<ScriptValue>(v8Array, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(3U, scriptValueVector.size());
        String reportOnZela;
        EXPECT_TRUE(scriptValueVector[0].toString(reportOnZela));
        EXPECT_EQ("Vini, vidi, vici.", reportOnZela);
        EXPECT_EQ(65535U, toUInt32(scriptValueVector[1].v8Value()));
        EXPECT_EQ(0.125, toFloat(scriptValueVector[2].v8Value()));
    }
    {
        v8::Handle<v8::Array> v8StringArray1 = v8::Array::New(m_scope.isolate(), 2);
        v8StringArray1->Set(toV8Value(0), toV8Value("foo"));
        v8StringArray1->Set(toV8Value(1), toV8Value("bar"));
        v8::Handle<v8::Array> v8StringArray2 = v8::Array::New(m_scope.isolate(), 3);
        v8StringArray2->Set(toV8Value(0), toV8Value("x"));
        v8StringArray2->Set(toV8Value(1), toV8Value("y"));
        v8StringArray2->Set(toV8Value(2), toV8Value("z"));
        v8::Handle<v8::Array> v8StringArrayArray = v8::Array::New(m_scope.isolate(), 2);
        v8StringArrayArray->Set(toV8Value(0), v8StringArray1);
        v8StringArrayArray->Set(toV8Value(1), v8StringArray2);

        NonThrowableExceptionState exceptionState;
        Vector<Vector<String> > stringVectorVector = toImplArray<Vector<String> >(v8StringArrayArray, 0, m_scope.isolate(), exceptionState);
        EXPECT_EQ(2U, stringVectorVector.size());
        EXPECT_EQ(2U, stringVectorVector[0].size());
        EXPECT_EQ("foo", stringVectorVector[0][0]);
        EXPECT_EQ("bar", stringVectorVector[0][1]);
        EXPECT_EQ(3U, stringVectorVector[1].size());
        EXPECT_EQ("x", stringVectorVector[1][0]);
        EXPECT_EQ("y", stringVectorVector[1][1]);
        EXPECT_EQ("z", stringVectorVector[1][2]);
    }
}

} // namespace

} // namespace blink
