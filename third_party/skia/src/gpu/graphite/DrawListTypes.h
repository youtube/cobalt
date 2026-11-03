/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_graphite_DrawListTypes_DEFINED
#define skgpu_graphite_DrawListTypes_DEFINED

#include "include/private/base/SkDebug.h"
#include "src/base/SkBlockAllocator.h"
#include "src/base/SkEnumBitMask.h"
#include "src/base/SkTBlockList.h"
#include "src/base/SkTInternalLList.h"
#include "src/gpu/graphite/DrawOrder.h"
#include "src/gpu/graphite/DrawParams.h"
#include "src/gpu/graphite/DrawTypes.h"
#include "src/gpu/graphite/PaintParams.h"
#include "src/gpu/graphite/PipelineData.h"
#include "src/gpu/graphite/geom/Rect.h"
#include "src/gpu/graphite/geom/Transform.h"

#include <cstdint>
#include <functional>
#include <optional>

namespace skgpu::graphite {
enum class BoundsTest {
    kDisjoint,
    kCompatibleOverlap,
    kIncompatibleOverlap,
};

enum class BindingListType {
    kSingle,
    kStencil,
};

struct LayerKey {
    GraphicsPipelineCache::Index fPipelineIndex;
    TextureDataCache::Index fTextureIndex;

    static constexpr LayerKey None() {
        constexpr LayerKey kInvalid = {GraphicsPipelineCache::kInvalidIndex,
                                       TextureDataCache::kInvalidIndex};
        return kInvalid;
    }

    // NOTE: removing the uniform index on this check decreases stencil lists on desk_samoa
    // from 602 -> 69
    bool operator==(const LayerKey& other) const {
        return fPipelineIndex == other.fPipelineIndex && fTextureIndex == other.fTextureIndex;
    }

    bool operator!=(const LayerKey& other) const { return !(*this == other); }
};

struct SingleDraw {
    SingleDraw(const DrawParams* params, const UniformDataCache::Index uniformIndex)
            : fDrawParams(params), fUniformIndex(uniformIndex) {}
    static constexpr BindingListType kListType = BindingListType::kSingle;

    const DrawParams* fDrawParams;
    const UniformDataCache::Index fUniformIndex;

    SK_DECLARE_INTERNAL_LLIST_INTERFACE(SingleDraw);
};

struct StencilDraws {
    StencilDraws(const LayerKey& key, const RenderStep* step) : fKey(key), fStep(step) {}

    SkTInternalLList<SingleDraw> fDraws;
    const LayerKey fKey;
    const RenderStep* fStep;
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(StencilDraws);
};

struct BindingWrapper {
    BindingWrapper(BindingListType type, const CompressedPaintersOrder& order, bool isDepthOnly)
            : fType(type), fOrder(order), fIsDepthOnly(isDepthOnly) {}

    const BindingListType fType;
    CompressedPaintersOrder fOrder;
    const bool fIsDepthOnly;
    Rect fBounds = Rect::InfiniteInverted();

    SK_DECLARE_INTERNAL_LLIST_INTERFACE(BindingWrapper);
};

struct SingleDrawList : public BindingWrapper {
    SingleDrawList(BindingListType type, const CompressedPaintersOrder& order, bool isDepthOnly)
            : BindingWrapper(type, order, isDepthOnly) {}
    LayerKey fKey;
    RenderStep* fStep;
    SkTInternalLList<SingleDraw> fDraws;
};

struct StencilDrawList : public BindingWrapper {
    StencilDrawList(BindingListType type, const CompressedPaintersOrder& order, bool isDepthOnly)
            : BindingWrapper(type, order, isDepthOnly) {}
    SkTInternalLList<StencilDraws> fStencilDraws;
};

struct Layer {
    Layer(const CompressedPaintersOrder& order) : fOrder(order) {}

    const CompressedPaintersOrder fOrder;
    CompressedPaintersOrder fListOrder = CompressedPaintersOrder::First();
    SkTInternalLList<BindingWrapper> fBindings;
    SK_DECLARE_INTERNAL_LLIST_INTERFACE(Layer);

    // Search backwards and towards the start, inclusive. Matching on the startList is valid, as
    // the insertion is guaranteed to be appendTail
    BindingWrapper* searchBinding(const LayerKey& key, BindingWrapper* startList) {
        BindingWrapper* end = startList ? startList->fPrev : nullptr;
        for (BindingWrapper* list = fBindings.tail(); list != end; list = list->fPrev) {
            if (list->fType == BindingListType::kSingle) {
                SingleDrawList* single = static_cast<SingleDrawList*>(list);
                if (single->fKey == key) {
                    return list;
                }
            }
        }
        return nullptr;
    }

    // Note, for the purposes of allowing intersections with non-shading draws, we only delineate
    // between depthOnlyDraws and nonDepthOnly draws. Although the stencil part of stencil renderers
    // are also non-shading, and thus could be bypassed by shading draws, in practice there are very
    // few scenarios where this increases batching and/or performance. This is because---regardless
    // of the direction of the traversal---the shading part of the stencil renderer is 1) likely
    // very close by 2) will stop any dependsOnDst draw anyways.
    //
    // This was implemented in https://review.skia.org/1171836 and slightly regresses performance
    // due to the overhead it introduces.
    template <bool kIsStencil, bool kIsDepthOnly = false, bool kForwards>
    SK_ALWAYS_INLINE std::pair<BoundsTest, BindingWrapper*> test(const Rect& drawBounds,
                                                                 const LayerKey& key,
                                                                 bool requiresBarrier,
                                                                 BindingWrapper* startList) {
        BindingWrapper* foundMatch = nullptr;
        BindingWrapper* list;
        BindingWrapper* end;
        if constexpr (kForwards) {
            list = startList ? startList : fBindings.head();
            end = nullptr;
        } else {
            list = fBindings.tail();
            end = startList ? startList->fPrev : nullptr;
        }
        // Advancement is also constexpr
        for (; list != end; list = kForwards ? list->fNext : list->fPrev) {
            if constexpr (kIsStencil) {
                if (list->fType == BindingListType::kStencil) {
                    StencilDrawList* stencil = static_cast<StencilDrawList*>(list);
                    for (StencilDraws* s = stencil->fStencilDraws.head(); s; s = s->fNext) {
                        if (s->fKey == key) {
                            foundMatch = list;
                            break;
                        }
                    }
                }
            } else {
                if (list->fType == BindingListType::kSingle) {
                    SingleDrawList* single = static_cast<SingleDrawList*>(list);
                    if (single->fKey == key) {
                        foundMatch = list;
                        // A depth only draw should not require a barrier.
                        if constexpr (!kIsDepthOnly) {
                            if (!requiresBarrier) continue;
                        }
                    }
                }
            }

            // Stencil draws always check for intersection. If it's not a stencil draw, it is either
            // a shading or depth-only draw. Both are allowed to intersect freely with existing
            // depth-only draws for different reasons:
            //
            // 1. Shading bypassing Depth-Only: An unclipped shading draw does not depend on extant
            //    depth masks. By bypassing it and drawing earlier, it safely skips a depth test
            //    that it naturally would have passed anyway (due to having a closer Z-value).
            //    Clipped shading draws are prevented from bypassing their parent depth-only draws
            //    by the stop-layer insertion mechanism, not by intersection testing.
            //
            // 2. Depth-Only bypassing Depth-Only: Because the hardware depth test min/maxs to
            //    retain the "closest" Z-value, depth writes are commutative. I.e. the greatest
            //    /least Z-value is retained regardless of draw-ordering. This allows
            //    intersecting depth-only draws to be safely reordered.
            //
            // However, an incoming depth-only draw may NOT bypass an extant shading draws. This is
            // because writing a closer Z-value would cause the shading draw to fail the depth test.
            if constexpr (!kIsStencil) {
                if (!list->fIsDepthOnly && list->fBounds.intersects(drawBounds)) {
                    return {BoundsTest::kIncompatibleOverlap, foundMatch};
                }
            } else {
                if (list->fBounds.intersects(drawBounds)) {
                    return {BoundsTest::kIncompatibleOverlap, foundMatch};
                }
            }
        }

        // Note, !foundMatch, but kDisjoint is functionally the same as a kCompatibleOverlap
        return {foundMatch ? BoundsTest::kCompatibleOverlap : BoundsTest::kDisjoint, foundMatch};
    }

    template <bool kIsDepthOnly = false>
    SK_ALWAYS_INLINE BindingWrapper* add(SkArenaAllocWithReset* alloc,
                                         BindingWrapper* match,
                                         const LayerKey& key,
                                         SingleDraw* draw,
                                         const RenderStep* step,
                                         bool insertBefore) {
        SingleDrawList* single;
        if (match) {
            single = static_cast<SingleDrawList*>(match);
            single->fBounds.join(draw->fDrawParams->drawBounds());
        } else {
            fListOrder = fListOrder.next();
            single =
                    alloc->make<SingleDrawList>(BindingListType::kSingle, fListOrder, kIsDepthOnly);
            single->fKey = key;
            single->fStep = const_cast<RenderStep*>(step);
            single->fBounds = draw->fDrawParams->drawBounds();
            if constexpr (kIsDepthOnly) {
                fBindings.addToHead(single);
            } else {
                fBindings.addToTail(single);
            }
        }
        SkASSERT(kIsDepthOnly == single->fIsDepthOnly);

        if (insertBefore) {
            single->fDraws.addToHead(draw);
        } else {
            single->fDraws.addToTail(draw);
        }

        return single;
    }

    template <bool kIsDepthOnly = false>
    BindingWrapper* addStencil(SkArenaAllocWithReset* alloc,
                               BindingWrapper*& match,
                               const LayerKey& key,
                               SingleDraw* draw,
                               const RenderStep* step,
                               StencilDraws** startList) {
        StencilDrawList* stencil;
        StencilDraws* searchStart = nullptr;

        if (match) {
            stencil = static_cast<StencilDrawList*>(match);
            stencil->fBounds.join(draw->fDrawParams->drawBounds());
            searchStart = (*startList) ? (*startList)->fNext : stencil->fStencilDraws.head();
        } else {
            fListOrder = fListOrder.next();
            stencil = alloc->make<StencilDrawList>(
                    BindingListType::kStencil, fListOrder, kIsDepthOnly);
            stencil->fBounds = draw->fDrawParams->drawBounds();
            if constexpr (kIsDepthOnly) {
                fBindings.addToHead(stencil);
            } else {
                fBindings.addToTail(stencil);
            }
            match = stencil;
        }
        SkASSERT(kIsDepthOnly == stencil->fIsDepthOnly);

        StencilDraws* sd = nullptr;
        for (StencilDraws* curr = searchStart; curr; curr = curr->fNext) {
            if (curr->fKey == key) {
                sd = curr;
                break;
            }
        }

        if (!sd) {
            sd = alloc->make<StencilDraws>(key, step);
            stencil->fStencilDraws.addToTail(sd);
        }

        sd->fDraws.addToTail(draw);
        *startList = sd;
        return stencil;
    }
};

struct Insertion {
    Layer* fLayer = nullptr;
    BindingWrapper* fWrapper = nullptr;

    explicit operator bool() const { return (fLayer != nullptr) && (fWrapper != nullptr); }
    bool operator>(const Insertion& other) const {
        if (!other.fLayer) {
            return true;
        }
        return fLayer->fOrder > other.fLayer->fOrder;
    }
};

}  // namespace skgpu::graphite

#endif  // skgpu_graphite_DrawListTypes_DEFINED
