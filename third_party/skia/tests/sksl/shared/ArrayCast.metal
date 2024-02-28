#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    half4 colorGreen;
    half4 colorRed;
};
struct Inputs {
};
struct Outputs {
    half4 sk_FragColor [[color(0)]];
};

template <typename T1, typename T2, size_t N>
bool operator==(thread const array<T1, N>& left, thread const array<T2, N>& right);
template <typename T1, typename T2, size_t N>
bool operator!=(thread const array<T1, N>& left, thread const array<T2, N>& right);

thread bool operator==(const float2x2 left, const float2x2 right);
thread bool operator!=(const float2x2 left, const float2x2 right);

template <size_t N>
array<half, N> array_of_half_from_float(thread const array<float, N>& x) {
    array<half, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = half(x[i]);
    }
    return result;
}

template <size_t N>
array<float, N> array_of_float_from_half(thread const array<half, N>& x) {
    array<float, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = float(x[i]);
    }
    return result;
}

template <size_t N>
array<short3, N> array_of_short3_from_int3(thread const array<int3, N>& x) {
    array<short3, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = short3(x[i]);
    }
    return result;
}

template <size_t N>
array<int3, N> array_of_int3_from_short3(thread const array<short3, N>& x) {
    array<int3, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = int3(x[i]);
    }
    return result;
}

template <size_t N>
array<float2x2, N> array_of_float2x2_from_half2x2(thread const array<half2x2, N>& x) {
    array<float2x2, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = float2x2(x[i]);
    }
    return result;
}

template <size_t N>
array<half2x2, N> array_of_half2x2_from_float2x2(thread const array<float2x2, N>& x) {
    array<half2x2, N> result;
    for (int i = 0; i < N; ++i) {
        result[i] = half2x2(x[i]);
    }
    return result;
}

template <typename T1, typename T2, size_t N>
bool operator==(thread const array<T1, N>& left, thread const array<T2, N>& right) {
    for (size_t index = 0; index < N; ++index) {
        if (!all(left[index] == right[index])) {
            return false;
        }
    }
    return true;
}

template <typename T1, typename T2, size_t N>
bool operator!=(thread const array<T1, N>& left, thread const array<T2, N>& right) {
    return !(left == right);
}
thread bool operator==(const float2x2 left, const float2x2 right) {
    return all(left[0] == right[0]) &&
           all(left[1] == right[1]);
}
thread bool operator!=(const float2x2 left, const float2x2 right) {
    return !(left == right);
}
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    array<float, 4> f = array<float, 4>{1.0, 2.0, 3.0, 4.0};
    array<half, 4> h = array_of_half_from_float(f);
    f = array_of_float_from_half(h);
    h = array_of_half_from_float(f);
    array<int3, 3> i3 = array<int3, 3>{int3(1), int3(2), int3(3)};
    array<short3, 3> s3 = array_of_short3_from_int3(i3);
    i3 = array_of_int3_from_short3(s3);
    s3 = array_of_short3_from_int3(i3);
    array<half2x2, 2> h2x2 = array<half2x2, 2>{half2x2(half2(1.0h, 2.0h), half2(3.0h, 4.0h)), half2x2(half2(5.0h, 6.0h), half2(7.0h, 8.0h))};
    array<float2x2, 2> f2x2 = array_of_float2x2_from_half2x2(h2x2);
    f2x2 = array_of_float2x2_from_half2x2(h2x2);
    h2x2 = array_of_half2x2_from_float2x2(f2x2);
    _out.sk_FragColor = (f == array_of_float_from_half(h) && i3 == array_of_int3_from_short3(s3)) && f2x2 == array_of_float2x2_from_half2x2(h2x2) ? _uniforms.colorGreen : _uniforms.colorRed;
    return _out;
}
