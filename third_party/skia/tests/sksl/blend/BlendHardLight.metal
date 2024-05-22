#include <metal_stdlib>
#include <simd/simd.h>
using namespace metal;
struct Uniforms {
    half4 src;
    half4 dst;
};
struct Inputs {
};
struct Outputs {
    half4 sk_FragColor [[color(0)]];
};
half blend_overlay_component_Qhh2h2(half2 s, half2 d) {
    return 2.0h * d.x <= d.y ? (2.0h * s.x) * d.x : s.y * d.y - (2.0h * (d.y - d.x)) * (s.y - s.x);
}
fragment Outputs fragmentMain(Inputs _in [[stage_in]], constant Uniforms& _uniforms [[buffer(0)]], bool _frontFacing [[front_facing]], float4 _fragCoord [[position]]) {
    Outputs _out;
    (void)_out;
    half4 _0_result = half4(blend_overlay_component_Qhh2h2(_uniforms.dst.xw, _uniforms.src.xw), blend_overlay_component_Qhh2h2(_uniforms.dst.yw, _uniforms.src.yw), blend_overlay_component_Qhh2h2(_uniforms.dst.zw, _uniforms.src.zw), _uniforms.dst.w + (1.0h - _uniforms.dst.w) * _uniforms.src.w);
    _0_result.xyz = _0_result.xyz + _uniforms.src.xyz * (1.0h - _uniforms.dst.w) + _uniforms.dst.xyz * (1.0h - _uniforms.src.w);
    _out.sk_FragColor = _0_result;
    return _out;
}
