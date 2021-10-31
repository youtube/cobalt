#version 100

precision mediump float;
precision mediump sampler2D;
uniform mediump vec4 uColor_Stage0;
uniform mediump vec4 uleftBorderColor_Stage1_c0_c0;
uniform mediump vec4 urightBorderColor_Stage1_c0_c0;
uniform highp vec4 uscale01_Stage1_c0_c0_c1_c0;
uniform highp vec4 ubias01_Stage1_c0_c0_c1_c0;
uniform highp vec4 uscale23_Stage1_c0_c0_c1_c0;
uniform highp vec4 ubias23_Stage1_c0_c0_c1_c0;
uniform mediump float uthreshold_Stage1_c0_c0_c1_c0;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump float vinCoverage_Stage0;
mediump vec4 stage_Stage1_c0_c0_c0_c0(mediump vec4 _input) {
    mediump vec4 _sample1099_c0_c0;
    mediump float t = length(vTransformedCoords_0_Stage0);
    _sample1099_c0_c0 = vec4(t, 1.0, 0.0, 0.0);
    return _sample1099_c0_c0;
}
mediump vec4 stage_Stage1_c0_c0_c1_c0(mediump vec4 _input) {
    mediump vec4 _sample1767_c0_c0;
    mediump float t = _input.x;
    highp vec4 scale, bias;
    if (t < uthreshold_Stage1_c0_c0_c1_c0) {
        scale = uscale01_Stage1_c0_c0_c1_c0;
        bias = ubias01_Stage1_c0_c0_c1_c0;
    } else {
        scale = uscale23_Stage1_c0_c0_c1_c0;
        bias = ubias23_Stage1_c0_c0_c1_c0;
    }
    _sample1767_c0_c0 = t * scale + bias;
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
    {
        _sample1992.xyz *= _sample1992.w;
    }
    return _sample1992;
}
void main() {
    mediump vec4 outputCoverage_Stage0;
    {
        mediump float alpha = 1.0;
        alpha = vinCoverage_Stage0;
        outputCoverage_Stage0 = vec4(alpha);
    }
    mediump vec4 output_Stage1;
    {
        mediump vec4 _sample1992;
        _sample1992 = stage_Stage1_c0_c0(vec4(1.0, 1.0, 1.0, 1.0));
        output_Stage1 = _sample1992;
    }
    {
        gl_FragColor = output_Stage1 * outputCoverage_Stage0;
    }
}
