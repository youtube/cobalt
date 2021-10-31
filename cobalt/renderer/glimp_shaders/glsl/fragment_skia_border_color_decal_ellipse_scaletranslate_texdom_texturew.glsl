#version 100

uniform highp float u_skRTHeight;
precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uleftBorderColor_Stage1_c0_c0;
uniform mediump vec4 urightBorderColor_Stage1_c0_c0;
uniform mediump vec4 ustart_Stage1_c0_c0_c1_c0;
uniform mediump vec4 uend_Stage1_c0_c0_c1_c0;
uniform mediump vec4 uscaleAndTranslate_Stage2;
uniform mediump vec4 uTexDom_Stage2;
uniform mediump vec3 uDecalParams_Stage2;
uniform sampler2D uTextureSampler_0_Stage2;
varying highp vec3 vEllipseOffsets_Stage0;
varying highp vec4 vEllipseRadii_Stage0;
varying mediump vec4 vinColor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
mediump vec4 stage_Stage1_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1099_c0_c0;
    mediump float t = length(vTransformedCoords_0_Stage0);
    _sample1099_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample1099_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1767_c0_c0;
    mediump float t = _input.x;
    _sample1767_c0_c0 = (1.0 - t) * ustart_Stage1_c0_c0_c1_c0 + t * uend_Stage1_c0_c0_c1_c0;
    return _sample1767_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1992;
    mediump vec4 _sample1099_c0_c0;
    _sample1099_c0_c0 = stage_Stage1_c0_c0_c0_c0(vec4(1.0));
    mediump vec4 t = _sample1099_c0_c0;
    if (t.x < 0.0) {
        _sample1992 = uleftBorderColor_Stage1_c0_c0;
    } else if (t.x > 1.0) {
        _sample1992 = urightBorderColor_Stage1_c0_c0;
    } else {
        mediump vec4 _sample1767_c0_c0;
        _sample1767_c0_c0 = stage_Stage1_c0_c0_c1_c0(t);
        _sample1992 = _sample1767_c0_c0;
    }
    return _sample1992;
}
void main() {
highp     vec4 sk_FragCoord = vec4(gl_FragCoord.x, u_skRTHeight - gl_FragCoord.y, gl_FragCoord.z, gl_FragCoord.w);
    mediump vec4 outputCoverage_Stage0;
    {
        highp vec2 offset = vEllipseOffsets_Stage0.xy;
        highp float test = dot(offset, offset) - 1.0;
        highp vec2 grad = (2.0 * offset) * (vEllipseOffsets_Stage0.z * vEllipseRadii_Stage0.xy);
        highp float grad_dot = dot(grad, grad);
        grad_dot = max(grad_dot, 6.103600026108325e-05);
        highp float invlen = vEllipseOffsets_Stage0.z * inversesqrt(grad_dot);
        highp float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 _sample1992;
        _sample1992 = stage_Stage1_c0_c0(vec4(1.0, 1.0, 1.0, 1.0));
        output_Stage1 = _sample1992;
    }
    mediump vec4 output_Stage2;
    {
        mediump vec2 coords = sk_FragCoord.xy * uscaleAndTranslate_Stage2.xy + uscaleAndTranslate_Stage2.zw;
        {
            highp vec2 origCoord = coords;
            highp vec2 clampedCoord = clamp(origCoord, uTexDom_Stage2.xy, uTexDom_Stage2.zw);
            mediump vec4 inside = texture2D(uTextureSampler_0_Stage2, clampedCoord).wwww * outputCoverage_Stage0;
            mediump float err = max(abs(clampedCoord.x - origCoord.x) * uDecalParams_Stage2.x, abs(clampedCoord.y - origCoord.y) * uDecalParams_Stage2.y);
            if (err > uDecalParams_Stage2.z) {
                err = 1.0;
            } else if (uDecalParams_Stage2.z < 1.0) {
                err = 0.0;
            }
            output_Stage2 = mix(inside, vec4(0.0, 0.0, 0.0, 0.0), err);
        }
    }
    {
        gl_FragColor = output_Stage1 * output_Stage2;
    }
}
