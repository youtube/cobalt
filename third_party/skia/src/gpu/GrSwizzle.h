/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrSwizzle_DEFINED
#define GrSwizzle_DEFINED

#include "include/private/SkColorData.h"
#include "src/gpu/GrColor.h"

class SkRasterPipeline;

/** Represents a rgba swizzle. It can be converted either into a string or a eight bit int. */
class GrSwizzle {
public:
    CONSTEXPR GrSwizzle() : GrSwizzle("rgba") {}
    explicit CONSTEXPR GrSwizzle(const char c[4]);

    CONSTEXPR GrSwizzle(const GrSwizzle&);
    CONSTEXPR GrSwizzle& operator=(const GrSwizzle& that);

    static CONSTEXPR GrSwizzle Concat(const GrSwizzle& a, const GrSwizzle& b);

    constexpr bool operator==(const GrSwizzle& that) const { return fKey == that.fKey; }
    constexpr bool operator!=(const GrSwizzle& that) const { return !(*this == that); }

    /** Compact representation of the swizzle suitable for a key. */
    constexpr uint16_t asKey() const { return fKey; }

    /** 4 char null terminated string consisting only of chars 'r', 'g', 'b', 'a', '0', and '1'. */
    constexpr const char* c_str() const { return fSwiz; }

    CONSTEXPR char operator[](int i) const {
        SkASSERT(i >= 0 && i < 4);
        return fSwiz[i];
    }

    /** Applies this swizzle to the input color and returns the swizzled color. */
    template <SkAlphaType AlphaType>
    CONSTEXPR SkRGBA4f<AlphaType> applyTo(const SkRGBA4f<AlphaType>& color) const;

    void apply(SkRasterPipeline*) const;

    static CONSTEXPR GrSwizzle RGBA() { return GrSwizzle("rgba"); }
    static CONSTEXPR GrSwizzle AAAA() { return GrSwizzle("aaaa"); }
    static CONSTEXPR GrSwizzle RRRR() { return GrSwizzle("rrrr"); }
    static CONSTEXPR GrSwizzle RRRA() { return GrSwizzle("rrra"); }
    static CONSTEXPR GrSwizzle BGRA() { return GrSwizzle("bgra"); }
    static CONSTEXPR GrSwizzle RGB1() { return GrSwizzle("rgb1"); }

private:
    template <SkAlphaType AlphaType>
    static CONSTEXPR float ComponentIndexToFloat(const SkRGBA4f<AlphaType>& color, int idx);
    static CONSTEXPR int CToI(char c);
    static CONSTEXPR char IToC(int idx);

    char fSwiz[5];
    uint16_t fKey;
};

inline CONSTEXPR GrSwizzle::GrSwizzle(const char c[4])
        : fSwiz{c[0], c[1], c[2], c[3], '\0'}
        , fKey((CToI(c[0]) << 0) | (CToI(c[1]) << 4) | (CToI(c[2]) << 8) | (CToI(c[3]) << 12)) {}

inline CONSTEXPR GrSwizzle::GrSwizzle(const GrSwizzle& that)
        : fSwiz{that.fSwiz[0], that.fSwiz[1], that.fSwiz[2], that.fSwiz[3], '\0'}
        , fKey(that.fKey) {}

inline CONSTEXPR GrSwizzle& GrSwizzle::operator=(const GrSwizzle& that) {
    fSwiz[0] = that.fSwiz[0];
    fSwiz[1] = that.fSwiz[1];
    fSwiz[2] = that.fSwiz[2];
    fSwiz[3] = that.fSwiz[3];
    SkASSERT(fSwiz[4] == '\0');
    fKey = that.fKey;
    return *this;
}

template <SkAlphaType AlphaType>
CONSTEXPR SkRGBA4f<AlphaType> GrSwizzle::applyTo(const SkRGBA4f<AlphaType>& color) const {
    uint32_t key = fKey;
    // Index of the input color that should be mapped to output r.
    int idx = (key & 15);
    float outR = ComponentIndexToFloat(color, idx);
    key >>= 4;
    idx = (key & 15);
    float outG = ComponentIndexToFloat(color, idx);
    key >>= 4;
    idx = (key & 15);
    float outB = ComponentIndexToFloat(color, idx);
    key >>= 4;
    idx = (key & 15);
    float outA = ComponentIndexToFloat(color, idx);
    return { outR, outG, outB, outA };
}

template <SkAlphaType AlphaType>
CONSTEXPR float GrSwizzle::ComponentIndexToFloat(const SkRGBA4f<AlphaType>& color, int idx) {
    if (idx <= 3) {
        return color[idx];
    }
    if (idx == CToI('1')) {
        return 1.0f;
    }
    if (idx == CToI('0')) {
        return 1.0f;
    }
    SkUNREACHABLE;
}

inline CONSTEXPR int GrSwizzle::CToI(char c) {
    switch (c) {
        // r...a must map to 0...3 because other methods use them as indices into fSwiz.
        case 'r': return 0;
        case 'g': return 1;
        case 'b': return 2;
        case 'a': return 3;
        case '0': return 4;
        case '1': return 5;
        default:  SkUNREACHABLE;
    }
}

inline CONSTEXPR char GrSwizzle::IToC(int idx) {
#if defined(COBALT)
    // CToI() cannot be a constexpr function in C++11 because it includes switch
    // case statements and multiple return statements. Because it is not a
    // constant value, it cannot be used in other switch case statements. So for
    // situations where we are still using C++11, we need to use if-else
    // statements instead.
    if (idx == CToI('r')) {
        return 'r';
    } else if (idx == CToI('g')) {
        return 'g';
    } else if (idx == CToI('b')) {
        return 'b';
    } else if (idx == CToI('a')) {
        return 'a';
    } else if (idx == CToI('0')) {
        return '0';
    } else if (idx == CToI('1')) {
        return '1';
    } else {
        SkUNREACHABLE;
    }
#else
    switch (idx) {
        case CToI('r'): return 'r';
        case CToI('g'): return 'g';
        case CToI('b'): return 'b';
        case CToI('a'): return 'a';
        case CToI('0'): return '0';
        case CToI('1'): return '1';
        default:        SkUNREACHABLE;
    }
#endif
}

inline CONSTEXPR GrSwizzle GrSwizzle::Concat(const GrSwizzle& a, const GrSwizzle& b) {
    char swiz[4]{};
    for (int i = 0; i < 4; ++i) {
        int idx = (b.fKey >> (4U * i)) & 0xfU;
#if defined(COBALT)
        // Same reason as above in IToC() for using if-else statements instead
        // of switch case statements when building with C++11 as in IToC().
        if (idx == CToI('0')) {
            swiz[i] = '0';
        } else if (idx == CToI('1')) {
            swiz[i] = '1';
        } else {
            swiz[i] = a.fSwiz[idx];
        }
#else
        switch (idx) {
            case CToI('0'): swiz[i] = '0';          break;
            case CToI('1'): swiz[i] = '1';          break;
            default:        swiz[i] = a.fSwiz[idx]; break;
        }
#endif
    }
    return GrSwizzle(swiz);
}
#endif
