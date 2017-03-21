/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS symbol tables. */

#include "mozilla/DebugOnly.h"
#include "mozilla/PodOperations.h"

#include "jsapi.h"
#include "jsatom.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "js/HashTable.h"
#include "vm/Shape.h"

#include "jscntxtinlines.h"
#include "jsobjinlines.h"

#include "vm/Shape-inl.h"

#include "nb/memory_scope.h"

using namespace js;
using namespace js::gc;

using mozilla::DebugOnly;
using mozilla::PodZero;

bool
ShapeTable::init(JSRuntime *rt, Shape *lastProp)
{
    TRACK_MEMORY_SCOPE("Javascript");
    /*
     * Either we're creating a table for a large scope that was populated
     * via property cache hit logic under JSOP_INITPROP, JSOP_SETNAME, or
     * JSOP_SETPROP; or else calloc failed at least once already. In any
     * event, let's try to grow, overallocating to hold at least twice the
     * current population.
     */
    uint32_t sizeLog2 = JS_CEILING_LOG2W(2 * entryCount);
    if (sizeLog2 < MIN_SIZE_LOG2)
        sizeLog2 = MIN_SIZE_LOG2;

    /*
     * Use rt->calloc_ for memory accounting and overpressure handling
     * without OOM reporting. See ShapeTable::change.
     */
    entries = (Shape **) rt->calloc_(sizeOfEntries(JS_BIT(sizeLog2)));
    if (!entries)
        return false;

    hashShift = HASH_BITS - sizeLog2;
    for (Shape::Range<NoGC> r(lastProp); !r.empty(); r.popFront()) {
        Shape &shape = r.front();
        Shape **spp = search(shape.propid(), true);

        /*
         * Beware duplicate args and arg vs. var conflicts: the youngest shape
         * (nearest to lastProp) must win. See bug 600067.
         */
        if (!SHAPE_FETCH(spp))
            SHAPE_STORE_PRESERVING_COLLISION(spp, &shape);
    }
    return true;
}

bool
Shape::makeOwnBaseShape(JSContext *cx)
{
    JS_ASSERT(!base()->isOwned());
    assertSameCompartmentDebugOnly(cx, compartment());

    BaseShape *nbase = js_NewGCBaseShape<NoGC>(cx);
    if (!nbase)
        return false;

    new (nbase) BaseShape(StackBaseShape(this));
    nbase->setOwned(base()->toUnowned());

    this->base_ = nbase;

    return true;
}

void
Shape::handoffTableTo(Shape *shape)
{
    JS_ASSERT(inDictionary() && shape->inDictionary());

    if (this == shape)
        return;

    JS_ASSERT(base()->isOwned() && !shape->base()->isOwned());

    BaseShape *nbase = base();

    JS_ASSERT_IF(shape->hasSlot(), nbase->slotSpan() > shape->slot());

    this->base_ = nbase->baseUnowned();
    nbase->adoptUnowned(shape->base()->toUnowned());

    shape->base_ = nbase;
}

/* static */ bool
Shape::hashify(JSContext *cx, Shape *shape)
{
    TRACK_MEMORY_SCOPE("Javascript");
    JS_ASSERT(!shape->hasTable());

    if (!shape->ensureOwnBaseShape(cx))
        return false;

    JSRuntime *rt = cx->runtime();
    ShapeTable *table = rt->new_<ShapeTable>(shape->entryCount());
    if (!table)
        return false;

    if (!table->init(rt, shape)) {
        js_free(table);
        return false;
    }

    shape->base()->setTable(table);
    return true;
}

/*
 * Double hashing needs the second hash code to be relatively prime to table
 * size, so we simply make hash2 odd.
 */
#define HASH1(hash0,shift)      ((hash0) >> (shift))
#define HASH2(hash0,log2,shift) ((((hash0) << (log2)) >> (shift)) | 1)

Shape **
ShapeTable::search(jsid id, bool adding)
{
    js::HashNumber hash0, hash1, hash2;
    int sizeLog2;
    Shape *stored, *shape, **spp, **firstRemoved;
    uint32_t sizeMask;

    JS_ASSERT(entries);
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /* Compute the primary hash address. */
    hash0 = HashId(id);
    hash1 = HASH1(hash0, hashShift);
    spp = entries + hash1;

    /* Miss: return space for a new entry. */
    stored = *spp;
    if (SHAPE_IS_FREE(stored))
        return spp;

    /* Hit: return entry. */
    shape = SHAPE_CLEAR_COLLISION(stored);
    if (shape && shape->propid() == id)
        return spp;

    /* Collision: double hash. */
    sizeLog2 = HASH_BITS - hashShift;
    hash2 = HASH2(hash0, sizeLog2, hashShift);
    sizeMask = JS_BITMASK(sizeLog2);

#ifdef DEBUG
    uintptr_t collision_flag = SHAPE_COLLISION;
#endif

    /* Save the first removed entry pointer so we can recycle it if adding. */
    if (SHAPE_IS_REMOVED(stored)) {
        firstRemoved = spp;
    } else {
        firstRemoved = NULL;
        if (adding && !SHAPE_HAD_COLLISION(stored))
            SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
        collision_flag &= uintptr_t(*spp) & SHAPE_COLLISION;
#endif
    }

    for (;;) {
        hash1 -= hash2;
        hash1 &= sizeMask;
        spp = entries + hash1;

        stored = *spp;
        if (SHAPE_IS_FREE(stored))
            return (adding && firstRemoved) ? firstRemoved : spp;

        shape = SHAPE_CLEAR_COLLISION(stored);
        if (shape && shape->propid() == id) {
            JS_ASSERT(collision_flag);
            return spp;
        }

        if (SHAPE_IS_REMOVED(stored)) {
            if (!firstRemoved)
                firstRemoved = spp;
        } else {
            if (adding && !SHAPE_HAD_COLLISION(stored))
                SHAPE_FLAG_COLLISION(spp, shape);
#ifdef DEBUG
            collision_flag &= uintptr_t(*spp) & SHAPE_COLLISION;
#endif
        }
    }

    /* NOTREACHED */
    return NULL;
}

bool
ShapeTable::change(int log2Delta, JSContext *cx)
{
    JS_ASSERT(entries);

    /*
     * Grow, shrink, or compress by changing this->entries.
     */
    int oldlog2 = HASH_BITS - hashShift;
    int newlog2 = oldlog2 + log2Delta;
    uint32_t oldsize = JS_BIT(oldlog2);
    uint32_t newsize = JS_BIT(newlog2);
    Shape **newTable = (Shape **) cx->calloc_(sizeOfEntries(newsize));
    if (!newTable)
        return false;

    /* Now that we have newTable allocated, update members. */
    hashShift = HASH_BITS - newlog2;
    removedCount = 0;
    Shape **oldTable = entries;
    entries = newTable;

    /* Copy only live entries, leaving removed and free ones behind. */
    for (Shape **oldspp = oldTable; oldsize != 0; oldspp++) {
        Shape *shape = SHAPE_FETCH(oldspp);
        if (shape) {
            Shape **spp = search(shape->propid(), true);
            JS_ASSERT(SHAPE_IS_FREE(*spp));
            *spp = shape;
        }
        oldsize--;
    }

    /* Finally, free the old entries storage. */
    js_free(oldTable);
    return true;
}

bool
ShapeTable::grow(JSContext *cx)
{
    JS_ASSERT(needsToGrow());

    uint32_t size = capacity();
    int delta = removedCount < size >> 2;

    if (!change(delta, cx) && entryCount + removedCount == size - 1) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

Shape *
Shape::getChildBinding(JSContext *cx, const StackShape &child)
{
    JS_ASSERT(!inDictionary());

    /* Try to allocate all slots inline. */
    uint32_t slots = child.slotSpan();
    gc::AllocKind kind = gc::GetGCObjectKind(slots);
    uint32_t nfixed = gc::GetGCKindSlots(kind);

    return cx->propertyTree().getChild(cx, this, nfixed, child);
}

/* static */ Shape *
Shape::replaceLastProperty(JSContext *cx, const StackBaseShape &base,
                           TaggedProto proto, HandleShape shape)
{
    JS_ASSERT(!shape->inDictionary());

    if (!shape->parent) {
        /* Treat as resetting the initial property of the shape hierarchy. */
        AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
        return EmptyShape::getInitialShape(cx, base.clasp, proto,
                                           base.parent, base.metadata, kind,
                                           base.flags & BaseShape::OBJECT_FLAG_MASK);
    }

    StackShape child(shape);
    StackShape::AutoRooter childRoot(cx, &child);
    {
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return NULL;

        child.base = nbase;
    }

    return cx->propertyTree().getChild(cx, shape->parent, shape->numFixedSlots(), child);
}

/*
 * Get or create a property-tree or dictionary child property of |parent|,
 * which must be lastProperty() if inDictionaryMode(), else parent must be
 * one of lastProperty() or lastProperty()->parent.
 */
/* static */ Shape *
JSObject::getChildProperty(JSContext *cx, HandleObject obj, HandleShape parent, StackShape &child)
{
    /*
     * Shared properties have no slot, but slot_ will reflect that of parent.
     * Unshared properties allocate a slot here but may lose it due to a
     * JS_ClearScope call.
     */
    if (!child.hasSlot()) {
        child.setSlot(parent->maybeSlot());
    } else {
        if (child.hasMissingSlot()) {
            uint32_t slot;
            if (!allocSlot(cx, obj, &slot))
                return NULL;
            child.setSlot(slot);
        } else {
            /* Slots can only be allocated out of order on objects in dictionary mode. */
            JS_ASSERT(obj->inDictionaryMode() ||
                      parent->hasMissingSlot() ||
                      child.slot() == parent->maybeSlot() + 1);
        }
    }

    RootedShape shape(cx);

    if (obj->inDictionaryMode()) {
        JS_ASSERT(parent == obj->lastProperty());
        StackShape::AutoRooter childRoot(cx, &child);
        shape = js_NewGCShape(cx);
        if (!shape)
            return NULL;
        if (child.hasSlot() && child.slot() >= obj->lastProperty()->base()->slotSpan()) {
            if (!JSObject::setSlotSpan(cx, obj, child.slot() + 1))
                return NULL;
        }
        shape->initDictionaryShape(child, obj->numFixedSlots(), &obj->shape_);
    } else {
        shape = cx->propertyTree().getChild(cx, parent, obj->numFixedSlots(), child);
        if (!shape)
            return NULL;
        //JS_ASSERT(shape->parent == parent);
        //JS_ASSERT_IF(parent != lastProperty(), parent == lastProperty()->parent);
        if (!JSObject::setLastProperty(cx, obj, shape))
            return NULL;
    }

    return shape;
}

bool
js::ObjectImpl::toDictionaryMode(JSContext *cx)
{
    JS_ASSERT(!inDictionaryMode());

    /* We allocate the shapes from cx->compartment(), so make sure it's right. */
    JS_ASSERT(compartment() == cx->compartment());

    uint32_t span = slotSpan();

    Rooted<ObjectImpl*> self(cx, this);

    /*
     * Clone the shapes into a new dictionary list. Don't update the
     * last property of this object until done, otherwise a GC
     * triggered while creating the dictionary will get the wrong
     * slot span for this object.
     */
    RootedShape root(cx);
    RootedShape dictionaryShape(cx);

    RootedShape shape(cx, lastProperty());
    while (shape) {
        JS_ASSERT(!shape->inDictionary());

        Shape *dprop = js_NewGCShape(cx);
        if (!dprop) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        HeapPtrShape *listp = dictionaryShape
                              ? &dictionaryShape->parent
                              : (HeapPtrShape *) root.address();

        StackShape child(shape);
        dprop->initDictionaryShape(child, self->numFixedSlots(), listp);

        JS_ASSERT(!dprop->hasTable());
        dictionaryShape = dprop;
        shape = shape->previous();
    }

    if (!Shape::hashify(cx, root)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    JS_ASSERT((Shape **) root->listp == root.address());
    root->listp = &self->shape_;
    self->shape_ = root;

    JS_ASSERT(self->inDictionaryMode());
    root->base()->setSlotSpan(span);

    return true;
}

/*
 * Normalize stub getter and setter values for faster is-stub testing in the
 * SHAPE_CALL_[GS]ETTER macros.
 */
static inline bool
NormalizeGetterAndSetter(JSObject *obj,
                         jsid id, unsigned attrs, unsigned flags,
                         PropertyOp &getter,
                         StrictPropertyOp &setter)
{
    if (setter == JS_StrictPropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_SETTER));
        setter = NULL;
    }
    if (getter == JS_PropertyStub) {
        JS_ASSERT(!(attrs & JSPROP_GETTER));
        getter = NULL;
    }

    return true;
}

/* static */ Shape *
JSObject::addProperty(JSContext *cx, HandleObject obj, HandleId id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32_t slot, unsigned attrs,
                      unsigned flags, int shortid, bool allowDictionary)
{
    JS_ASSERT(!JSID_IS_VOID(id));

    if (!obj->isExtensible()) {
        obj->reportNotExtensible(cx);
        return NULL;
    }

    NormalizeGetterAndSetter(obj, id, attrs, flags, getter, setter);

    Shape **spp = NULL;
    if (obj->inDictionaryMode())
        spp = obj->lastProperty()->table().search(id, true);

    return addPropertyInternal(cx, obj, id, getter, setter, slot, attrs, flags, shortid,
                               spp, allowDictionary);
}

static bool
ShouldConvertToDictionary(JSObject *obj)
{
    /*
     * Use a lower limit if this object is likely a hashmap (SETELEM was used
     * to set properties).
     */
    if (obj->hadElementsAccess())
        return obj->lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT_WITH_ELEMENTS_ACCESS;
    return obj->lastProperty()->entryCount() >= PropertyTree::MAX_HEIGHT;
}

/* static */ Shape *
JSObject::addPropertyInternal(JSContext *cx, HandleObject obj, HandleId id,
                              PropertyOp getter, StrictPropertyOp setter,
                              uint32_t slot, unsigned attrs,
                              unsigned flags, int shortid, Shape **spp,
                              bool allowDictionary)
{
    JS_ASSERT_IF(!allowDictionary, !obj->inDictionaryMode());

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    ShapeTable *table = NULL;
    if (!obj->inDictionaryMode()) {
        bool stableSlot =
            (slot == SHAPE_INVALID_SLOT) ||
            obj->lastProperty()->hasMissingSlot() ||
            (slot == obj->lastProperty()->maybeSlot() + 1);
        JS_ASSERT_IF(!allowDictionary, stableSlot);
        if (allowDictionary &&
            (!stableSlot || ShouldConvertToDictionary(obj)))
        {
            if (!obj->toDictionaryMode(cx))
                return NULL;
            table = &obj->lastProperty()->table();
            spp = table->search(id, true);
        }
    } else {
        table = &obj->lastProperty()->table();
        if (table->needsToGrow()) {
            if (!table->grow(cx))
                return NULL;
            spp = table->search(id, true);
            JS_ASSERT(!SHAPE_FETCH(spp));
        }
    }

    JS_ASSERT(!!table == !!spp);

    /* Find or create a property tree node labeled by our arguments. */
    RootedShape shape(cx);
    {
        RootedShape last(cx, obj->lastProperty());

        uint32_t index;
        bool indexed = js_IdIsIndex(id, &index);

        Rooted<UnownedBaseShape*> nbase(cx);
        if (last->base()->matchesGetterSetter(getter, setter) && !indexed) {
            nbase = last->base()->unowned();
        } else {
            StackBaseShape base(last->base());
            base.updateGetterSetter(attrs, getter, setter);
            if (indexed)
                base.flags |= BaseShape::INDEXED;
            nbase = BaseShape::getUnowned(cx, base);
            if (!nbase)
                return NULL;
        }

        StackShape child(nbase, id, slot, obj->numFixedSlots(), attrs, flags, shortid);
        shape = getChildProperty(cx, obj, last, child);
    }

    if (shape) {
        JS_ASSERT(shape == obj->lastProperty());

        if (table) {
            /* Store the tree node pointer in the table entry for id. */
            SHAPE_STORE_PRESERVING_COLLISION(spp, static_cast<Shape *>(shape));
            ++table->entryCount;

            /* Pass the table along to the new last property, namely shape. */
            JS_ASSERT(&shape->parent->table() == table);
            shape->parent->handoffTableTo(shape);
        }

        obj->checkShapeConsistency();
        return shape;
    }

    obj->checkShapeConsistency();
    return NULL;
}

/*
 * Check and adjust the new attributes for the shape to make sure that our
 * slot access optimizations are sound. It is responsibility of the callers to
 * enforce all restrictions from ECMA-262 v5 8.12.9 [[DefineOwnProperty]].
 */
inline bool
CheckCanChangeAttrs(JSContext *cx, JSObject *obj, Shape *shape, unsigned *attrsp)
{
    if (shape->configurable())
        return true;

    /* A permanent property must stay permanent. */
    *attrsp |= JSPROP_PERMANENT;

    /* Reject attempts to remove a slot from the permanent data property. */
    if (shape->isDataDescriptor() && shape->hasSlot() &&
        (*attrsp & (JSPROP_GETTER | JSPROP_SETTER | JSPROP_SHARED))) {
        obj->reportNotConfigurable(cx, shape->propid());
        return false;
    }

    return true;
}

/* static */ Shape *
JSObject::putProperty(JSContext *cx, HandleObject obj, HandleId id,
                      PropertyOp getter, StrictPropertyOp setter,
                      uint32_t slot, unsigned attrs,
                      unsigned flags, int shortid)
{
    JS_ASSERT(!JSID_IS_VOID(id));

#ifdef DEBUG
    if (obj->isArray()) {
        uint32_t index;
        if (js_IdIsIndex(id, &index))
            JS_ASSERT(index < obj->getArrayLength() || obj->arrayLengthIsWritable());
    }
#endif

    NormalizeGetterAndSetter(obj, id, attrs, flags, getter, setter);

    AutoRooterGetterSetter gsRoot(cx, attrs, &getter, &setter);

    /* Search for id in order to claim its entry if table has been allocated. */
    Shape **spp;
    RootedShape shape(cx, Shape::search(cx, obj->lastProperty(), id, &spp, true));
    if (!shape) {
        /*
         * You can't add properties to a non-extensible object, but you can change
         * attributes of properties in such objects.
         */
        if (!obj->isExtensible()) {
            obj->reportNotExtensible(cx);
            return NULL;
        }

        return addPropertyInternal(cx, obj, id, getter, setter, slot, attrs, flags, shortid, spp, true);
    }

    /* Property exists: search must have returned a valid *spp. */
    JS_ASSERT_IF(spp, !SHAPE_IS_REMOVED(*spp));

    if (!CheckCanChangeAttrs(cx, obj, shape, &attrs))
        return NULL;

    /*
     * If the caller wants to allocate a slot, but doesn't care which slot,
     * copy the existing shape's slot into slot so we can match shape, if all
     * other members match.
     */
    bool hadSlot = shape->hasSlot();
    uint32_t oldSlot = shape->maybeSlot();
    if (!(attrs & JSPROP_SHARED) && slot == SHAPE_INVALID_SLOT && hadSlot)
        slot = oldSlot;

    Rooted<UnownedBaseShape*> nbase(cx);
    {
        uint32_t index;
        bool indexed = js_IdIsIndex(id, &index);
        StackBaseShape base(obj->lastProperty()->base());
        base.updateGetterSetter(attrs, getter, setter);
        if (indexed)
            base.flags |= BaseShape::INDEXED;
        nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return NULL;
    }

    /*
     * Now that we've possibly preserved slot, check whether all members match.
     * If so, this is a redundant "put" and we can return without more work.
     */
    if (shape->matchesParamsAfterId(nbase, slot, attrs, flags, shortid))
        return shape;

    /*
     * Overwriting a non-last property requires switching to dictionary mode.
     * The shape tree is shared immutable, and we can't removeProperty and then
     * addPropertyInternal because a failure under add would lose data.
     */
    if (shape != obj->lastProperty() && !obj->inDictionaryMode()) {
        if (!obj->toDictionaryMode(cx))
            return NULL;
        spp = obj->lastProperty()->table().search(shape->propid(), false);
        shape = SHAPE_FETCH(spp);
    }

    JS_ASSERT_IF(shape->hasSlot() && !(attrs & JSPROP_SHARED), shape->slot() == slot);

    if (obj->inDictionaryMode()) {
        /*
         * Updating some property in a dictionary-mode object. Create a new
         * shape for the existing property, and also generate a new shape for
         * the last property of the dictionary (unless the modified property
         * is also the last property).
         */
        bool updateLast = (shape == obj->lastProperty());
        shape = obj->replaceWithNewEquivalentShape(cx, shape);
        if (!shape)
            return NULL;
        if (!updateLast && !obj->generateOwnShape(cx))
            return NULL;

        /* FIXME bug 593129 -- slot allocation and JSObject *this must move out of here! */
        if (slot == SHAPE_INVALID_SLOT && !(attrs & JSPROP_SHARED)) {
            if (!allocSlot(cx, obj, &slot))
                return NULL;
        }

        if (updateLast)
            shape->base()->adoptUnowned(nbase);
        else
            shape->base_ = nbase;

        shape->setSlot(slot);
        shape->attrs = uint8_t(attrs);
        shape->flags = flags | Shape::IN_DICTIONARY;
        shape->shortid_ = int16_t(shortid);
    } else {
        /*
         * Updating the last property in a non-dictionary-mode object. Find an
         * alternate shared child of the last property's previous shape.
         */
        StackBaseShape base(obj->lastProperty()->base());
        base.updateGetterSetter(attrs, getter, setter);

        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return NULL;

        JS_ASSERT(shape == obj->lastProperty());

        /* Find or create a property tree node labeled by our arguments. */
        StackShape child(nbase, id, slot, obj->numFixedSlots(), attrs, flags, shortid);
        RootedShape parent(cx, shape->parent);
        Shape *newShape = JSObject::getChildProperty(cx, obj, parent, child);

        if (!newShape) {
            obj->checkShapeConsistency();
            return NULL;
        }

        shape = newShape;
    }

    /*
     * Can't fail now, so free the previous incarnation's slot if the new shape
     * has no slot. But we do not need to free oldSlot (and must not, as trying
     * to will botch an assertion in JSObject::freeSlot) if the new last
     * property (shape here) has a slotSpan that does not cover it.
     */
    if (hadSlot && !shape->hasSlot()) {
        if (oldSlot < obj->slotSpan())
            obj->freeSlot(oldSlot);
        ++cx->runtime()->propertyRemovals;
    }

    obj->checkShapeConsistency();

    return shape;
}

/* static */ Shape *
JSObject::changeProperty(JSContext *cx, HandleObject obj, HandleShape shape, unsigned attrs,
                         unsigned mask, PropertyOp getter, StrictPropertyOp setter)
{
    JS_ASSERT(obj->nativeContains(cx, shape));

    attrs |= shape->attrs & mask;

    /* Allow only shared (slotless) => unshared (slotful) transition. */
    JS_ASSERT(!((attrs ^ shape->attrs) & JSPROP_SHARED) ||
              !(attrs & JSPROP_SHARED));

    types::MarkTypePropertyConfigured(cx, obj, shape->propid());
    if (attrs & (JSPROP_GETTER | JSPROP_SETTER))
        types::AddTypePropertyId(cx, obj, shape->propid(), types::Type::UnknownType());

    if (getter == JS_PropertyStub)
        getter = NULL;
    if (setter == JS_StrictPropertyStub)
        setter = NULL;

    if (!CheckCanChangeAttrs(cx, obj, shape, &attrs))
        return NULL;

    if (shape->attrs == attrs && shape->getter() == getter && shape->setter() == setter)
        return shape;

    /*
     * Let JSObject::putProperty handle this |overwriting| case, including
     * the conservation of shape->slot (if it's valid). We must not call
     * removeProperty because it will free an allocated shape->slot, and
     * putProperty won't re-allocate it.
     */
    RootedId propid(cx, shape->propid());
    Shape *newShape = putProperty(cx, obj, propid, getter, setter, shape->maybeSlot(),
                                    attrs, shape->flags, shape->maybeShortid());

    obj->checkShapeConsistency();
    return newShape;
}

bool
JSObject::removeProperty(JSContext *cx, jsid id_)
{
    RootedId id(cx, id_);
    RootedObject self(cx, this);

    Shape **spp;
    RootedShape shape(cx, Shape::search(cx, lastProperty(), id, &spp));
    if (!shape)
        return true;

    /*
     * If shape is not the last property added, or the last property cannot
     * be removed, switch to dictionary mode.
     */
    if (!self->inDictionaryMode() && (shape != self->lastProperty() || !self->canRemoveLastProperty())) {
        if (!self->toDictionaryMode(cx))
            return false;
        spp = self->lastProperty()->table().search(shape->propid(), false);
        shape = SHAPE_FETCH(spp);
    }

    /*
     * If in dictionary mode, get a new shape for the last property after the
     * removal. We need a fresh shape for all dictionary deletions, even of
     * the last property. Otherwise, a shape could replay and caches might
     * return deleted DictionaryShapes! See bug 595365. Do this before changing
     * the object or table, so the remaining removal is infallible.
     */
    RootedShape spare(cx);
    if (self->inDictionaryMode()) {
        spare = js_NewGCShape(cx);
        if (!spare)
            return false;
        new (spare) Shape(shape->base()->unowned(), 0);
        if (shape == self->lastProperty()) {
            /*
             * Get an up to date unowned base shape for the new last property
             * when removing the dictionary's last property. Information in
             * base shapes for non-last properties may be out of sync with the
             * object's state.
             */
            RootedShape previous(cx, self->lastProperty()->parent);
            StackBaseShape base(self->lastProperty()->base());
            base.updateGetterSetter(previous->attrs, previous->getter(), previous->setter());
            BaseShape *nbase = BaseShape::getUnowned(cx, base);
            if (!nbase)
                return false;
            previous->base_ = nbase;
        }
    }

    /* If shape has a slot, free its slot number. */
    if (shape->hasSlot()) {
        self->freeSlot(shape->slot());
        ++cx->runtime()->propertyRemovals;
    }

    /*
     * A dictionary-mode object owns mutable, unique shapes on a non-circular
     * doubly linked list, hashed by lastProperty()->table. So we can edit the
     * list and hash in place.
     */
    if (self->inDictionaryMode()) {
        ShapeTable &table = self->lastProperty()->table();

        if (SHAPE_HAD_COLLISION(*spp)) {
            *spp = SHAPE_REMOVED;
            ++table.removedCount;
            --table.entryCount;
        } else {
            *spp = NULL;
            --table.entryCount;

#ifdef DEBUG
            /*
             * Check the consistency of the table but limit the number of
             * checks not to alter significantly the complexity of the
             * delete in debug builds, see bug 534493.
             */
            Shape *aprop = self->lastProperty();
            for (int n = 50; --n >= 0 && aprop->parent; aprop = aprop->parent)
                JS_ASSERT_IF(aprop != shape, self->nativeContains(cx, aprop));
#endif
        }

        {
            /* Remove shape from its non-circular doubly linked list. */
            Shape *oldLastProp = self->lastProperty();
            shape->removeFromDictionary(self);

            /* Hand off table from the old to new last property. */
            oldLastProp->handoffTableTo(self->lastProperty());
        }

        /* Generate a new shape for the object, infallibly. */
        JS_ALWAYS_TRUE(self->generateOwnShape(cx, spare));

        /* Consider shrinking table if its load factor is <= .25. */
        uint32_t size = table.capacity();
        if (size > ShapeTable::MIN_SIZE && table.entryCount <= size >> 2)
            (void) table.change(-1, cx);
    } else {
        /*
         * Non-dictionary-mode shape tables are shared immutables, so all we
         * need do is retract the last property and we'll either get or else
         * lazily make via a later hashify the exact table for the new property
         * lineage.
         */
        JS_ASSERT(shape == self->lastProperty());
        self->removeLastProperty(cx);
    }

    self->checkShapeConsistency();
    return true;
}

/* static */ void
JSObject::clear(JSContext *cx, HandleObject obj)
{
    RootedShape shape(cx, obj->lastProperty());
    JS_ASSERT(obj->inDictionaryMode() == shape->inDictionary());

    while (shape->parent) {
        shape = shape->parent;
        JS_ASSERT(obj->inDictionaryMode() == shape->inDictionary());
    }
    JS_ASSERT(shape->isEmptyShape());

    if (obj->inDictionaryMode())
        shape->listp = &obj->shape_;

    JS_ALWAYS_TRUE(JSObject::setLastProperty(cx, obj, shape));

    ++cx->runtime()->propertyRemovals;
    obj->checkShapeConsistency();
}

void
JSObject::rollbackProperties(JSContext *cx, uint32_t slotSpan)
{
    /*
     * Remove properties from this object until it has a matching slot span.
     * The object cannot have escaped in a way which would prevent safe
     * removal of the last properties.
     */
    JS_ASSERT(!inDictionaryMode() && slotSpan <= this->slotSpan());
    while (this->slotSpan() != slotSpan) {
        JS_ASSERT(lastProperty()->hasSlot() && getSlot(lastProperty()->slot()).isUndefined());
        removeLastProperty(cx);
    }
}

Shape *
js::ObjectImpl::replaceWithNewEquivalentShape(JSContext *cx, Shape *oldShape, Shape *newShape)
{
    JS_ASSERT(cx->compartment() == oldShape->compartment());
    JS_ASSERT_IF(oldShape != lastProperty(),
                 inDictionaryMode() &&
                 nativeLookup(cx, oldShape->propidRef()) == oldShape);

    ObjectImpl *self = this;

    if (!inDictionaryMode()) {
        Rooted<ObjectImpl*> selfRoot(cx, self);
        RootedShape newRoot(cx, newShape);
        if (!toDictionaryMode(cx))
            return NULL;
        oldShape = selfRoot->lastProperty();
        self = selfRoot;
        newShape = newRoot;
    }

    if (!newShape) {
        Rooted<ObjectImpl*> selfRoot(cx, self);
        RootedShape oldRoot(cx, oldShape);
        newShape = js_NewGCShape(cx);
        if (!newShape)
            return NULL;
        new (newShape) Shape(oldRoot->base()->unowned(), 0);
        self = selfRoot;
        oldShape = oldRoot;
    }

    ShapeTable &table = self->lastProperty()->table();
    Shape **spp = oldShape->isEmptyShape()
                  ? NULL
                  : table.search(oldShape->propidRef(), false);

    /*
     * Splice the new shape into the same position as the old shape, preserving
     * enumeration order (see bug 601399).
     */
    StackShape nshape(oldShape);
    newShape->initDictionaryShape(nshape, self->numFixedSlots(), oldShape->listp);

    JS_ASSERT(newShape->parent == oldShape);
    oldShape->removeFromDictionary(self);

    if (newShape == self->lastProperty())
        oldShape->handoffTableTo(newShape);

    if (spp)
        SHAPE_STORE_PRESERVING_COLLISION(spp, newShape);
    return newShape;
}

bool
JSObject::shadowingShapeChange(JSContext *cx, const Shape &shape)
{
    return generateOwnShape(cx);
}

/* static */ bool
JSObject::clearParent(JSContext *cx, HandleObject obj)
{
    return setParent(cx, obj, NullPtr());
}

/* static */ bool
JSObject::setParent(JSContext *cx, HandleObject obj, HandleObject parent)
{
    if (parent && !parent->setDelegate(cx))
        return false;

    if (obj->inDictionaryMode()) {
        StackBaseShape base(obj->lastProperty());
        base.parent = parent;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        obj->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape = Shape::setObjectParent(cx, parent, obj->getTaggedProto(), obj->shape_);
    if (!newShape)
        return false;

    obj->shape_ = newShape;
    return true;
}

/* static */ Shape *
Shape::setObjectParent(JSContext *cx, JSObject *parent, TaggedProto proto, Shape *last)
{
    if (last->getObjectParent() == parent)
        return last;

    StackBaseShape base(last);
    base.parent = parent;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ bool
JSObject::setMetadata(JSContext *cx, HandleObject obj, HandleObject metadata)
{
    if (obj->inDictionaryMode()) {
        StackBaseShape base(obj->lastProperty());
        base.metadata = metadata;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        obj->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape = Shape::setObjectMetadata(cx, metadata, obj->getTaggedProto(), obj->shape_);
    if (!newShape)
        return false;

    obj->shape_ = newShape;
    return true;
}

/* static */ Shape *
Shape::setObjectMetadata(JSContext *cx, JSObject *metadata, TaggedProto proto, Shape *last)
{
    if (last->getObjectMetadata() == metadata)
        return last;

    StackBaseShape base(last);
    base.metadata = metadata;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ bool
js::ObjectImpl::preventExtensions(JSContext *cx, Handle<ObjectImpl*> obj)
{
    MOZ_ASSERT(obj->isExtensible(),
               "Callers must ensure |obj| is extensible before calling "
               "preventExtensions");

    if (obj->isProxy()) {
        RootedObject object(cx, obj->asObjectPtr());
        return js::Proxy::preventExtensions(cx, object);
    }

    RootedObject self(cx, obj->asObjectPtr());

    /*
     * Force lazy properties to be resolved by iterating over the objects' own
     * properties.
     */
    AutoIdVector props(cx);
    if (!js::GetPropertyNames(cx, self, JSITER_HIDDEN | JSITER_OWNONLY, &props))
        return false;

    /*
     * Convert all dense elements to sparse properties. This will shrink the
     * initialized length and capacity of the object to zero and ensure that no
     * new dense elements can be added without calling growElements(), which
     * checks isExtensible().
     */
    if (self->isNative() && !JSObject::sparsifyDenseElements(cx, self))
        return false;

    return self->setFlag(cx, BaseShape::NOT_EXTENSIBLE, GENERATE_SHAPE);
}

bool
js::ObjectImpl::setFlag(JSContext *cx, /*BaseShape::Flag*/ uint32_t flag_,
                        GenerateShape generateShape)
{
    BaseShape::Flag flag = (BaseShape::Flag) flag_;

    if (lastProperty()->getObjectFlags() & flag)
        return true;

    Rooted<ObjectImpl*> self(cx, this);

    if (inDictionaryMode()) {
        if (generateShape == GENERATE_SHAPE && !generateOwnShape(cx))
            return false;
        StackBaseShape base(self->lastProperty());
        base.flags |= flag;
        UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
        if (!nbase)
            return false;

        self->lastProperty()->base()->adoptUnowned(nbase);
        return true;
    }

    Shape *newShape =
        Shape::setObjectFlag(cx, flag, self->getTaggedProto(), self->lastProperty());
    if (!newShape)
        return false;

    self->shape_ = newShape;
    return true;
}

bool
js::ObjectImpl::clearFlag(JSContext *cx, /*BaseShape::Flag*/ uint32_t flag)
{
    JS_ASSERT(inDictionaryMode());
    JS_ASSERT(lastProperty()->getObjectFlags() & flag);

    RootedObject self(cx, this->asObjectPtr());

    StackBaseShape base(self->lastProperty());
    base.flags &= ~flag;
    UnownedBaseShape *nbase = BaseShape::getUnowned(cx, base);
    if (!nbase)
        return false;

    self->lastProperty()->base()->adoptUnowned(nbase);
    return true;
}

/* static */ Shape *
Shape::setObjectFlag(JSContext *cx, BaseShape::Flag flag, TaggedProto proto, Shape *last)
{
    if (last->getObjectFlags() & flag)
        return last;

    StackBaseShape base(last);
    base.flags |= flag;

    RootedShape lastRoot(cx, last);
    return replaceLastProperty(cx, base, proto, lastRoot);
}

/* static */ inline HashNumber
StackBaseShape::hash(const StackBaseShape *base)
{
    HashNumber hash = base->flags;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (uintptr_t(base->clasp) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (uintptr_t(base->parent) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (uintptr_t(base->metadata) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ uintptr_t(base->rawGetter);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ uintptr_t(base->rawSetter);
    return hash;
}

/* static */ inline bool
StackBaseShape::match(UnownedBaseShape *key, const StackBaseShape *lookup)
{
    return key->flags == lookup->flags
        && key->clasp == lookup->clasp
        && key->parent == lookup->parent
        && key->metadata == lookup->metadata
        && key->rawGetter == lookup->rawGetter
        && key->rawSetter == lookup->rawSetter;
}

void
StackBaseShape::AutoRooter::trace(JSTracer *trc)
{
    if (base->parent) {
        gc::MarkObjectRoot(trc, (JSObject**)&base->parent,
                           "StackBaseShape::AutoRooter parent");
    }
    if (base->metadata) {
        gc::MarkObjectRoot(trc, (JSObject**)&base->metadata,
                           "StackBaseShape::AutoRooter metadata");
    }
    if ((base->flags & BaseShape::HAS_GETTER_OBJECT) && base->rawGetter) {
        gc::MarkObjectRoot(trc, (JSObject**)&base->rawGetter,
                           "StackBaseShape::AutoRooter getter");
    }
    if ((base->flags & BaseShape::HAS_SETTER_OBJECT) && base->rawSetter) {
        gc::MarkObjectRoot(trc, (JSObject**)&base->rawSetter,
                           "StackBaseShape::AutoRooter setter");
    }
}

/* static */ UnownedBaseShape*
BaseShape::getUnowned(JSContext *cx, const StackBaseShape &base)
{
    BaseShapeSet &table = cx->compartment()->baseShapes;

    if (!table.initialized() && !table.init())
        return NULL;

    BaseShapeSet::AddPtr p = table.lookupForAdd(&base);

    if (p)
        return *p;

    StackBaseShape::AutoRooter root(cx, &base);

    BaseShape *nbase_ = js_NewGCBaseShape<CanGC>(cx);
    if (!nbase_)
        return NULL;

    new (nbase_) BaseShape(base);

    UnownedBaseShape *nbase = static_cast<UnownedBaseShape *>(nbase_);

    if (!table.relookupOrAdd(p, &base, nbase))
        return NULL;

    return nbase;
}

void
JSCompartment::sweepBaseShapeTable()
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_BASE_SHAPE);

    if (baseShapes.initialized()) {
        for (BaseShapeSet::Enum e(baseShapes); !e.empty(); e.popFront()) {
            UnownedBaseShape *base = e.front();
            if (IsBaseShapeAboutToBeFinalized(&base))
                e.removeFront();
        }
    }
}

void
BaseShape::finalize(FreeOp *fop)
{
    if (table_) {
        fop->delete_(table_);
        table_ = NULL;
    }
}

inline
InitialShapeEntry::InitialShapeEntry() : shape(NULL), proto(NULL)
{
}

inline
InitialShapeEntry::InitialShapeEntry(const ReadBarriered<Shape> &shape, TaggedProto proto)
  : shape(shape), proto(proto)
{
}

inline InitialShapeEntry::Lookup
InitialShapeEntry::getLookup() const
{
    return Lookup(shape->getObjectClass(), proto, shape->getObjectParent(), shape->getObjectMetadata(),
                  shape->numFixedSlots(), shape->getObjectFlags());
}

/* static */ inline HashNumber
InitialShapeEntry::hash(const Lookup &lookup)
{
    HashNumber hash = uintptr_t(lookup.clasp) >> 3;
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (uintptr_t(lookup.proto.toWord()) >> 3);
    hash = JS_ROTATE_LEFT32(hash, 4) ^ (uintptr_t(lookup.parent) >> 3) ^ (uintptr_t(lookup.metadata) >> 3);
    return hash + lookup.nfixed;
}

/* static */ inline bool
InitialShapeEntry::match(const InitialShapeEntry &key, const Lookup &lookup)
{
    const Shape *shape = *key.shape.unsafeGet();
    return lookup.clasp == shape->getObjectClass()
        && lookup.proto.toWord() == key.proto.toWord()
        && lookup.parent == shape->getObjectParent()
        && lookup.metadata == shape->getObjectMetadata()
        && lookup.nfixed == shape->numFixedSlots()
        && lookup.baseFlags == shape->getObjectFlags();
}

/* static */ Shape *
EmptyShape::getInitialShape(JSContext *cx, Class *clasp, TaggedProto proto,
                            JSObject *parent, JSObject *metadata,
                            size_t nfixed, uint32_t objectFlags)
{
    JS_ASSERT_IF(proto.isObject(), cx->compartment() == proto.toObject()->compartment());
    JS_ASSERT_IF(parent, cx->compartment() == parent->compartment());

    InitialShapeSet &table = cx->compartment()->initialShapes;

    if (!table.initialized() && !table.init())
        return NULL;

    typedef InitialShapeEntry::Lookup Lookup;
    InitialShapeSet::AddPtr p =
        table.lookupForAdd(Lookup(clasp, proto, parent, metadata, nfixed, objectFlags));

    if (p)
        return p->shape;

    SkipRoot skip(cx, &p); /* The hash may look like a GC pointer and get poisoned. */
    Rooted<TaggedProto> protoRoot(cx, proto);
    RootedObject parentRoot(cx, parent);
    RootedObject metadataRoot(cx, metadata);

    StackBaseShape base(cx->compartment(), clasp, parent, metadata, objectFlags);
    Rooted<UnownedBaseShape*> nbase(cx, BaseShape::getUnowned(cx, base));
    if (!nbase)
        return NULL;

    Shape *shape = cx->propertyTree().newShape(cx);
    if (!shape)
        return NULL;
    new (shape) EmptyShape(nbase, nfixed);

    if (!table.relookupOrAdd(p, Lookup(clasp, protoRoot, parentRoot, metadataRoot, nfixed, objectFlags),
                             InitialShapeEntry(shape, protoRoot)))
    {
        return NULL;
    }

    return shape;
}

/* static */ Shape *
EmptyShape::getInitialShape(JSContext *cx, Class *clasp, TaggedProto proto,
                            JSObject *parent, JSObject *metadata,
                            AllocKind kind, uint32_t objectFlags)
{
    return getInitialShape(cx, clasp, proto, parent, metadata, GetGCKindSlots(kind, clasp), objectFlags);
}

void
NewObjectCache::invalidateEntriesForShape(JSContext *cx, HandleShape shape, HandleObject proto)
{
    Class *clasp = shape->getObjectClass();

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    if (CanBeFinalizedInBackground(kind, clasp))
        kind = GetBackgroundAllocKind(kind);

    Rooted<GlobalObject *> global(cx, &shape->getObjectParent()->global());
    Rooted<types::TypeObject *> type(cx, proto->getNewType(cx, clasp));

    EntryIndex entry;
    if (lookupGlobal(clasp, global, kind, &entry))
        PodZero(&entries[entry]);
    if (!proto->is<GlobalObject>() && lookupProto(clasp, proto, kind, &entry))
        PodZero(&entries[entry]);
    if (lookupType(clasp, type, kind, &entry))
        PodZero(&entries[entry]);
}

/* static */ void
EmptyShape::insertInitialShape(JSContext *cx, HandleShape shape, HandleObject proto)
{
    InitialShapeEntry::Lookup lookup(shape->getObjectClass(), TaggedProto(proto),
                                     shape->getObjectParent(), shape->getObjectMetadata(),
                                     shape->numFixedSlots(), shape->getObjectFlags());

    InitialShapeSet::Ptr p = cx->compartment()->initialShapes.lookup(lookup);
    JS_ASSERT(p);

    InitialShapeEntry &entry = const_cast<InitialShapeEntry &>(*p);
    JS_ASSERT(entry.shape->isEmptyShape());

    /* The new shape had better be rooted at the old one. */
#ifdef DEBUG
    Shape *nshape = shape;
    while (!nshape->isEmptyShape())
        nshape = nshape->previous();
    JS_ASSERT(nshape == entry.shape);
#endif

    entry.shape = shape.get();

    /*
     * This affects the shape that will be produced by the various NewObject
     * methods, so clear any cache entry referring to the old shape. This is
     * not required for correctness (though it may bust on the above asserts):
     * the NewObject must always check for a nativeEmpty() result and generate
     * the appropriate properties if found. Clearing the cache entry avoids
     * this duplicate regeneration.
     */
    cx->runtime()->newObjectCache.invalidateEntriesForShape(cx, shape, proto);
}

/*
 * This is called by the minor GC to ensure that any relocated proto objects
 * get updated in the shape table.
 */
void
JSCompartment::markAllInitialShapeTableEntries(JSTracer *trc)
{
    if (!initialShapes.initialized())
        return;

    for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
        if (!e.front().proto.isObject())
            continue;
        JSObject *proto = e.front().proto.toObject();
        JS_SET_TRACING_LOCATION(trc, (void*)&e.front().proto);
        MarkObjectRoot(trc, &proto, "InitialShapeSet proto");
        if (proto != e.front().proto.toObject()) {
            InitialShapeEntry moved = e.front();
            moved.proto = TaggedProto(proto);
            e.rekeyFront(e.front().getLookup(), moved);
        }
    }
}

void
JSCompartment::sweepInitialShapeTable()
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_INITIAL_SHAPE);

    if (initialShapes.initialized()) {
        for (InitialShapeSet::Enum e(initialShapes); !e.empty(); e.popFront()) {
            const InitialShapeEntry &entry = e.front();
            Shape *shape = entry.shape;
            JSObject *proto = entry.proto.raw();
            if (IsShapeAboutToBeFinalized(&shape) || (entry.proto.isObject() && IsObjectAboutToBeFinalized(&proto))) {
                e.removeFront();
            } else {
#ifdef DEBUG
                DebugOnly<JSObject *> parent = shape->getObjectParent();
                JS_ASSERT(!parent || !IsObjectAboutToBeFinalized(&parent));
                JS_ASSERT(parent == shape->getObjectParent());
#endif
                if (shape != entry.shape || proto != entry.proto.raw()) {
                    InitialShapeEntry newKey(shape, proto);
                    e.rekeyFront(newKey.getLookup(), newKey);
                }
            }
        }
    }
}

void
AutoRooterGetterSetter::Inner::trace(JSTracer *trc)
{
    if ((attrs & JSPROP_GETTER) && *pgetter)
        gc::MarkObjectRoot(trc, (JSObject**) pgetter, "AutoRooterGetterSetter getter");
    if ((attrs & JSPROP_SETTER) && *psetter)
        gc::MarkObjectRoot(trc, (JSObject**) psetter, "AutoRooterGetterSetter setter");
}
