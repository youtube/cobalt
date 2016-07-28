#ifndef JSDestructibleObject_h
#define JSDestructibleObject_h

#include "JSObject.h"

namespace JSC {

struct ClassInfo;

class JSDestructibleObject : public JSNonFinalObject {
public:
    typedef JSNonFinalObject Base;

    static const bool needsDestruction = true;

    const ClassInfo* classInfo() const { return m_classInfo; }

protected:
    JSDestructibleObject(JSGlobalData& globalData, Structure* structure, Butterfly* butterfly = 0)
        : JSNonFinalObject(globalData, structure, butterfly)
        , m_classInfo(structure->classInfo())
    {
        ASSERT(m_classInfo);
    }

private:
    const ClassInfo* m_classInfo;
};

} // namespace JSC

#endif
